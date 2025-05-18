// common/include/file_utils.hpp
#ifndef FILE_UTILS_HPP
#define FILE_UTILS_HPP

#include <string>
#include <fstream>
#include <vector>
#include <cstddef> // For size_t

namespace benchmark_common {

bool generate_test_file(const std::string& filename, size_t size_bytes);

class ChunkReader {
public:
    // Статический метод для получения размера файла, как в вашей реализации
    static size_t get_file_size(const std::string& fname);

    ChunkReader(const std::string& filename, size_t chunk_size);
    ~ChunkReader();

    // Возвращает следующий чанк данных.
    // Возвращает пустой вектор, если достигнут конец файла или произошла ошибка.
    std::vector<char> next_chunk();

    // Проверяет, достигнут ли конец файла.
    bool eof() const;

    // Сбрасывает состояние чтения к началу файла.
    void reset();

    // Возвращает количество байт, оставшихся для чтения.
    size_t remaining_bytes() const;

    // Методы, которые были в вашей реализации, но не в моем hpp:
    size_t get_total_bytes_read_from_impl() const { return total_bytes_read_; } // Если нужен доступ к total_bytes_read_
    size_t get_file_size_from_impl() const { return file_size_; } // Если нужен доступ к file_size_

private:
    std::string filename_;
    size_t chunk_size_;
    std::ifstream file_stream_;
    size_t file_size_; // Размер файла, определенный при открытии
    bool eof_flag_;
    size_t total_bytes_read_;
};

} // namespace benchmark_common

#endif // FILE_UTILS_HPP
