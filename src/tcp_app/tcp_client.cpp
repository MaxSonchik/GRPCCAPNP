// benchmark/tcp_app/tcp_client.cpp

#include "config.hpp"
#include "file_utils.hpp"
#include "reversal_utils.hpp"
#include "metrics_aggregator.hpp"
#include "include/tcp_messaging.hpp"
#include <boost/asio.hpp>
#include <iostream>
#include <vector>
#include <memory> // For std::enable_shared_from_this, std::shared_ptr
#include <array>
#include <string>
#include <filesystem> // Для std::filesystem

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
          m_host(host), // Сохраняем host
          m_port_str(std::to_string(port)), // Сохраняем port как строку
          m_chunk_reader(config::TEST_FILE_NAME, config::CHUNK_SIZE)
           {
        m_total_chunks_to_send = m_chunk_reader.total_chunks();
        // Корректировка m_total_chunks_to_send для файлов размером < CHUNK_SIZE
        if (m_chunk_reader.file_size() > 0 && m_total_chunks_to_send == 0) {
            m_total_chunks_to_send = 1;
        } else if (m_chunk_reader.file_size() == 0) {
             m_total_chunks_to_send = 0;
        }
        // Не вызываем shared_from_this() или асинхронные операции в конструкторе
        std::cout << "TCPClient object created. Call start() to connect and process." << std::endl;
    }

    // Метод для запуска асинхронных операций
    void start() {
        // Теперь вызов shared_from_this() здесь безопасен
        auto self = shared_from_this();
        m_resolver.async_resolve(m_host, m_port_str,
            [this, self](const boost::system::error_code& ec, tcp::resolver::results_type endpoints) {
                if (!ec) {
                    do_connect(endpoints);
                } else {
                    std::cerr << "TCP Client: Resolve error: " << ec.message() << std::endl;
                    // Остановка io_context, если разрешение не удалось
                    if (!m_io_context.stopped()) m_io_context.stop();
                }
            });
    }

private:
    void do_connect(const tcp::resolver::results_type& endpoints) {
        auto self = shared_from_this();
        boost::asio::async_connect(m_socket, endpoints,
            [this, self](const boost::system::error_code& ec, const tcp::endpoint& /*endpoint*/) {
                if (!ec) {
                    std::cout << "TCP Client: Connected to "
                              << m_socket.remote_endpoint().address().to_string()
                              << ":" << m_socket.remote_endpoint().port() << std::endl;
                    m_metrics.start_timer(); // Таймер запускается после успешного соединения
                    send_next_chunk();
                } else {
                    std::cerr << "TCP Client: Connect error: " << ec.message() << std::endl;
                    if (!m_io_context.stopped()) m_io_context.stop();
                }
            });
    }

    void send_next_chunk() {
        // Проверка, если все чанки отправлены или конец файла
        if (m_chunks_sent >= m_total_chunks_to_send || m_chunk_reader.eof()) {
            if (m_chunk_reader.file_size() == 0 && m_chunks_sent == 0) {
                 std::cout << "TCP Client: Test file is empty. Nothing to send." << std::endl;
            } else {
                std::cout << "TCP Client: All " << m_chunks_sent << " intended chunks processed or EOF reached." << std::endl;
            }
            if (!m_timer_stopped_flag) {
                 m_metrics.stop_timer();
                 m_timer_stopped_flag = true;
            }
            // Закрываем сокет и останавливаем io_context, так как работа клиента завершена
            boost::system::error_code ignored_ec;
            m_socket.shutdown(tcp::socket::shutdown_both, ignored_ec);
            m_socket.close(ignored_ec);
            if (!m_io_context.stopped()) m_io_context.stop();
            return;
        }

        m_current_chunk_data = m_chunk_reader.read_next_chunk();

        // Если read_next_chunk вернул пустой вектор, но m_chunk_reader.eof() еще false,
        // это может быть ошибкой чтения или концом файла, который не был правильно обработан.
        // Флаг m_chunk_reader.eof() должен быть главным индикатором.
        if (m_current_chunk_data.empty()) {
            if (m_chunk_reader.eof()){
                 std::cout << "TCP Client: EOF confirmed by ChunkReader, all data sent. Total chunks: " << m_chunks_sent << std::endl;
            } else {
                 std::cerr << "TCP Client: Read empty chunk unexpectedly before EOF. Aborting." << std::endl;
                 m_metrics.record_chunk_verified(false); // Считаем это ошибкой
            }
            if (!m_timer_stopped_flag) {
                 m_metrics.stop_timer();
                 m_timer_stopped_flag = true;
            }
            boost::system::error_code ignored_ec;
            m_socket.shutdown(tcp::socket::shutdown_both, ignored_ec);
            m_socket.close(ignored_ec);
            if (!m_io_context.stopped()) m_io_context.stop();
            return;
        }

        m_expected_reversed_chunk = utils::get_reversed_vector_content(m_current_chunk_data);

        auto self = shared_from_this();
        auto buffers_to_send = tcp_messaging::prepare_message(m_current_chunk_data, m_write_header_buffer);

        boost::asio::async_write(m_socket, buffers_to_send,
            [this, self](const boost::system::error_code& ec, std::size_t /*bytes_transferred*/) {
                if (!ec) {
                    // std::cout << "TCP Client: Sent chunk " << (m_chunks_sent + 1) << "/" << m_total_chunks_to_send << std::endl;
                    do_read_header();
                } else {
                    std::cerr << "TCP Client: Write error: " << ec.message() << std::endl;
                    if (!m_timer_stopped_flag) { m_metrics.stop_timer(); m_timer_stopped_flag = true; }
                    if (!m_io_context.stopped()) m_io_context.stop();
                }
            });
    }

    void do_read_header() {
        auto self = shared_from_this();
        tcp_messaging::async_read_header(m_socket, m_read_header_buffer,
            [this, self](const boost::system::error_code& ec) {
                if (!ec) {
                    uint32_t body_length = tcp_messaging::parse_header(m_read_header_buffer);
                     if (body_length > config::CHUNK_SIZE * 2) { // Sanity check
                         std::cerr << "TCP Client: Excessive body length in response: " << body_length << ". Closing." << std::endl;
                         if (!m_timer_stopped_flag) { m_metrics.stop_timer(); m_timer_stopped_flag = true; }
                         if (!m_io_context.stopped()) m_io_context.stop();
                         return;
                    }
                    m_read_body_buffer.resize(body_length);
                    if (body_length == 0) { // Сервер ответил пустым телом
                        if (m_current_chunk_data.empty()) { // Если мы и отправляли пустой чанк (не должно быть по логике ChunkReader)
                             handle_received_chunk(); // Обрабатываем как есть
                        } else {
                            // Отправили данные, получили 0 в ответ - это может быть ошибка протокола или специальный сигнал
                            std::cerr << "TCP Client: Received 0-length body for non-empty sent chunk. Verification will likely fail." << std::endl;
                            // Продолжаем к do_read_body, который прочитает 0 байт, затем верификация
                            do_read_body(body_length);
                        }
                    } else {
                        do_read_body(body_length);
                    }
                } else {
                    std::cerr << "TCP Client: Read header error: " << ec.message() << std::endl;
                    if (ec == boost::asio::error::eof && m_chunks_sent >= m_total_chunks_to_send) {
                        std::cout << "TCP Client: Server closed connection after all chunks processed, as expected." << std::endl;
                    }
                    if (!m_timer_stopped_flag) { m_metrics.stop_timer(); m_timer_stopped_flag = true; }
                    if (!m_io_context.stopped()) m_io_context.stop();
                }
            });
    }

    void do_read_body(uint32_t body_length_expected) {
        auto self = shared_from_this();
        if (body_length_expected == 0) { // Если ожидается 0 байт, async_read не нужен
            handle_received_chunk(); // m_read_body_buffer уже должен быть пуст (т.к. resize(0) в do_read_header)
            return;
        }
        boost::asio::async_read(m_socket, boost::asio::buffer(m_read_body_buffer.data(), m_read_body_buffer.size()),
            [this, self](const boost::system::error_code& ec, std::size_t /*length_read*/) {
                if (!ec) {
                    handle_received_chunk();
                } else {
                    std::cerr << "TCP Client: Read body error: " << ec.message() << std::endl;
                    if (!m_timer_stopped_flag) { m_metrics.stop_timer(); m_timer_stopped_flag = true; }
                    if (!m_io_context.stopped()) m_io_context.stop();
                }
            });
    }

    void handle_received_chunk() {
        bool verified = (m_read_body_buffer == m_expected_reversed_chunk);
        m_metrics.record_chunk_processed(m_current_chunk_data.size());
        m_metrics.record_chunk_verified(verified);

        if (!verified) {
            std::cerr << "TCP Client: ERROR! Chunk " << (m_chunks_sent + 1)
                      << " (original size: " << m_current_chunk_data.size()
                      << ", received size: " << m_read_body_buffer.size()
                      << ") verification FAILED." << std::endl;
            // Остановка на первой ошибке
            if (!m_timer_stopped_flag) { m_metrics.stop_timer(); m_timer_stopped_flag = true; }
            if (!m_io_context.stopped()) m_io_context.stop();
            return;
        }

        m_chunks_sent++;
        // std::cout << "TCP Client: Chunk " << m_chunks_sent << " verified OK." << std::endl;


        // Логика завершения или продолжения
        if (m_chunks_sent >= m_total_chunks_to_send || m_chunk_reader.eof()) {
            // Эта проверка дублируется в send_next_chunk, но здесь как подтверждение после получения ответа
            std::cout << "TCP Client: Successfully processed all " << m_chunks_sent << " chunks." << std::endl;
            if (!m_timer_stopped_flag) {
                m_metrics.stop_timer();
                m_timer_stopped_flag = true;
            }
            // io_context остановится сам, так как больше нет pending async operations,
            // либо явно остановить, если хотим быть уверены.
            // Закрытие сокета здесь также хороший тон.
            boost::system::error_code ignored_ec;
            m_socket.shutdown(tcp::socket::shutdown_both, ignored_ec);
            m_socket.close(ignored_ec);
            if (!m_io_context.stopped()) m_io_context.stop();
        } else {
            send_next_chunk(); // Отправляем следующий чанк
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
    bool m_timer_stopped_flag = false;
};


int main(int argc, char* argv[]) {
    try {
        std::string server_ip = config::DEFAULT_SERVER_IP;
        if (argc > 1) {
            server_ip = argv[1];
            std::cout << "TCP Client: Using server IP from argument: " << server_ip << std::endl;
        } else {
            std::cout << "TCP Client: Using default server IP: " << server_ip << " (localhost)" << std::endl;
            std::cout << "TCP Client: You can specify server IP as a command line argument." << std::endl;
        }

        generate_test_file_if_not_exists(config::TEST_FILE_NAME, config::TOTAL_FILE_SIZE);

        if (!std::filesystem::exists(config::TEST_FILE_NAME) ||
            std::filesystem::file_size(config::TEST_FILE_NAME) != config::TOTAL_FILE_SIZE) {
            std::cerr << "TCP Client: Test file '" << config::TEST_FILE_NAME
                      << "' could not be created or has incorrect size. Aborting." << std::endl;
            return 1;
        }

        boost::asio::io_context io_context;
        MetricsAggregator metrics("TCP", config::TOTAL_FILE_SIZE, config::CHUNK_SIZE);

        auto client = std::make_shared<TCPClient>(io_context, server_ip, config::TCP_SERVER_PORT, metrics);
        client->start(); // Запускаем операции клиента

        io_context.run(); // Запускаем цикл событий Asio

        // io_context.run() завершится, когда не останется работы или будет вызван stop()
        std::cout << "TCP Client: io_context.run() finished." << std::endl;

        metrics.print_summary();
        metrics.save_to_csv(config::CSV_RESULTS_FILE);

    } catch (const std::exception& e) {
        // Ловим std::bad_weak_ptr здесь, если он все еще возникает
        if (dynamic_cast<const std::bad_weak_ptr*>(&e)) {
             std::cerr << "TCP Client Exception: Caught std::bad_weak_ptr in main. This indicates an issue with object lifetime." << std::endl;
        } else {
            std::cerr << "TCP Client Exception: " << e.what() << std::endl;
        }
        return 1;
    }
    return 0;
}
