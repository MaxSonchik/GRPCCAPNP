// benchmark/server/tcp_server.cpp
#include "config.hpp"
#include "reversal_utils.hpp"
#include "tcp_messaging.hpp"

#include <array>
#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <vector>

using boost::asio::ip::tcp;

class TCPSession : public std::enable_shared_from_this<TCPSession> {
public:
  TCPSession(tcp::socket socket) : m_socket(std::move(socket)) {
    std::cout << "TCP Session: New connection from "
              << m_socket.remote_endpoint().address().to_string() << ":"
              << m_socket.remote_endpoint().port() << std::endl;
  }

  ~TCPSession() {
     std::cout << "TCP Session: Connection closed with "
               << m_socket.remote_endpoint().address().to_string()
               << std::endl;
  }

  void start() { do_read_header(); }

private:
  void do_read_header() {
    auto self = shared_from_this();
    tcp_messaging::async_read_header(
        m_socket, m_read_header_buffer,
        [this,
         self](const boost::system::error_code &ec,
               std::size_t /*length - not provided by this handler variant */) {
          if (!ec) {
            uint32_t body_length =
                tcp_messaging::parse_header(m_read_header_buffer);
            std::cout << "TCP Session: Received header for body of length: "
            << body_length << std::endl;

            if (body_length ==
                0) {
              m_read_body_buffer.clear();
              do_write();
              return;
            }

            if (body_length > config::CHUNK_SIZE * 2) {
              std::cerr << "TCP Session: Excessive body length received: "
                        << body_length
                        << ". Max expected around: " << config::CHUNK_SIZE
                        << ". Closing session." << std::endl;

              return;
            }
            m_read_body_buffer.resize(body_length);
            do_read_body();
          } else {
            if (ec == boost::asio::error::eof) {
              std::cout << "TCP Session: Client disconnected gracefully (EOF "
                           "on header read)."
                        << std::endl;
            } else if (ec == boost::asio::error::operation_aborted) {
              std::cout
                  << "TCP Session: Operation aborted (likely server shutdown)."
                  << std::endl;
            } else {
              std::cerr << "TCP Session: Error reading header: " << ec.message()
                        << std::endl;
            }

          }
        });
  }

  void do_read_body() {
    auto self = shared_from_this();
    boost::asio::async_read(
        m_socket,
        boost::asio::buffer(m_read_body_buffer.data(),
                            m_read_body_buffer.size()),
        boost::asio::transfer_exactly(
            m_read_body_buffer.size()),
        [this, self](const boost::system::error_code &ec,
                     std::size_t length_read) {
          if (!ec) {
            std::cout << "TCP Session: Read body of size " << length_read <<
            std::endl;
            if (length_read != m_read_body_buffer.size()) {
              std::cerr << "TCP Session: Incomplete body read. Expected "
                        << m_read_body_buffer.size() << ", got " << length_read
                        << ". Closing." << std::endl;
              return;
            }

            utils::reverse_vector_content(m_read_body_buffer);
            do_write();
          } else {
            if (ec == boost::asio::error::eof) {
              std::cout
                  << "TCP Session: Client disconnected while reading body."
                  << std::endl;
            } else if (ec == boost::asio::error::operation_aborted) {
              std::cout << "TCP Session: Operation aborted (likely server "
                           "shutdown) while reading body."
                        << std::endl;
            } else {
              std::cerr << "TCP Session: Error reading body: " << ec.message()
                        << std::endl;
            }
          }
        });
  }

  void do_write() {
    auto self = shared_from_this();

    auto buffers_to_send = tcp_messaging::prepare_message(
        m_read_body_buffer, m_write_header_buffer);

    boost::asio::async_write(
        m_socket, buffers_to_send,
        [this, self](const boost::system::error_code &ec,
                     std::size_t bytes_transferred) {
          if (!ec) {
             std::cout << "TCP Session: Wrote response of "
                       << (bytes_transferred - tcp_messaging::HEADER_SIZE) <<
                       " payload bytes." << std::endl;
            do_read_header(); // Ready for the next message from this client
          } else {
            if (ec == boost::asio::error::operation_aborted) {
              std::cout << "TCP Session: Operation aborted (likely server "
                           "shutdown) while writing."
                        << std::endl;
            } else {
              std::cerr << "TCP Session: Error writing response: "
                        << ec.message() << std::endl;
            }
          }
        });
  }

  tcp::socket m_socket;
  std::array<char, tcp_messaging::HEADER_SIZE> m_read_header_buffer;
  std::vector<char> m_read_body_buffer;
  std::array<char, tcp_messaging::HEADER_SIZE> m_write_header_buffer;
};

class TCPServer {
public:
  TCPServer(boost::asio::io_context &io_context, unsigned short port)
      : m_io_context(io_context),
        m_acceptor(io_context, tcp::endpoint(tcp::v4(), port)) {
    std::cout << "TCP Server listening on port " << port << std::endl;
    do_accept();
  }

private:
  void do_accept() {
    m_acceptor.async_accept([this](const boost::system::error_code &ec,
                                   tcp::socket socket) {
      if (!ec) {
        std::make_shared<TCPSession>(std::move(socket))->start();
      } else {
        if (ec == boost::asio::error::operation_aborted) {
          std::cout
              << "TCP Server: Accept operation aborted (server shutting down?)."
              << std::endl;
          return;
        }
        std::cerr << "TCP Server: Accept error: " << ec.message() << std::endl;
      }
      if (m_acceptor.is_open()) {
        do_accept();
      } else {
        std::cout
            << "TCP Server: Acceptor is closed. Not accepting new connections."
            << std::endl;
      }
    });
  }

  boost::asio::io_context &m_io_context;
  tcp::acceptor m_acceptor;
};

int main() {
  try {
    boost::asio::io_context io_context;
    TCPServer server(io_context, config::TCP_SERVER_PORT);


    boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
    signals.async_wait([&](const boost::system::error_code & ,
                           int ) {
      std::cout << "TCP Server: Shutdown signal received. Stopping io_context."
                << std::endl;
      if (!io_context.stopped()) {
        io_context.stop();
      }
    });

    io_context.run();
    std::cout << "TCP Server: io_context.run() finished. Server has shut down."
              << std::endl;

  } catch (const std::exception &e) {
    std::cerr << "TCP Server Exception in main: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
