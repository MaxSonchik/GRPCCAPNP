#include "../include/metrics_aggregator.hpp"
#include <fstream>
#include <iostream>
#include <iomanip> // For std::fixed, std::setprecision
#include <numeric> // For std::accumulate
#include <algorithm> // For std::min_element, std::max_element
#include <cmath>     // For std::sqrt

namespace benchmark_common {

MetricsAggregator::MetricsAggregator(const std::string& protocol_name,
                                     size_t total_payload_size_bytes,
                                     size_t chunk_size_bytes)
    : protocol_name_(protocol_name),
      total_payload_to_transfer_bytes_(total_payload_size_bytes),
      chunk_size_bytes_(chunk_size_bytes) {}

void MetricsAggregator::record_chunk_sent(size_t payload_size, size_t on_wire_size) {
    actual_payload_transferred_bytes_ += payload_size;
    total_on_wire_bytes_ += on_wire_size;
}

void MetricsAggregator::record_chunk_rtt_us(long long rtt_us) {
    chunk_rtt_us_.push_back(rtt_us);
}

void MetricsAggregator::set_total_transaction_time_ms(long long time_ms) {
    total_transaction_time_ms_ = time_ms;
}

void MetricsAggregator::log_error(const std::string& error_message) {
    errors_.push_back(error_message);
}

// --- Приватные методы для расчетов ---
double MetricsAggregator::get_total_payload_sent_mb() const {
    return static_cast<double>(actual_payload_transferred_bytes_) / (1024.0 * 1024.0);
}

double MetricsAggregator::get_total_data_on_wire_mb() const {
    return static_cast<double>(total_on_wire_bytes_) / (1024.0 * 1024.0);
}

double MetricsAggregator::get_protocol_overhead_percentage() const {
    if (actual_payload_transferred_bytes_ == 0) return 0.0;
    // Считаем оверхед на передачу в одну сторону.
    // Если total_on_wire_bytes_ это сумма (оригинал + префикс),
    // то оверхед = (total_on_wire_bytes_ - actual_payload_transferred_bytes_) / actual_payload_transferred_bytes_
    // Но ваш gRPC пример считает (TotalDataSentOnWire - TotalPayloadSent) / TotalPayloadSent
    // где TotalPayloadSent - это полезная нагрузка, а TotalDataSentOnWire - это полезная + оверхед протокола.
    // Для Cap'n Proto: on_wire_size чанка = payload_size (так как нет явного заголовка длины в нашем RPC вызове,
    // Cap'n Proto сам кадрирует). Но оверхед все равно есть из-за структуры сообщения Cap'n Proto.
    // Мы будем считать, что `on_wire_size` это то, что реально ушло по сети для одного чанка.
    // И `actual_payload_transferred_bytes_` это сумма `payload_size` этих чанков.
    double overhead = static_cast<double>(total_on_wire_bytes_ - actual_payload_transferred_bytes_);
    return (overhead / static_cast<double>(actual_payload_transferred_bytes_)) * 100.0;
}

double MetricsAggregator::get_throughput_payload_mbps() const { // Payload в один конец
    if (total_transaction_time_ms_ == 0) return 0.0;
    // Скорость передачи полезной нагрузки в одну сторону
    double time_s = static_cast<double>(total_transaction_time_ms_) / 1000.0;
    double payload_mb = get_total_payload_sent_mb();
    return (payload_mb * 8.0) / time_s; // в Мбит/с
}

double MetricsAggregator::get_throughput_on_wire_mbps() const { // On-wire в один конец
    if (total_transaction_time_ms_ == 0) return 0.0;
    double time_s = static_cast<double>(total_transaction_time_ms_) / 1000.0;
    double on_wire_mb = get_total_data_on_wire_mb();
    return (on_wire_mb * 8.0) / time_s; // в Мбит/с
}

double MetricsAggregator::get_avg_chunk_rtt_ms() const {
    if (chunk_rtt_us_.empty()) return 0.0;
    long long sum_rtt_us = std::accumulate(chunk_rtt_us_.begin(), chunk_rtt_us_.end(), 0LL);
    return static_cast<double>(sum_rtt_us) / chunk_rtt_us_.size() / 1000.0; // в мс
}

double MetricsAggregator::get_min_chunk_rtt_ms() const {
    if (chunk_rtt_us_.empty()) return 0.0;
    return static_cast<double>(*std::min_element(chunk_rtt_us_.begin(), chunk_rtt_us_.end())) / 1000.0;
}

double MetricsAggregator::get_max_chunk_rtt_ms() const {
    if (chunk_rtt_us_.empty()) return 0.0;
    return static_cast<double>(*std::max_element(chunk_rtt_us_.begin(), chunk_rtt_us_.end())) / 1000.0;
}

double MetricsAggregator::get_std_dev_chunk_rtt_ms() const {
    if (chunk_rtt_us_.size() < 2) return 0.0;
    double mean = get_avg_chunk_rtt_ms() * 1000.0; // в us для точности расчета
    double sq_sum = 0.0;
    for (long long rtt_us : chunk_rtt_us_) {
        sq_sum += (static_cast<double>(rtt_us) - mean) * (static_cast<double>(rtt_us) - mean);
    }
    return std::sqrt(sq_sum / (chunk_rtt_us_.size() -1) ) / 1000.0; // в мс
}

size_t MetricsAggregator::get_num_chunks() const {
    return chunk_rtt_us_.size(); // Или по actual_payload_transferred_bytes_ / chunk_size_bytes_
}


void MetricsAggregator::print_summary_to_console() const {
    std::cout << "\n--- Benchmark Summary (" << protocol_name_ << ") ---" << std::endl;
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "TotalTransactionTime:        " << static_cast<double>(total_transaction_time_ms_) / 1000.0 << " s" << std::endl;
    std::cout << "TotalPayloadSent:            " << get_total_payload_sent_mb() << " MB" << std::endl;
    std::cout << "TotalDataSentOnWire:         " << get_total_data_on_wire_mb() << " MB" << std::endl;
    std::cout << "ProtocolOverheadSend:        " << get_protocol_overhead_percentage() << " %" << std::endl;
    std::cout << "ThroughputPayload_Mbps:      " << get_throughput_payload_mbps() << " Mbps" << std::endl;
    std::cout << "ThroughputPayload_Gbps:      " << get_throughput_payload_mbps() / 1000.0 << " Gbps" << std::endl;
    std::cout << "ThroughputOnWire_Mbps:       " << get_throughput_on_wire_mbps() << " Mbps" << std::endl;
    std::cout << "ThroughputOnWire_Gbps:       " << get_throughput_on_wire_mbps() / 1000.0 << " Gbps" << std::endl;
    if (!chunk_rtt_us_.empty()) {
        std::cout << "AvgChunkRTT:                 " << get_avg_chunk_rtt_ms() << " ms" << std::endl;
        std::cout << "MinChunkRTT:                 " << get_min_chunk_rtt_ms() << " ms" << std::endl;
        std::cout << "MaxChunkRTT:                 " << get_max_chunk_rtt_ms() << " ms" << std::endl;
        std::cout << "StdDevChunkRTT:              " << get_std_dev_chunk_rtt_ms() << " ms" << std::endl;
    }
    std::cout << "NumChunks:                   " << get_num_chunks() << std::endl;
    if (!errors_.empty()) {
        std::cout << "Errors (" << errors_.size() << "):" << std::endl;
        for (const auto& err : errors_) {
            std::cout << "  - " << err << std::endl;
        }
    }
    std::cout << "-----------------------------------\n" << std::endl;
}

bool MetricsAggregator::save_summary_csv(const std::string& filename) const {
    std::ofstream outfile(filename);
    if (!outfile) {
        std::cerr << "[ERROR] Failed to open summary CSV file for writing: " << filename << std::endl;
        return false;
    }
    outfile << "Metric,Value,Unit\n";
    outfile << std::fixed << std::setprecision(6); // Точность для float/double

    outfile << "Protocol," << protocol_name_ << ",\n";
    outfile << "TotalTransactionTime," << static_cast<double>(total_transaction_time_ms_) / 1000.0 << ",s\n";
    outfile << "TotalPayloadSent," << get_total_payload_sent_mb() << ",MB\n";
    outfile << "TotalDataSentOnWire," << get_total_data_on_wire_mb() << ",MB\n";
    outfile << "ProtocolOverheadSend," << get_protocol_overhead_percentage() << ",%\n";
    outfile << "ThroughputPayload_Mbps," << get_throughput_payload_mbps() << ",Mbps\n";
    outfile << "ThroughputPayload_Gbps," << get_throughput_payload_mbps() / 1000.0 << ",Gbps\n";
    outfile << "ThroughputOnWire_Mbps," << get_throughput_on_wire_mbps() << ",Mbps\n";
    outfile << "ThroughputOnWire_Gbps," << get_throughput_on_wire_mbps() / 1000.0 << ",Gbps\n";
    if (!chunk_rtt_us_.empty()) {
        outfile << "AvgChunkRTT," << get_avg_chunk_rtt_ms() << ",ms\n";
        outfile << "MinChunkRTT," << get_min_chunk_rtt_ms() << ",ms\n";
        outfile << "MaxChunkRTT," << get_max_chunk_rtt_ms() << ",ms\n";
        outfile << "StdDevChunkRTT," << get_std_dev_chunk_rtt_ms() << ",ms\n";
    }
    outfile << "NumChunks," << get_num_chunks() << ",\n";
    outfile << "ErrorsEncountered," << errors_.size() << ",\n";

    outfile.close();
    std::cout << "[INFO] Summary metrics saved to " << filename << std::endl;
    return true;
}

bool MetricsAggregator::save_detailed_rtt_csv(const std::string& filename) const {
    if (chunk_rtt_us_.empty()) {
        std::cout << "[INFO] No RTT data to save for " << filename << std::endl;
        return true; // Не ошибка, просто нечего сохранять
    }
    std::ofstream outfile(filename);
    if (!outfile) {
        std::cerr << "[ERROR] Failed to open detailed RTT CSV file for writing: " << filename << std::endl;
        return false;
    }
    outfile << "ChunkNumber,RTT_us\n";
    for (size_t i = 0; i < chunk_rtt_us_.size(); ++i) {
        outfile << (i + 1) << "," << chunk_rtt_us_[i] << "\n";
    }
    outfile.close();
    std::cout << "[INFO] Detailed RTT metrics saved to " << filename << std::endl;
    return true;
}

} // namespace benchmark_common
