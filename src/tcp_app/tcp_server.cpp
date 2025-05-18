#include "config.hpp"
#include "reversal_utils.hpp"
#include "tcp_messaging.hpp"
#include <boost/asio.hpp>
#include <iostream>
#include <vector>
#include <memory> // For std::enable_shared_from_this, std::shared_ptr
#include <array>

using boost::asio::ip::tcp;

class TCPSession : public std::enable_shared_from_this<TCPSession> {
public:
    TCPSession(tcp::socket socket) : m_socket(std::move(socket)) {
         std::cout << "TCP Session created with "
                  << m_socket.remote_endpoint().address().to_string()
                  << ":" << m_socket.remote_endpoint().port() << std::endl;
    }

    void start() {
        do_read_header();
    }

private:
    void do_read_header() {
        auto self = shared_from_this();
        tcp_messaging::async_read_header(m_socket, m_read_header_buffer,
            [this, self](const boost::system::error_code& ec) {
                if (!ec) {
                    uint32_t body_length = tcp_messaging::parse_header(m_read_header_buffer);
                    if (body_length == 0) { // Could be a signal to close or just an empty message
                        std::cout << "TCP Session: Received header for 0 length body. Finishing or expecting more." << std::endl;
                        // Decide if this means close or expect another header
                        // For now, let's assume it's valid and we try to read another header if client sends it
                         do_read_header(); // Or close if this is a termination signal
                        return;
                    }
                    if (body_length > config::CHUNK_SIZE * 2) { // Sanity check
                         std::cerr << "TCP Session: Excessive body length received: " << body_length << ". Closing." << std::endl;
                         return; // Close socket by letting shared_ptr go out of scope
                    }
                    m_read_body_buffer.resize(body_length);
                    do_read_body(body_length);
                } else {
                    if (ec == boost::asio::error::eof) {
                        std::cout << "TCP Session: Client disconnected gracefully." << std::endl;
                    } else {
                        std::cerr << "TCP Session: Error reading header: " << ec.message() << std::endl;
                    }
                    // Connection closed or error, session ends.
                }
            });
    }

    void do_read_body(uint32_t /*body_length_expected*/) {
        auto self = shared_from_this();
        // Используем напрямую boost::asio::async_read
        boost::asio::async_read(m_socket, boost::asio::buffer(m_read_body_buffer.data(), m_read_body_buffer.size()),
            [this, self](const boost::system::error_code& ec, std::size_t length_read) {
                if (!ec) {
                    // std::cout << "TCP Session: Read body of size " << length_read << std::endl;
                    // Убедимся, что m_read_body_buffer имеет правильный размер после чтения, если length_read < m_read_body_buffer.size()
                    // Хотя async_read должен был прочитать ровно m_read_body_buffer.size() байт или выдать ошибку (например, EOF)
                    // Если вдруг он прочитал меньше без ошибки, то это нужно учесть.
                    // Но для нашего случая, где мы передаем размер буфера, он должен заполнить его или вернуть ошибку.
                    // Если размер не соответствует ожидаемому, это может быть проблемой.
                    // Для простоты пока предполагаем, что length_read == m_read_body_buffer.size() при успехе.

                    utils::reverse_vector_content(m_read_body_buffer); // m_read_body_buffer уже содержит прочитанные данные
                    do_write();
                } else {
                     if (ec == boost::asio::error::eof) {
                        std::cout << "TCP Session: Client disconnected while reading body." << std::endl;
                    } else {
                        std::cerr << "TCP Session: Error reading body: " << ec.message() << std::endl;
                    }
                }
            });
    }

    void do_write() {
        auto self = shared_from_this();
        auto buffers_to_send = tcp_messaging::prepare_message(m_read_body_buffer, m_write_header_buffer);

        boost::asio::async_write(m_socket, buffers_to_send,
            [this, self](const boost::system::error_code& ec, std::size_t /*bytes_transferred*/) {
                if (!ec) {
                    // std::cout << "TCP Session: Wrote response of size " << m_read_body_buffer.size() << std::endl;
                    do_read_header(); // Ready for the next message
                } else {
                    std::cerr << "TCP Session: Error writing response: " << ec.message() << std::endl;
                }
            });
    }

    tcp::socket m_socket;
    std::array<char, tcp_messaging::HEADER_SIZE> m_read_header_buffer;
    std::vector<char> m_read_body_buffer;
    std::array<char, tcp_messaging::HEADER_SIZE> m_write_header_buffer; // For sending response
};

class TCPServer {
public:
    TCPServer(boost::asio::io_context& io_context, unsigned short port)
        : m_io_context(io_context),
          m_acceptor(io_context, tcp::endpoint(tcp::v4(), port)) {
        std::cout << "TCP Server listening on port " << port << std::endl;
        do_accept();
    }

private:
    void do_accept() {
        m_acceptor.async_accept(
            [this](const boost::system::error_code& ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<TCPSession>(std::move(socket))->start();
                } else {
                    std::cerr << "TCP Server: Accept error: " << ec.message() << std::endl;
                }
                do_accept(); // Continue accepting new connections
            });
    }

    boost::asio::io_context& m_io_context;
    tcp::acceptor m_acceptor;
};

int main() {
    try {
        boost::asio::io_context io_context;
        TCPServer server(io_context, config::TCP_SERVER_PORT);
        io_context.run();
    } catch (const std::exception& e) {
        std::cerr << "TCP Server Exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
