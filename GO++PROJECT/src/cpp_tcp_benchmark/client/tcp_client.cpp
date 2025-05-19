// benchmark/client/tcp_client.cpp
#include "../include/config.hpp"
#include "../common/include/chunk_reader.hpp"      // Adjusted path
#include "../common/include/reversal_utils.hpp"    // Adjusted path
#include "../common/include/metrics_aggregator.hpp"// Adjusted path
#include "../common/include/tcp_messaging.hpp"     // Adjusted path
#include "../common/include/file_utils.hpp"        // For generate_test_file_if_not_exists

#include <boost/asio.hpp>
#include <iostream>
#include <vector>
#include <memory>
#include <array>
#include <string>
#include <filesystem> // Для std::filesystem

namespace fs = std::filesystem;
using boost::asio::ip::tcp;

class TCPClient : public std::enable_shared_from_this<TCPClient> {
public:
    TCPClient(boost::asio::io_context& io_context,
              const std::string& host,
              unsigned short port,
              MetricsAggregator& metrics)
        : m_io_context(io_context),
          m_socket(io_context),
          m_resolver(io_context),
          m_metrics(metrics),
          m_host(host),
          m_port_str(std::to_string(port)),
          m_chunk_reader(config::TEST_FILE_NAME, config::CHUNK_SIZE)
           {
        m_total_chunks_to_send = m_chunk_reader.total_chunks();
        if (m_chunk_reader.file_size() > 0 && m_total_chunks_to_send == 0 && m_chunk_reader.chunks_read() == 0) { // File < chunk_size
             m_total_chunks_to_send = 1;
        } else if (m_chunk_reader.file_size() == 0) {
             m_total_chunks_to_send = 0;
        }
        std::cout << "TCPClient: Total chunks to send: " << m_total_chunks_to_send << " (from file size: " << m_chunk_reader.file_size() << ")" << std::endl;
    }

    void start() {
        auto self = shared_from_this(); // Ok here, as it's not in constructor body
        m_resolver.async_resolve(m_host, m_port_str,
            [this, self](const boost::system::error_code& ec, tcp::resolver::results_type endpoints) {
                if (!ec) {
                    do_connect(endpoints);
                } else {
                    std::cerr << "TCP Client: Resolve error: " << ec.message() << std::endl;
                    stop_client_operations(true); // Force stop
                }
            });
    }

private:
    void stop_client_operations(bool error_occurred) {
        if (!m_operations_stopped) {
            m_operations_stopped = true;
            if (!m_timer_stopped_flag) {
                 m_metrics.stop_timer();
                 m_timer_stopped_flag = true;
            }
            boost::system::error_code ignored_ec;
            if (m_socket.is_open()){
                m_socket.shutdown(tcp::socket::shutdown_both, ignored_ec);
                m_socket.close(ignored_ec);
            }
            if (!m_io_context.stopped()) {
                 // std::cout << "TCP Client: Stopping io_context explicitly." << std::endl;
                 m_io_context.stop();
            }
            if (error_occurred) {
                std::cout << "TCP Client: Operations stopped due to an error." << std::endl;
            } else {
                std::cout << "TCP Client: Operations finished successfully." << std::endl;
            }
        }
    }

    void do_connect(const tcp::resolver::results_type& endpoints) {
        auto self = shared_from_this();
        boost::asio::async_connect(m_socket, endpoints,
            [this, self](const boost::system::error_code& ec, const tcp::endpoint& /*endpoint*/) {
                if (!ec) {
                    std::cout << "TCP Client: Connected to "
                              << m_socket.remote_endpoint().address().to_string()
                              << ":" << m_socket.remote_endpoint().port() << std::endl;
                    m_metrics.start_timer();
                    send_next_chunk();
                } else {
                    std::cerr << "TCP Client: Connect error: " << ec.message() << std::endl;
                    stop_client_operations(true);
                }
            });
    }

    void send_next_chunk() {
        if (m_operations_stopped) return;

        if (m_chunks_sent >= m_total_chunks_to_send || m_chunk_reader.eof()) {
             if (m_chunk_reader.file_size() == 0 && m_chunks_sent == 0) {
                 std::cout << "TCP Client: Test file is empty. Nothing to send." << std::endl;
             } else {
                 std::cout << "TCP Client: All " << m_chunks_sent
                           << " chunks processed or EOF reached (ChunkReader EOF: " << m_chunk_reader.eof()
                           << ", Total to send: " << m_total_chunks_to_send
                           << "). Chunks read by reader: " << m_chunk_reader.chunks_read() << std::endl;
             }
             stop_client_operations(false); // Normal completion
             return;
        }

        m_current_chunk_data = m_chunk_reader.read_next_chunk();

        if (m_current_chunk_data.empty() && !m_chunk_reader.eof()) {
            std::cerr << "TCP Client: Read empty chunk unexpectedly before EOF (chunks_sent: " << m_chunks_sent << "). Aborting." << std::endl;
            m_metrics.record_chunk_verified(false);
            stop_client_operations(true);
            return;
        }
        if (m_current_chunk_data.empty() && m_chunk_reader.eof()) { // Legitimate end if last read was partial
            std::cout << "TCP Client: Reached true EOF after reading last chunk. Processed " << m_chunks_sent << " chunks." << std::endl;
            stop_client_operations(false);
            return;
        }


        m_expected_reversed_chunk = utils::get_reversed_vector_content(m_current_chunk_data);
        m_metrics.start_chunk_rtt_timer(); // Start RTT timer for this chunk

        auto self = shared_from_this();
        auto buffers_to_send = tcp_messaging::prepare_message(m_current_chunk_data, m_write_header_buffer);

        boost::asio::async_write(m_socket, buffers_to_send,
            [this, self](const boost::system::error_code& ec, std::size_t bytes_transferred) {
                if (m_operations_stopped) return;
                if (!ec) {
                    // std::cout << "TCP Client: Sent chunk " << (m_chunks_sent + 1) << "/" << m_total_chunks_to_send
                    //           << " (" << bytes_transferred - tcp_messaging::HEADER_SIZE << " payload bytes)" << std::endl;
                    do_read_header();
                } else {
                    std::cerr << "TCP Client: Write error: " << ec.message() << std::endl;
                    stop_client_operations(true);
                }
            });
    }

    void do_read_header() {
        if (m_operations_stopped) return;
        auto self = shared_from_this();
        tcp_messaging::async_read_header(m_socket, m_read_header_buffer,
            [this, self](const boost::system::error_code& ec, std::size_t /*length: this lambda variant doesn't receive length*/) {
                if (m_operations_stopped) return;
                if (!ec) {
                    uint32_t body_length = tcp_messaging::parse_header(m_read_header_buffer);
                     if (body_length > config::CHUNK_SIZE * 2) {
                         std::cerr << "TCP Client: Excessive body length in response: " << body_length << ". Max expected: " << config::CHUNK_SIZE << ". Closing." << std::endl;
                         stop_client_operations(true);
                         return;
                    }
                    m_read_body_buffer.resize(body_length); // Prepare buffer for body
                    if (body_length == 0) { // Server responded with empty body
                        // std::cout << "TCP Client: Received header for 0-length body." << std::endl;
                        // This might be valid if an empty chunk was sent and reversed (still empty)
                        // Or it might be an error if a non-empty chunk was sent.
                        // Verification will handle it.
                        handle_received_chunk_data(); // Proceed to handle (empty) data
                    } else {
                        do_read_body();
                    }
                } else {
                    if (ec == boost::asio::error::eof) {
                        std::cout << "TCP Client: Server closed connection while reading header." << std::endl;
                         // If we've sent all chunks and server closes, it might be okay.
                        if (m_chunks_sent >= m_total_chunks_to_send) {
                             std::cout << "TCP Client: EOF from server, assuming all data processed." << std::endl;
                             stop_client_operations(false);
                        } else {
                             std::cerr << "TCP Client: EOF from server before all data processed. Chunks sent: " << m_chunks_sent << "/" << m_total_chunks_to_send << std::endl;
                             stop_client_operations(true);
                        }
                    } else {
                        std::cerr << "TCP Client: Read header error: " << ec.message() << std::endl;
                        stop_client_operations(true);
                    }
                }
            });
    }

    void do_read_body() { // body_length_expected is m_read_body_buffer.size()
        if (m_operations_stopped) return;
        auto self = shared_from_this();
        boost::asio::async_read(m_socket, boost::asio::buffer(m_read_body_buffer.data(), m_read_body_buffer.size()),
            boost::asio::transfer_exactly(m_read_body_buffer.size()), // Ensure all expected bytes are read
            [this, self](const boost::system::error_code& ec, std::size_t length_read) {
                if (m_operations_stopped) return;
                if (!ec) {
                    if (length_read != m_read_body_buffer.size()) {
                        std::cerr << "TCP Client: Read body error: Incomplete read. Expected "
                                  << m_read_body_buffer.size() << ", got " << length_read << std::endl;
                        stop_client_operations(true);
                        return;
                    }
                    handle_received_chunk_data();
                } else {
                     if (ec == boost::asio::error::eof) {
                         std::cerr << "TCP Client: Server closed connection while reading body. Expected "
                                   << m_read_body_buffer.size() << " bytes." << std::endl;
                     } else {
                        std::cerr << "TCP Client: Read body error: " << ec.message() << std::endl;
                     }
                    stop_client_operations(true);
                }
            });
    }

    void handle_received_chunk_data() {
        if (m_operations_stopped) return;

        bool verified = (m_read_body_buffer == m_expected_reversed_chunk);
        m_metrics.stop_and_record_chunk_rtt(m_current_chunk_data.size(), verified); // Stop RTT timer and record

        if (!verified) {
            std::cerr << "TCP Client: ERROR! Chunk " << (m_chunks_sent + 1)
                      << " (original size: " << m_current_chunk_data.size()
                      << ", received size: " << m_read_body_buffer.size()
                      << ") verification FAILED." << std::endl;
            // You might want to log the differing chunks here for debugging
            stop_client_operations(true); // Stop on first error
            return;
        }

        m_chunks_sent++;
        // std::cout << "TCP Client: Chunk " << m_chunks_sent << " verified OK. (" << m_read_body_buffer.size() << " bytes)" << std::endl;

        if (m_chunks_sent >= m_total_chunks_to_send || m_chunk_reader.eof()) {
             std::cout << "TCP Client: Successfully processed all " << m_chunks_sent << " chunks." << std::endl;
             stop_client_operations(false); // Normal completion
        } else {
            send_next_chunk(); // Send the next chunk
        }
    }

    boost::asio::io_context& m_io_context;
    tcp::socket m_socket;
    tcp::resolver m_resolver;
    MetricsAggregator& m_metrics;

    std::string m_host;
    std::string m_port_str;

    ChunkReader m_chunk_reader;

    std::vector<char> m_current_chunk_data;
    std::vector<char> m_expected_reversed_chunk;

    std::array<char, tcp_messaging::HEADER_SIZE> m_write_header_buffer;
    std::array<char, tcp_messaging::HEADER_SIZE> m_read_header_buffer;
    std::vector<char> m_read_body_buffer;

    std::size_t m_chunks_sent = 0;
    std::size_t m_total_chunks_to_send = 0;
    bool m_timer_stopped_flag = false; // For the main timer
    bool m_operations_stopped = false; // General flag to halt operations
};


int main(int argc, char* argv[]) {
    try {
        std::string server_ip = config::DEFAULT_SERVER_IP;
        if (argc > 1) {
            server_ip = argv[1];
            std::cout << "TCP Client: Using server IP from argument: " << server_ip << std::endl;
        } else {
            std::cout << "TCP Client: Using default server IP: " << server_ip << std::endl;
        }
         std::cout << "TCP Client: Target file size: " << config::TOTAL_FILE_SIZE / (1024.0*1024.0) << " MB, Chunk size: " << config::CHUNK_SIZE / 1024.0 << " KB." << std::endl;


        generate_test_file_if_not_exists(config::TEST_FILE_NAME, config::TOTAL_FILE_SIZE);

        if (!fs::exists(config::TEST_FILE_NAME)) { // Check again after generation attempt
            std::cerr << "TCP Client: Test file '" << config::TEST_FILE_NAME
                      << "' could not be created or found. Aborting." << std::endl;
            return 1;
        }
        if (fs::file_size(config::TEST_FILE_NAME) != config::TOTAL_FILE_SIZE) {
             std::cerr << "TCP Client: Test file '" << config::TEST_FILE_NAME
                      << "' has incorrect size. Expected " << config::TOTAL_FILE_SIZE
                      << ", got " << fs::file_size(config::TEST_FILE_NAME) << ". Aborting." << std::endl;
            return 1;
        }


        boost::asio::io_context io_context;
        MetricsAggregator metrics("CPP_TCP", config::TOTAL_FILE_SIZE, config::CHUNK_SIZE);

        auto client = std::make_shared<TCPClient>(io_context, server_ip, config::TCP_SERVER_PORT, metrics);
        client->start();

        io_context.run();

        std::cout << "TCP Client: io_context.run() finished." << std::endl;

        metrics.print_summary();
        metrics.save_to_csv(config::CPP_OVERALL_METRICS_FILE, config::CPP_CHUNK_RTT_METRICS_FILE);

    } catch (const std::exception& e) {
        std::cerr << "TCP Client Exception in main: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
