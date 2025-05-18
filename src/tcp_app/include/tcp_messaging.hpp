#ifndef TCP_MESSAGING_HPP
#define TCP_MESSAGING_HPP

#include <vector>
#include <cstdint> // For uint32_t
#include <boost/asio.hpp>
#include <cstring> // For memcpy
#include <array>   // For std::array

namespace tcp_messaging {

const std::size_t HEADER_SIZE = sizeof(uint32_t);

// Prepares a message with a 4-byte length_prefix header
inline std::vector<boost::asio::const_buffer> prepare_message(const std::vector<char>& payload, std::array<char, HEADER_SIZE>& header_buffer) {
    uint32_t payload_size_net = htonl(static_cast<uint32_t>(payload.size()));
    std::memcpy(header_buffer.data(), &payload_size_net, HEADER_SIZE);

    std::vector<boost::asio::const_buffer> buffers;
    buffers.push_back(boost::asio::buffer(header_buffer.data(), HEADER_SIZE));
    buffers.push_back(boost::asio::buffer(payload.data(), payload.size()));
    return buffers;
}

// Helper to read header
template<typename AsyncReadStream, typename Handler>
void async_read_header(AsyncReadStream& socket, std::array<char, HEADER_SIZE>& header_buffer, Handler handler) {
    boost::asio::async_read(socket, boost::asio::buffer(header_buffer.data(), HEADER_SIZE),
        [handler](const boost::system::error_code& ec, std::size_t /*length*/) {
            handler(ec);
        });
}

// Helper to parse header
inline uint32_t parse_header(const std::array<char, HEADER_SIZE>& header_buffer) {
    uint32_t msg_len_net;
    std::memcpy(&msg_len_net, header_buffer.data(), HEADER_SIZE);
    return ntohl(msg_len_net);
}




} // namespace tcp_messaging

#endif // TCP_MESSAGING_HPP
