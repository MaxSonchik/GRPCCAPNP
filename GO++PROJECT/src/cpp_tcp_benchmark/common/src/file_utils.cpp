#include "file_utils.hpp"
#include <filesystem> // Requires C++17. Link with -lstdc++fs or -lboost_filesystem if compiler needs it
#include <fstream>
#include <iostream>
#include <random>
#include <vector>

namespace fs = std::filesystem;

void generate_test_file_if_not_exists(const std::string &filename,
                                      std::size_t target_size) {
  bool regenerate = false;
  if (!fs::exists(filename)) {
    std::cout << "Test file '" << filename << "' does not exist. Generating..."
              << std::endl;
    regenerate = true;
  } else {
    std::uintmax_t current_size = fs::file_size(filename);
    if (current_size != target_size) {
      std::cout << "Test file '" << filename
                << "' exists but has incorrect size (" << current_size
                << " bytes vs expected " << target_size
                << " bytes). Regenerating..." << std::endl;
      regenerate = true;
      fs::remove(filename); // Remove the old file
    } else {
      std::cout << "Test file '" << filename
                << "' already exists with correct size." << std::endl;
    }
  }

  if (regenerate) {
    std::ofstream outfile(filename, std::ios::binary | std::ios::out);
    if (!outfile) {
      std::cerr << "Error: Could not open file " << filename << " for writing."
                << std::endl;
      // Consider throwing an exception or exiting
      throw std::runtime_error("Failed to open test file for writing.");
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, 255);

    std::size_t buffer_size = 1 * 1024 * 1024; // 1MB buffer
    std::vector<char> buffer(buffer_size);
    std::size_t bytes_written = 0;

    std::cout << "Generating " << target_size / (1024.0 * 1024.0)
              << " MB file. This may take a while..." << std::endl;
    int progress_counter = 0;

    while (bytes_written < target_size) {
      std::size_t bytes_to_write =
          std::min(buffer_size, target_size - bytes_written);
      for (std::size_t i = 0; i < bytes_to_write; ++i) {
        buffer[i] = static_cast<char>(distrib(gen));
      }
      outfile.write(buffer.data(), bytes_to_write);
      if (!outfile) {
        std::cerr << "Error writing to file " << filename << ". Disk full?"
                  << std::endl;
        outfile.close();
        fs::remove(filename); // Clean up partial file
        throw std::runtime_error("Failed to write to test file.");
      }
      bytes_written += bytes_to_write;

      if (++progress_counter % 100 == 0) { // Print progress every 100 MB
        std::cout << ".";
        std::cout.flush();
      }
    }
    std::cout << std::endl
              << "File '" << filename << "' generated successfully ("
              << bytes_written << " bytes)." << std::endl;
    outfile.close();
  }
}
