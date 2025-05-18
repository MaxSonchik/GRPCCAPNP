// grpc_app/grpc_client.cpp
#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <map> // Изменено с deque на map
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <iomanip>
#include <algorithm>

#include <grpcpp/grpcpp.h>
#include <grpcpp/impl/codegen/byte_buffer.h> // Для ByteSizeLong

#include "gen_proto/benchmark.grpc.pb.h" // Путь к вашим сгенерированным файлам

#include "common/include/config.hpp"
#include "common/include/file_utils.hpp"
#include "common/include/reversal_utils.hpp"
#include "common/include/metrics_aggregator.hpp"

#ifndef UNUSED_PARAM
#define UNUSED_PARAM(x) (void)(x)
#endif

using grpc::Channel;
using grpc::ChannelArguments;
using grpc::ClientContext;
using grpc::ClientReaderWriter;
using grpc::Status;
using benchmark_grpc::ChunkRequest;
using benchmark_grpc::ChunkResponse;
using benchmark_grpc::FileProcessor;

// Вспомогательная функция для вывода HEX-дампа
void print_client_hex_data(const std::string& title, const std::vector<char>& data, size_t count = 32) {
    std::cout << "[CLIENT DEBUG HEX] " << title << " (first " << std::min(count, data.size()) << " of " << data.size() << " bytes): ";
    for (size_t i = 0; i < std::min(count, data.size()); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(static_cast<unsigned char>(data[i])) << " ";
    }
    std::cout << std::dec << std::endl;
}


class GrpcFileClient {
public:
    GrpcFileClient(std::shared_ptr<Channel> channel)
        : stub_(FileProcessor::NewStub(channel)) {}

    void ProcessFile(const std::string& filename_to_send,
                     size_t configured_chunk_size,
                     benchmark_common::MetricsAggregator& metrics_collector) {

        std::cout << "[gRPC CLIENT INFO] Preparing to process file: " << filename_to_send << std::endl;

        benchmark_common::ChunkReader reader(filename_to_send, configured_chunk_size);

        ClientContext context;
        std::chrono::system_clock::time_point deadline =
            std::chrono::system_clock::now() + std::chrono::minutes(15); // Увеличил дедлайн
        context.set_deadline(deadline);

        std::shared_ptr<ClientReaderWriter<ChunkRequest, ChunkResponse>> stream(
            stub_->ProcessFileChunks(&context));

        if (!stream) {
            std::string err_msg = "gRPC Client: Failed to create stream. Stub returned nullptr.";
            std::cerr << "[gRPC CLIENT ERROR] " << err_msg << std::endl;
            metrics_collector.log_error(err_msg);
            return;
        }

        std::cout << "[gRPC CLIENT INFO] Connected to server. Starting to stream file." << std::endl;

        auto overall_processing_start_time = std::chrono::steady_clock::now();

        struct SentChunkInfo {
            size_t client_assigned_id;
            size_t original_payload_size;
            size_t on_wire_request_size_bytes;
            std::vector<char> original_data_for_verification;
            std::chrono::steady_clock::time_point time_sent;
        };

        std::map<size_t, SentChunkInfo> inflight_requests_map; // Используем map
        std::mutex map_mutex;
        std::condition_variable cv_can_send;
        std::atomic<bool> writer_thread_finished_sending{false}; // Переименовал для ясности
        std::atomic<bool> writer_stream_broken{false};
        std::atomic<size_t> total_chunks_actually_sent_by_writer{0};

        const size_t MAX_INFLIGHT_REQUESTS = 2000; // Можно настроить

        std::thread writer_thread([&]() {
            size_t client_chunk_id_counter = 0;
            std::vector<char> chunk_data_buffer;

            try {
                while (true) {
                    {
                        std::unique_lock<std::mutex> lock(map_mutex);
                        cv_can_send.wait(lock, [&]{ return inflight_requests_map.size() < MAX_INFLIGHT_REQUESTS; });
                    }

                    chunk_data_buffer = reader.next_chunk();
                    if (chunk_data_buffer.empty() && reader.eof()) {
                        std::cout << "[gRPC CLIENT INFO] (Writer): Reached EOF from ChunkReader." << std::endl;
                        break;
                    }
                    if (chunk_data_buffer.empty() && !reader.eof()){
                        std::string err_msg = "gRPC Client (Writer): Error reading chunk or empty chunk before EOF.";
                        std::cerr << "[gRPC CLIENT ERROR] " << err_msg << std::endl;
                        metrics_collector.log_error(err_msg);
                        writer_stream_broken = true; // Помечаем проблему
                        return;
                    }

                    client_chunk_id_counter++;

                    ChunkRequest request;
                    request.set_data_chunk(chunk_data_buffer.data(), chunk_data_buffer.size());
                    request.set_client_assigned_chunk_id(client_chunk_id_counter); // Отправляем ID клиента

                    SentChunkInfo log_entry;
                    log_entry.client_assigned_id = client_chunk_id_counter;
                    log_entry.original_payload_size = chunk_data_buffer.size();
                    log_entry.on_wire_request_size_bytes = request.ByteSizeLong();
                    log_entry.original_data_for_verification = chunk_data_buffer;
                    log_entry.time_sent = std::chrono::steady_clock::now();

                    if (client_chunk_id_counter % 500 == 0 || client_chunk_id_counter == 1) {
                         std::cout << "[CLIENT PROGRESS] gRPC (Writer): Sending client_id " << log_entry.client_assigned_id
                                   << ", size: " << log_entry.original_payload_size << std::endl;
                    }
                    // print_client_hex_data("Writer: Sending client_id " + std::to_string(log_entry.client_assigned_id), log_entry.original_data_for_verification);

                    if (!stream->Write(request)) {
                        std::cerr << "[gRPC CLIENT ERROR] (Writer): Failed to write to stream for client_id " << client_chunk_id_counter << "." << std::endl;
                        writer_stream_broken = true;
                        cv_can_send.notify_all();
                        break;
                    }
                    total_chunks_actually_sent_by_writer++;

                    {
                        std::lock_guard<std::mutex> lock(map_mutex);
                        inflight_requests_map[log_entry.client_assigned_id] = log_entry;
                    }
                }
            } catch (const std::exception& e) {
                 std::cerr << "[gRPC CLIENT ERROR] (Writer): Exception: " << e.what() << std::endl;
                 metrics_collector.log_error(std::string("Writer thread exception: ") + e.what());
                 writer_stream_broken = true;
            }

            // Поток писателя завершил отправку данных из файла
            writer_thread_finished_sending = true; // Устанавливаем флаг
            std::cout << "[gRPC CLIENT INFO] (Writer): Finished reading file. Total chunks prepared by writer: " << client_chunk_id_counter << std::endl;

            if (!writer_stream_broken.load()) {
                 std::cout << "[gRPC CLIENT INFO] (Writer): Calling WritesDone()." << std::endl;
                 if(!stream->WritesDone()){
                    std::cerr << "[gRPC CLIENT ERROR] (Writer): WritesDone() failed." << std::endl;
                    // Это может произойти, если читатель уже сломал поток или сервер закрыл его.
                 } else {
                    std::cout << "[gRPC CLIENT INFO] (Writer): WritesDone() successful." << std::endl;
                 }
            }
            cv_can_send.notify_all(); // Разбудить читателя, если он ждет, и сообщить, что больше данных не будет
        });


        ChunkResponse response;
        size_t received_responses_count = 0;
        size_t total_bytes_verified_payload_by_reader = 0;

        try {
            while (stream->Read(&response)) {
                auto chunk_received_time = std::chrono::steady_clock::now();
                long long server_echoed_client_id_long = response.original_client_chunk_id();
                size_t server_echoed_client_id = static_cast<size_t>(server_echoed_client_id_long);

                SentChunkInfo request_log_entry;
                bool found_request_in_map = false;
                {
                    std::lock_guard<std::mutex> lock(map_mutex);
                    auto it = inflight_requests_map.find(server_echoed_client_id);
                    if (it != inflight_requests_map.end()) {
                        request_log_entry = it->second; // Копируем данные
                        inflight_requests_map.erase(it); // Удаляем из map
                        found_request_in_map = true;
                    } else {
                        std::string warn_msg = "gRPC Client (Reader): Warning - received response for client_id "
                                             + std::to_string(server_echoed_client_id)
                                             + ", but no such request was in flight map. Processed responses: "
                                             + std::to_string(received_responses_count)
                                             + ". Map size: " + std::to_string(inflight_requests_map.size());
                        std::cerr << "[gRPC CLIENT WARNING] " << warn_msg << std::endl;
                        metrics_collector.log_error(warn_msg);
                        // Не делаем continue, чтобы не нарушить подсчет received_responses_count,
                        // но и не обрабатываем этот "лишний" ответ дальше.
                        // Однако, это может привести к тому, что writer_thread_finished_sending будет true,
                        // а map не пуста, и цикл Read будет продолжаться.
                        // Если это происходит часто, это серьезная проблема.
                    }
                }
                if (found_request_in_map) { // Только если нашли соответствующий запрос
                    cv_can_send.notify_one(); // Сообщаем писателю, что место в map освободилось
                    received_responses_count++;

                    auto rtt_us = std::chrono::duration_cast<std::chrono::microseconds>(chunk_received_time - request_log_entry.time_sent);
                    metrics_collector.record_chunk_rtt_us(rtt_us.count());
                    metrics_collector.record_chunk_sent(request_log_entry.original_payload_size, request_log_entry.on_wire_request_size_bytes);

                    std::vector<char> expected_reversed_chunk = request_log_entry.original_data_for_verification;
                    benchmark_common::reverse_bytes(expected_reversed_chunk);

                    std::string received_data_str = response.reversed_chunk_data();
                    std::vector<char> received_chunk_vec(received_data_str.begin(), received_data_str.end());

                    if (received_chunk_vec.size() != expected_reversed_chunk.size()) {
                        std::string err_msg = "VERIFICATION FAILED for client_id " + std::to_string(request_log_entry.client_assigned_id)
                                           + ". Size mismatch. Expected " + std::to_string(expected_reversed_chunk.size())
                                           + ", got " + std::to_string(received_chunk_vec.size());
                        std::cerr << "[gRPC CLIENT ERROR] " << err_msg << std::endl;
                        metrics_collector.log_error(err_msg);
                    } else {
                        if (received_chunk_vec != expected_reversed_chunk) {
                            std::string err_msg = "VERIFICATION FAILED for client_id " + std::to_string(request_log_entry.client_assigned_id) + ". Content mismatch.";
                            std::cerr << "[gRPC CLIENT ERROR] " << err_msg << std::endl;
                            metrics_collector.log_error(err_msg);

                            std::cout << "==== ERROR DEBUG CLIENT_ID: " << request_log_entry.client_assigned_id << " ====" << std::endl;
                            print_client_hex_data("Original Data", request_log_entry.original_data_for_verification, 64);
                            print_client_hex_data("Expected Reversed", expected_reversed_chunk, 64);
                            print_client_hex_data("Received Reversed", received_chunk_vec, 64);
                            std::cout << "====================================" << std::endl;
                        } else {
                            total_bytes_verified_payload_by_reader += request_log_entry.original_payload_size;
                        }
                    }
                    if (received_responses_count % 500 == 0 || received_responses_count == 1 || received_responses_count == total_chunks_actually_sent_by_writer.load()) {
                         std::cout << "[CLIENT PROGRESS] gRPC: Processed " << received_responses_count << " of " << total_chunks_actually_sent_by_writer.load()
                                   << " sent responses. Verified "
                                   << std::fixed << std::setprecision(2) << (total_bytes_verified_payload_by_reader / (1024.0*1024.0))
                                   << " MB. Last RTT: " << rtt_us.count() << " us." << std::endl;
                    }
                } else { // Не нашли запрос в map
                    // Если writer уже закончил и карта пуста, это может быть нормально, если Read() вернул false после этого
                    // Но если Read() все еще возвращает true, это проблема.
                    if (!writer_thread_finished_sending.load()) {
                         std::cerr << "[gRPC CLIENT ERROR] (Reader): Received a response for client_id " << server_echoed_client_id
                                   << " but it was not in the inflight map, and writer is NOT finished." << std::endl;
                    }
                }
            } // Конец while (stream->Read(&response))
        } catch (const std::exception& e) {
             std::cerr << "[gRPC CLIENT ERROR] (Reader): Exception during Read loop: " << e.what() << std::endl;
             metrics_collector.log_error(std::string("Exception in Read loop: ") + e.what());
        }
        std::cout << "[gRPC CLIENT INFO] (Reader): Read loop finished or broken." << std::endl;

        // Убеждаемся, что writer_thread завершился
        if (writer_thread.joinable()) {
            writer_thread.join();
        }
        std::cout << "[gRPC CLIENT INFO] Writer thread joined." << std::endl;

        // Проверяем, остались ли необработанные запросы в map (не должно быть, если все ответы пришли)
        {
            std::lock_guard<std::mutex> lock(map_mutex);
            if (!inflight_requests_map.empty()) {
                std::string warn_msg = "gRPC Client: Warning - " + std::to_string(inflight_requests_map.size())
                                     + " requests remaining in flight map after stream completion.";
                std::cerr << "[gRPC CLIENT WARNING] " << warn_msg << std::endl;
                metrics_collector.log_error(warn_msg);
                for (const auto& pair : inflight_requests_map) {
                    std::cerr << "  - Unanswered client_id: " << pair.first << std::endl;
                }
            }
        }


        Status status = stream->Finish();
        auto overall_processing_end_time = std::chrono::steady_clock::now();

        auto total_duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(overall_processing_end_time - overall_processing_start_time);
        metrics_collector.set_total_transaction_time_ms(total_duration_ms.count());

        if (status.ok()) {
            std::cout << "[gRPC CLIENT INFO] Transaction finished successfully (status OK)." << std::endl;
        } else {
            std::string err_msg = "gRPC Client: RPC failed. Final Status - Error code: " + std::to_string(status.error_code()) +
                                  ", message: " + status.error_message();
            std::cerr << "[gRPC CLIENT ERROR] " << err_msg << std::endl;
            metrics_collector.log_error(err_msg);
        }

        std::cout << "[gRPC CLIENT SUMMARY] Total chunks prepared by writer: " << total_chunks_actually_sent_by_writer.load() << std::endl;
        std::cout << "[gRPC CLIENT SUMMARY] Total responses processed by reader: " << received_responses_count << std::endl;
        std::cout << "[gRPC CLIENT SUMMARY] Total bytes (payload) verified by reader: " << total_bytes_verified_payload_by_reader << std::endl;

        if (writer_thread_finished_sending.load() && !writer_stream_broken.load() && status.ok() && // <--- ИЗМЕНЕНО ЗДЕСЬ
            total_bytes_verified_payload_by_reader != benchmark_common::ACTUAL_FILE_SIZE_BYTES ) {
             std::string final_warn = "Potential data loss or incomplete processing: Verified bytes (" +
                                     std::to_string(total_bytes_verified_payload_by_reader) +
                                     ") != Expected total bytes (" + std::to_string(benchmark_common::ACTUAL_FILE_SIZE_BYTES) + ")";
             std::cerr << "[gRPC CLIENT WARNING] " << final_warn << std::endl;
             metrics_collector.log_error(final_warn);
        } else if (writer_thread_finished_sending.load() && !writer_stream_broken.load() && status.ok() && // <--- И ИЗМЕНЕНО ЗДЕСЬ
                   total_bytes_verified_payload_by_reader == benchmark_common::ACTUAL_FILE_SIZE_BYTES) {
            std::cout << "[gRPC CLIENT INFO] All data successfully transferred and verified!" << std::endl;
        }
    }

private:
    std::unique_ptr<FileProcessor::Stub> stub_;
};

int main(int argc, char** argv) {
    UNUSED_PARAM(argc);
    UNUSED_PARAM(argv);

    std::cout << "[gRPC CLIENT INFO] Starting gRPC client." << std::endl;

    const std::string test_filename = benchmark_common::TEST_FILE_NAME;
    const size_t target_file_size_bytes = benchmark_common::ACTUAL_FILE_SIZE_BYTES;
    const size_t chunk_size_bytes = benchmark_common::CHUNK_SIZE_BYTES;
    const std::string server_target_address = benchmark_common::GRPC_SERVER_ADDRESS + ":" + std::to_string(benchmark_common::GRPC_SERVER_PORT);
    const std::string csv_file_prefix = benchmark_common::CSV_OUTPUT_FILE_PREFIX;

    bool generate_new_file = true;
    std::ifstream test_file_check_stream(test_filename, std::ios::binary | std::ios::ate);
    if (test_file_check_stream.is_open()) {
        size_t existing_file_size = test_file_check_stream.tellg();
        test_file_check_stream.close();
        if (existing_file_size == target_file_size_bytes) {
            std::cout << "[gRPC CLIENT INFO] Test file '" << test_filename
                      << "' already exists with correct size (" << existing_file_size << " bytes). Skipping generation." << std::endl;
            generate_new_file = false;
        } else {
            std::cout << "[gRPC CLIENT INFO] Test file '" << test_filename
                      << "' exists but has incorrect size (" << existing_file_size
                      << " vs " << target_file_size_bytes << "). Regenerating." << std::endl;
        }
    }  else {
         std::cout << "[gRPC CLIENT INFO] Test file '" << test_filename
                   << "' does not exist. Generating." << std::endl;
    }

    if (generate_new_file) {
        if (!benchmark_common::generate_test_file(test_filename, target_file_size_bytes)) {
            std::cerr << "[gRPC CLIENT ERROR] Failed to generate test file. Exiting." << std::endl;
            return 1;
        }
    }

    benchmark_common::MetricsAggregator metrics(
        "gRPC",
        target_file_size_bytes,
        chunk_size_bytes
    );

    ChannelArguments ch_args;
    ch_args.SetMaxReceiveMessageSize(-1);
    ch_args.SetMaxSendMessageSize(-1);
    ch_args.SetInt(GRPC_ARG_KEEPALIVE_TIME_MS, 20000);
    ch_args.SetInt(GRPC_ARG_KEEPALIVE_TIMEOUT_MS, 10000);
    ch_args.SetInt(GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS, 1);
    ch_args.SetInt(GRPC_ARG_HTTP2_MAX_PINGS_WITHOUT_DATA, 0);
    ch_args.SetInt(GRPC_ARG_HTTP2_MIN_RECV_PING_INTERVAL_WITHOUT_DATA_MS, 10000);

    std::shared_ptr<Channel> channel = grpc::CreateCustomChannel(
        server_target_address, grpc::InsecureChannelCredentials(), ch_args);

    std::cout << "[gRPC CLIENT INFO] Attempting to connect to " << server_target_address << std::endl;

    GrpcFileClient grpc_client_instance(channel);

    try {
        grpc_client_instance.ProcessFile(
            test_filename,
            chunk_size_bytes,
            metrics
        );
    } catch (const std::exception& e) {
        std::string error_msg = std::string("gRPC Client (main): Exception caught: ") + e.what();
        std::cerr << "[gRPC CLIENT ERROR] " << error_msg << std::endl;
        metrics.log_error(error_msg);
    } catch (...) {
        std::string error_msg = "gRPC Client (main): Unknown exception caught.";
        std::cerr << "[gRPC CLIENT ERROR] " << error_msg << std::endl;
        metrics.log_error(error_msg);
    }

    metrics.print_summary_to_console();
    metrics.save_summary_csv(csv_file_prefix + "grpc_summary.csv");
    metrics.save_detailed_rtt_csv(csv_file_prefix + "grpc_detailed_rtt.csv");

    std::cout << "[gRPC CLIENT INFO] gRPC client finished." << std::endl;
    return 0;
}
