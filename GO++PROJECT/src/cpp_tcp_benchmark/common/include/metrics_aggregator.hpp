#ifndef METRICS_AGGREGATOR_HPP
#define METRICS_AGGREGATOR_HPP

#include <chrono>
#include <cstddef> // For size_t
#include <string>
#include <vector>

#ifdef __linux__
#include <fstream> // For reading /proc
#include <sstream> // For parsing /proc
#endif

struct ChunkRTTInfo {
  std::size_t chunk_index;
  std::chrono::microseconds rtt;
  std::size_t chunk_size_bytes;
  bool verified;
};

class MetricsAggregator {
public:
  MetricsAggregator(const std::string &protocol_name,
                    std::size_t total_file_size_expected,
                    std::size_t chunk_size_expected);

  void start_timer();
  void stop_timer();

  void start_chunk_rtt_timer();
  void stop_and_record_chunk_rtt(std::size_t chunk_size_bytes, bool verified);

  void record_chunk_processed(
      std::size_t
          bytes_processed); // Legacy, might be replaced by RTT recording
  void record_chunk_verified(
      bool success); // Legacy, might be replaced by RTT recording

  void print_summary() const;
  void save_to_csv(const std::string &overall_metrics_file,
                   const std::string &chunk_rtt_file) const;

  // Client resource usage (Linux specific)
  void start_resource_monitoring();
  void stop_resource_monitoring();

private:
  // For CPU usage
  struct ProcStatInfo {
    unsigned long long utime = 0; // user time
    unsigned long long stime = 0; // system time
    unsigned long long total_time() const { return utime + stime; }
  };
  ProcStatInfo get_proc_stat() const;
  long get_clk_tck() const; // System clock ticks per second
  long get_peak_memory_kb() const;

  std::string m_protocol_name;
  std::size_t m_total_file_size_expected;
  std::size_t m_chunk_size_expected;

  std::chrono::steady_clock::time_point m_start_time;
  std::chrono::steady_clock::time_point m_end_time;
  bool m_timer_running = false;

  std::chrono::steady_clock::time_point m_current_chunk_rtt_start_time;

  std::size_t m_total_bytes_processed = 0;
  std::size_t m_verified_chunks_count = 0;
  std::size_t m_processed_chunks_count =
      0; // Tracks how many chunks had RTT recorded

  std::vector<ChunkRTTInfo> m_chunk_rtt_data;

  // Client resource usage
  bool m_resource_monitoring_enabled;
  std::chrono::steady_clock::time_point m_resource_monitor_start_time;
  ProcStatInfo m_cpu_stat_start;
  double m_avg_cpu_usage_percent = 0.0; // Average over the benchmark duration
  long m_peak_memory_kb = 0;            // Peak resident set size (VmHWM)
  bool m_resource_monitoring_active = false;
};

#endif // METRICS_AGGREGATOR_HPP
