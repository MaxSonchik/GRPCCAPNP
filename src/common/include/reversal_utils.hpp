#pragma once

#include <vector>
#include <string>
#include <algorithm> // Для std::reverse

namespace benchmark_common {

// Реверсирует содержимое вектора байт (char)
inline void reverse_bytes(std::vector<char>& data) {
    std::reverse(data.begin(), data.end());
}

// Реверсирует содержимое строки (полезно для gRPC, который работает со std::string)
inline void reverse_string(std::string& data) {
    std::reverse(data.begin(), data.end());
}

} // namespace benchmark_common
