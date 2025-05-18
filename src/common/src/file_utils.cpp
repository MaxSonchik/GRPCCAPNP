#include "file_utils.hpp"
#include <iostream>
#include <vector>
#include <random> // Для генерации случайных данных

namespace benchmark_common {

bool generate_test_file(const std::string& filename, size_t size_bytes) {
    std::ofstream outfile(filename, std::ios::binary | std::ios::trunc);
    if (!outfile) {
        std::cerr << "Error: Could not open file " << filename << " for writing." << std::endl;
        return false;
    }

    std::cout << "Generating test file '" << filename << "' of size "
              << (size_bytes / (1024.0 * 1024.0 * 1024.0)) << " GB..." << std::endl;

    const size_t buffer_size = 1 * 1024 * 1024; // 1 MB buffer
    std::vector<char> buffer(buffer_size);

    // Генератор случайных чисел
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, 255);

    for (size_t i = 0; i < buffer_size; ++i) {
        buffer[i] = static_cast<char>(dist(rng));
    }

    size_t bytes_written = 0;
    while (bytes_written < size_bytes) {
        size_t to_write = std::min(buffer_size, size_bytes - bytes_written);
        if (!outfile.write(buffer.data(), to_write)) {
            std::cerr << "Error: Failed to write to file " << filename << std::endl;
            outfile.close();
            return false;
        }
        bytes_written += to_write;

        if (bytes_written % (100 * 1024 * 1024) == 0) { // Обновление каждые 100MB
             std::cout << "Generated " << (bytes_written / (1024.0 * 1024.0)) << " MB..." << std::endl;
        }
    }

    outfile.close();
    std::cout << "Test file generation complete. Total bytes written: " << bytes_written << std::endl;
    return true;
}


size_t ChunkReader::get_file_size(const std::string& fname) {
    std::ifstream in(fname, std::ifstream::ate | std::ifstream::binary);
    if (!in.is_open()) {
        throw std::runtime_error("Could not open file to get_file_size: " + fname);
    }
    return in.tellg();
}

ChunkReader::ChunkReader(const std::string& filename, size_t chunk_size)
    : filename_(filename), chunk_size_(chunk_size), eof_flag_(false), total_bytes_read_(0) {
    file_size_ = get_file_size(filename_);
    file_stream_.open(filename_, std::ios::binary);
    if (!file_stream_) {
        throw std::runtime_error("Error: Could not open file for reading: " + filename_);
    }
}

ChunkReader::~ChunkReader() {
    if (file_stream_.is_open()) {
        file_stream_.close();
    }
}

std::vector<char> ChunkReader::next_chunk() {
    if (eof_flag_ || !file_stream_.is_open()) {
        return {};
    }

    std::vector<char> buffer(chunk_size_);
    file_stream_.read(buffer.data(), chunk_size_);
    std::streamsize bytes_read_this_call = file_stream_.gcount();

    if (bytes_read_this_call > 0) {
        buffer.resize(bytes_read_this_call); // Обрезаем до фактически прочитанного размера
        total_bytes_read_ += bytes_read_this_call;
        return buffer;
    } else {
        eof_flag_ = true;
        if (file_stream_.eof()) {
            // Достигнут конец файла
        } else if (file_stream_.fail()) {
            // Ошибка чтения, не EOF
            std::cerr << "Error: File read failed for " << filename_ << std::endl;
        }
        return {};
    }
}

bool ChunkReader::eof() const {
    return eof_flag_ || (total_bytes_read_ >= file_size_);
}

void ChunkReader::reset() {
    if (file_stream_.is_open()) {
        file_stream_.clear(); // Сбросить флаги ошибок (например, eof)
        file_stream_.seekg(0, std::ios::beg);
    } else {
         file_stream_.open(filename_, std::ios::binary);
         if (!file_stream_) {
            throw std::runtime_error("Error: Could not reopen file for reset: " + filename_);
        }
    }
    eof_flag_ = false;
    total_bytes_read_ = 0;
}

size_t ChunkReader::remaining_bytes() const {
    if (total_bytes_read_ >= file_size_) return 0;
    return file_size_ - total_bytes_read_;
}

} // namespace benchmark_common
