#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <cstddef> // For size_t

namespace config {

// Network Configuration
const unsigned short TCP_SERVER_PORT = 12345;
const std::string DEFAULT_SERVER_IP = "127.0.0.1"; // Default to localhost

// File Configuration
const std::string TEST_FILE_NAME = "test_file.dat";
const std::size_t TOTAL_FILE_SIZE = 10ULL * 1024 * 1024 * 1024; // 10 GB
// const std::size_t TOTAL_FILE_SIZE = 100 * 1024 * 1024; // 100 MB for faster testing
const std::size_t CHUNK_SIZE = 64 * 1024; // 64 KB

// CSV Output Files
const std::string RESULTS_DIR = "results"; // Subdirectory for CSV files
const std::string CPP_OVERALL_METRICS_FILE = RESULTS_DIR + "/cpp_overall_metrics.csv";
const std::string CPP_CHUNK_RTT_METRICS_FILE = RESULTS_DIR + "/cpp_chunk_rtt_metrics.csv";
const std::string GO_OVERALL_METRICS_FILE = RESULTS_DIR + "/go_overall_metrics.csv"; // Placeholder for Go
const std::string GO_CHUNK_RTT_METRICS_FILE = RESULTS_DIR + "/go_chunk_rtt_metrics.csv"; // Placeholder for Go


// CPU/Memory Monitoring (Client-side, Linux specific)
const bool ENABLE_CLIENT_RESOURCE_MONITORING = true; // Set to false to disable
const unsigned int CPU_SAMPLING_INTERVAL_MS = 500; // How often to sample CPU usage

} // namespace config

#endif // CONFIG_HPP
