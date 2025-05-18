#ifndef METRICS_AGGREGATOR_HPP
#define METRICS_AGGREGATOR_HPP

#include <string>
#include <vector>
#include <chrono>
#include <cstddef> // For size_t

namespace benchmark_common {

class MetricsAggregator {
public:
    MetricsAggregator(const std::string& protocol_name,
                      size_t total_payload_size_bytes,
                      size_t chunk_size_bytes);

    void record_chunk_sent(size_t payload_size, size_t on_wire_size);
    void record_chunk_rtt_us(long long rtt_us); // RTT в микросекундах
    void set_total_transaction_time_ms(long long time_ms);
    void log_error(const std::string& error_message);

    void print_summary_to_console() const;
    bool save_summary_csv(const std::string& filename) const;
    bool save_detailed_rtt_csv(const std::string& filename) const;

private:
    std::string protocol_name_;
    size_t total_payload_to_transfer_bytes_; // Сколько всего данных мы намеревались передать (один путь)
    size_t actual_payload_transferred_bytes_ = 0; // Сколько реально передано (один путь, по данным от chunk_sent)
    size_t total_on_wire_bytes_ = 0;        // Сумма всех on_wire_size
    size_t chunk_size_bytes_;               // Ожидаемый размер чанка
    long long total_transaction_time_ms_ = 0;

    std::vector<long long> chunk_rtt_us_; // Храним все RTT для детальной статистики
    std::vector<std::string> errors_;

    // Приватные методы для расчетов
    double get_total_payload_sent_mb() const;
    double get_total_data_on_wire_mb() const;
    double get_protocol_overhead_percentage() const;
    double get_throughput_payload_mbps() const; // Payload в один конец
    double get_throughput_on_wire_mbps() const; // On-wire в один конец

    double get_avg_chunk_rtt_ms() const;
    double get_min_chunk_rtt_ms() const;
    double get_max_chunk_rtt_ms() const;
    double get_std_dev_chunk_rtt_ms() const;
    size_t get_num_chunks() const;
};

} // namespace benchmark_common

#endif // METRICS_AGGREGATOR_HPP
