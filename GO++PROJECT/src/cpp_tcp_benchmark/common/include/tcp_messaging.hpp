#ifndef TCP_MESSAGING_HPP
#define TCP_MESSAGING_HPP

#include <algorithm> // For std::reverse with iterators for endian conversion (if needed)
#include <array> // For std::array
#include <boost/asio.hpp>
#include <cstdint> // For uint32_t
#include <cstring> // For memcpy
#include <vector>

// It's good practice to ensure network byte order (Big Endian)
// For portable code, prefer boost/asio/detail/socket_ops.hpp for htonl/ntohl
// or implement manually if Boost version is too old or for no Boost dependency
// here. For simplicity, assuming standard POSIX/Windows availability of
// htonl/ntohl
#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h> // For htonl, ntohl
#else
#include <arpa/inet.h> // For htonl, ntohl
#endif

namespace tcp_messaging {

const std::size_t HEADER_SIZE = sizeof(uint32_t);

// Prepares a message with a 4-byte length_prefix header (network byte order)
inline std::vector<boost::asio::const_buffer>
prepare_message(const std::vector<char> &payload,
                std::array<char, HEADER_SIZE> &header_buffer) {
  uint32_t payload_size_net = htonl(static_cast<uint32_t>(payload.size()));
  std::memcpy(header_buffer.data(), &payload_size_net, HEADER_SIZE);

  std::vector<boost::asio::const_buffer> buffers;
  buffers.push_back(boost::asio::buffer(header_buffer.data(), HEADER_SIZE));
  if (!payload.empty()) { // Only add payload buffer if payload is not empty
    buffers.push_back(boost::asio::buffer(payload.data(), payload.size()));
  }
  return buffers;
}

// Helper to read header
template <typename AsyncReadStream, typename Handler>
void async_read_header(AsyncReadStream &socket,
                       std::array<char, HEADER_SIZE> &header_buffer,
                       Handler handler) {
  boost::asio::async_read(
      socket, boost::asio::buffer(header_buffer.data(), HEADER_SIZE),
      // Pass the handler directly
      handler
      // Original lambda was:
      // [handler](const boost::system::error_code& ec, std::size_t /*length*/)
      // {
      //     handler(ec); // Call the passed-in handler
      // }
  );
}

// Helper to parse header (assumes network byte order)
inline uint32_t
parse_header(const std::array<char, HEADER_SIZE> &header_buffer) {
  uint32_t msg_len_net;
  std::memcpy(&msg_len_net, header_buffer.data(), HEADER_SIZE);
  return ntohl(msg_len_net);
}

} // namespace tcp_messaging

#endif // TCP_MESSAGING_HPP
