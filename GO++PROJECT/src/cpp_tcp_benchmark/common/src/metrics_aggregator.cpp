#include "metrics_aggregator.hpp"
#include "config.hpp" // To get config::ENABLE_CLIENT_RESOURCE_MONITORING
#include <fstream>
#include <iostream>

#include <filesystem> // Для std::filesystem
namespace fs = std::filesystem;

#include <algorithm> // For std::min/max_element
#include <iomanip>   // For std::fixed, std::setprecision
#include <numeric>   // For std::accumulate

#ifdef __linux__
#include <unistd.h> // For sysconf(_SC_CLK_TCK)
#endif

MetricsAggregator::MetricsAggregator(const std::string &protocol_name,
                                     std::size_t total_file_size_expected,
                                     std::size_t chunk_size_expected)
    : m_protocol_name(protocol_name),
      m_total_file_size_expected(total_file_size_expected),
      m_chunk_size_expected(chunk_size_expected),
      m_resource_monitoring_enabled(config::ENABLE_CLIENT_RESOURCE_MONITORING) {
}

void MetricsAggregator::start_timer() {
  if (!m_timer_running) {
    m_start_time = std::chrono::steady_clock::now();
    m_timer_running = true;
    if (m_resource_monitoring_enabled) {
      start_resource_monitoring();
    }
  }
}

void MetricsAggregator::stop_timer() {
  if (m_timer_running) {
    m_end_time = std::chrono::steady_clock::now();
    m_timer_running = false;
    if (m_resource_monitoring_enabled) {
      stop_resource_monitoring();
    }
  }
}

void MetricsAggregator::start_chunk_rtt_timer() {
  m_current_chunk_rtt_start_time = std::chrono::steady_clock::now();
}

void MetricsAggregator::stop_and_record_chunk_rtt(std::size_t chunk_size_bytes,
                                                  bool verified) {
  auto rtt_end_time = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
      rtt_end_time - m_current_chunk_rtt_start_time);

  m_chunk_rtt_data.push_back(
      {m_processed_chunks_count + 1, duration, chunk_size_bytes, verified});
  m_processed_chunks_count++; // Increment after adding to keep index 1-based
  m_total_bytes_processed += chunk_size_bytes;
  if (verified) {
    m_verified_chunks_count++;
  }
}

void MetricsAggregator::record_chunk_processed(std::size_t bytes_processed) {
  m_total_bytes_processed += bytes_processed;
}

void MetricsAggregator::record_chunk_verified(bool success) {
  // This is somewhat legacy now that RTT records verification status
  // For now, let's ensure it's consistent if called
  // m_processed_chunks_count++; // This was for an older way of counting
  if (success) {
    // m_verified_chunks_count++; // This also needs care if called
    // independently
  }
}

#ifdef __linux__
long MetricsAggregator::get_clk_tck() const { return sysconf(_SC_CLK_TCK); }

MetricsAggregator::ProcStatInfo MetricsAggregator::get_proc_stat() const {
  ProcStatInfo stat_info;
  std::ifstream stat_file("/proc/self/stat");
  if (stat_file.is_open()) {
    std::string line;
    std::getline(stat_file, line);
    std::stringstream ss(line);
    std::string value;
    // Skip to the 14th (utime) and 15th (stime) values
    for (int i = 1; i < 14; ++i)
      ss >> value;
    ss >> stat_info.utime; // 14th field: utime
    ss >> stat_info.stime; // 15th field: stime
  }
  return stat_info;
}

long MetricsAggregator::get_peak_memory_kb() const {
  long peak_mem = 0;
  std::ifstream status_file("/proc/self/status");
  if (status_file.is_open()) {
    std::string line;
    while (std::getline(status_file, line)) {
      if (line.rfind("VmHWM:", 0) == 0) { // Starts with VmHWM:
        std::stringstream ss(line);
        std::string key, value, unit;
        ss >> key >> value >> unit;
        try {
          peak_mem = std::stol(value); // Value is in kB
        } catch (const std::exception &e) {
          std::cerr << "MetricsAggregator: Error parsing VmHWM: " << e.what()
                    << std::endl;
        }
        break;
      }
    }
  }
  return peak_mem;
}

void MetricsAggregator::start_resource_monitoring() {
  if (!m_resource_monitoring_enabled || m_resource_monitoring_active)
    return;
  m_resource_monitor_start_time = std::chrono::steady_clock::now();
  m_cpu_stat_start = get_proc_stat();
  m_peak_memory_kb = 0; // Reset for new run
  m_resource_monitoring_active = true;
}

void MetricsAggregator::stop_resource_monitoring() {
  if (!m_resource_monitoring_enabled || !m_resource_monitoring_active)
    return;

  auto monitor_end_time = std::chrono::steady_clock::now();
  ProcStatInfo cpu_stat_end = get_proc_stat();
  m_peak_memory_kb = get_peak_memory_kb(); // Get peak memory at the very end

  auto duration_sec = std::chrono::duration_cast<std::chrono::duration<double>>(
                          monitor_end_time - m_resource_monitor_start_time)
                          .count();

  unsigned long long cpu_ticks_used =
      (cpu_stat_end.total_time() - m_cpu_stat_start.total_time());
  long clk_tck = get_clk_tck();

  if (duration_sec > 0 && clk_tck > 0) {
    double cpu_seconds_used = static_cast<double>(cpu_ticks_used) / clk_tck;
    m_avg_cpu_usage_percent = (cpu_seconds_used / duration_sec) * 100.0;
    // CPU usage can be > 100% on multi-core systems, this reflects total core
    // utilization
  } else {
    m_avg_cpu_usage_percent = 0.0;
  }
  m_resource_monitoring_active = false;
}
#else // Non-Linux stubs for resource monitoring
void MetricsAggregator::start_resource_monitoring() {
  m_avg_cpu_usage_percent = -1.0;
  m_peak_memory_kb = -1;
} // Indicate not available
void MetricsAggregator::stop_resource_monitoring() { /* Do nothing */ }
long MetricsAggregator::get_peak_memory_kb() const { return -1; }
#endif

void MetricsAggregator::print_summary() const {
  std::cout << "\n--- " << m_protocol_name << " Benchmark Summary ---"
            << std::endl;
  if (!m_timer_running &&
      m_start_time != std::chrono::steady_clock::time_point()) {
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        m_end_time - m_start_time);
    double duration_sec = duration.count() / 1000.0;

    std::cout << "Total time: " << std::fixed << std::setprecision(3)
              << duration_sec << " s" << std::endl;
    std::cout << "Total bytes processed: " << m_total_bytes_processed
              << " bytes (" << std::fixed << std::setprecision(2)
              << (m_total_bytes_processed / (1024.0 * 1024.0)) << " MB)"
              << std::endl;

    if (duration_sec > 0) {
      double throughput_mbps =
          (static_cast<double>(m_total_bytes_processed) * 8) /
          (duration_sec * 1024 * 1024);
      std::cout << "Throughput: " << std::fixed << std::setprecision(2)
                << throughput_mbps << " Mbps" << std::endl;
    } else {
      std::cout << "Throughput: N/A (duration is zero)" << std::endl;
    }
    std::cout << "Total chunks processed: " << m_processed_chunks_count
              << std::endl;
    std::cout << "Chunks verified successfully: " << m_verified_chunks_count
              << std::endl;
    if (m_processed_chunks_count > 0 &&
        m_verified_chunks_count < m_processed_chunks_count) {
      std::cout << "WARNING: "
                << (m_processed_chunks_count - m_verified_chunks_count)
                << " chunks failed verification!" << std::endl;
    }

    if (config::ENABLE_CLIENT_RESOURCE_MONITORING) {
#ifdef __linux__
      std::cout << "Client Avg CPU Usage: " << std::fixed
                << std::setprecision(2) << m_avg_cpu_usage_percent << " %"
                << std::endl;
      std::cout << "Client Peak Memory (RSS): " << m_peak_memory_kb << " KB ("
                << std::fixed << std::setprecision(2)
                << (m_peak_memory_kb / 1024.0) << " MB)" << std::endl;
#else
      std::cout
          << "Client CPU/Memory monitoring: Not available on this platform."
          << std::endl;
#endif
    }

  } else {
    std::cout << "Timer was not run or is still running. No summary available."
              << std::endl;
  }

  if (!m_chunk_rtt_data.empty()) {
    auto sum_rtt = std::accumulate(
        m_chunk_rtt_data.begin(), m_chunk_rtt_data.end(),
        std::chrono::microseconds(0),
        [](std::chrono::microseconds sum, const ChunkRTTInfo &item) {
          return sum + item.rtt;
        });
    double avg_rtt_ms =
        static_cast<double>(sum_rtt.count()) / m_chunk_rtt_data.size() / 1000.0;

    auto min_rtt_it =
        std::min_element(m_chunk_rtt_data.begin(), m_chunk_rtt_data.end(),
                         [](const ChunkRTTInfo &a, const ChunkRTTInfo &b) {
                           return a.rtt < b.rtt;
                         });
    double min_rtt_ms = static_cast<double>(min_rtt_it->rtt.count()) / 1000.0;

    auto max_rtt_it =
        std::max_element(m_chunk_rtt_data.begin(), m_chunk_rtt_data.end(),
                         [](const ChunkRTTInfo &a, const ChunkRTTInfo &b) {
                           return a.rtt < b.rtt;
                         });
    double max_rtt_ms = static_cast<double>(max_rtt_it->rtt.count()) / 1000.0;

    std::cout << "Chunk RTT (ms) - Avg: " << std::fixed << std::setprecision(3)
              << avg_rtt_ms << ", Min: " << min_rtt_ms
              << ", Max: " << max_rtt_ms << std::endl;
  }

  std::cout << "--- End of Summary ---" << std::endl;
}

void MetricsAggregator::save_to_csv(
    const std::string &overall_metrics_file_path,
    const std::string &chunk_rtt_file_path) const {
  // Create results directory if it doesn't exist
  fs::path dir_path = fs::path(overall_metrics_file_path).parent_path();
  if (!dir_path.empty() && !fs::exists(dir_path)) {
    fs::create_directories(dir_path);
  }

  // --- Save Overall Metrics ---
  std::ofstream overall_file(overall_metrics_file_path);
  if (!overall_file.is_open()) {
    std::cerr << "Error: Could not open file " << overall_metrics_file_path
              << " for writing overall metrics." << std::endl;
    return;
  }

  overall_file
      << "Protocol,TotalTime_s,TotalBytesProcessed,Throughput_Mbps,TotalChunks,"
         "VerifiedChunks,ClientAvgCPU_percent,ClientPeakMemory_KB\n";
  if (!m_timer_running &&
      m_start_time != std::chrono::steady_clock::time_point()) {
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        m_end_time - m_start_time);
    double duration_sec = duration.count() / 1000.0;
    double throughput_mbps =
        (duration_sec > 0)
            ? (static_cast<double>(m_total_bytes_processed) * 8) /
                  (duration_sec * 1024 * 1024)
            : 0.0;

    overall_file << m_protocol_name << "," << std::fixed << std::setprecision(6)
                 << duration_sec << "," << m_total_bytes_processed << ","
                 << std::fixed << std::setprecision(6) << throughput_mbps << ","
                 << m_processed_chunks_count << "," << m_verified_chunks_count
                 << "," << std::fixed << std::setprecision(2)
                 << m_avg_cpu_usage_percent << "," << m_peak_memory_kb << "\n";
  }
  overall_file.close();
  std::cout << "Overall metrics saved to " << overall_metrics_file_path
            << std::endl;

  // --- Save Chunk RTT Metrics ---
  std::ofstream rtt_file(chunk_rtt_file_path);
  if (!rtt_file.is_open()) {
    std::cerr << "Error: Could not open file " << chunk_rtt_file_path
              << " for writing chunk RTT metrics." << std::endl;
    return;
  }
  rtt_file << "Protocol,ChunkIndex,RTT_us,ChunkSizeBytes,Verified\n";
  for (const auto &rtt_info : m_chunk_rtt_data) {
    rtt_file << m_protocol_name << "," << rtt_info.chunk_index << ","
             << rtt_info.rtt.count() << "," << rtt_info.chunk_size_bytes << ","
             << (rtt_info.verified ? "true" : "false") << "\n";
  }
  rtt_file.close();
  std::cout << "Chunk RTT metrics saved to " << chunk_rtt_file_path
            << std::endl;
}
