#ifndef FILE_UTILS_HPP
#define FILE_UTILS_HPP

#include <cstddef> // For size_t
#include <string>
#include <vector>

void generate_test_file_if_not_exists(const std::string &filename,
                                      std::size_t target_size);

#endif // FILE_UTILS_HPP
