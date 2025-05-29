#ifndef REVERSAL_UTILS_HPP
#define REVERSAL_UTILS_HPP

#include <algorithm> // For std::reverse
#include <vector>

namespace utils {

// Reverses the content of the vector in-place
inline void reverse_vector_content(std::vector<char> &data) {
  std::reverse(data.begin(), data.end());
}

// Returns a new vector with reversed content
inline std::vector<char>
get_reversed_vector_content(const std::vector<char> &data) {
  std::vector<char> reversed_data = data;
  std::reverse(reversed_data.begin(), reversed_data.end());
  return reversed_data;
}

} // namespace utils

#endif // REVERSAL_UTILS_HPP
