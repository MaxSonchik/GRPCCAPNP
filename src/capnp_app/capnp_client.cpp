// capnp_app/capnp_client.cpp
#include <iostream>
#include <vector>
#include <string>
#include <fstream>   // Для std::ifstream для проверки файла
#include <algorithm> // Для std::equal
#include <chrono>    // Для замера времени
#include <iomanip>   // Для std::fixed, std::setprecision при выводе метрик вручную

// KJ includes
#include <kj/async-io.h>
#include <kj/memory.h>    // Для kj::mv, kj::ArrayPtr
#include <kj/exception.h> // Для kj::Exception
// #include <kj/debug.h>  // Используем std::cout/cerr для простоты

// Cap'n Proto includes
#include <capnp/rpc-twoparty.h> // Для TwoPartyClient
#include <capnp/capability.h>   // Для FileProcessor::Client

// Проектные includes
#include "benchmark.capnp.h"    // Сгенерированный код
#include "common/include/config.hpp"
#include "common/include/reversal_utils.hpp"
#include "common/include/file_utils.hpp"
#include "common/include/metrics_aggregator.hpp" // Включаем, но используем осторожно

#ifndef UNUSED_PARAM
#define UNUSED_PARAM(x) (void)(x)
#endif

int main(int argc, char* argv[]) {
    UNUSED_PARAM(argc);
    UNUSED_PARAM(argv);

    std::cout << "[CLIENT INFO] Starting Cap'n Proto client." << std::endl;

    // --- Конфигурация ---
    const std::string test_filename = benchmark_common::TEST_FILE_NAME;
    const size_t target_file_size_bytes = benchmark_common::ACTUAL_FILE_SIZE_BYTES;
    const size_t chunk_size_bytes = benchmark_common::CHUNK_SIZE_BYTES;
    const std::string server_connect_to = benchmark_common::CAPNP_CLIENT_CONNECT_TO;
    const int server_port = benchmark_common::CAPNP_SERVER_PORT;

    // --- Инициализация MetricsAggregator ---
    benchmark_common::MetricsAggregator metrics(
        "CapnProto",
        target_file_size_bytes,
        chunk_size_bytes
    );

bool regenerate_new_file = true; // ИСПРАВЛЕНО ИМЯ ПЕРЕМЕННОЙ
    std::ifstream test_file_check(test_filename, std::ios::binary | std::ios::ate);
    if (test_file_check.is_open()) {
        size_t existing_file_size = test_file_check.tellg();
        test_file_check.close();
        if (existing_file_size == target_file_size_bytes) {
            std::cout << "[CLIENT INFO] Test file '" << test_filename
                      << "' already exists with correct size. Skipping generation." << std::endl;
            regenerate_new_file = false; // ИСПРАВЛЕНО ИМЯ ПЕРЕМЕННОЙ
        } else {
            std::cout << "[CLIENT INFO] Test file '" << test_filename
                      << "' exists but has incorrect size (" << existing_file_size
                      << " vs " << target_file_size_bytes << "). Regenerating." << std::endl;
        }
    } else {
         std::cout << "[CLIENT INFO] Test file '" << test_filename
                   << "' does not exist. Generating." << std::endl;
    }

    if (regenerate_new_file) { // ИСПРАВЛЕНО ИМЯ ПЕРЕМЕННОЙ
        if (!benchmark_common::generate_test_file(test_filename, target_file_size_bytes)) {
            std::cerr << "[CLIENT ERROR] Failed to generate test file '" << test_filename << "'. Exiting." << std::endl;
            metrics.log_error("Failed to generate test file");
            // ... (сохранение метрик при ошибке, если нужно) ...
            return 1;
        }
    }
    // --- Конец генерации файла ---

    std::string server_address_str = server_connect_to + ":" + std::to_string(server_port);
    std::cout << "[CLIENT INFO] Will connect to " << server_address_str << std::endl;

    try {
        kj::AsyncIoContext ioContext = kj::setupAsyncIo();
        kj::Network& network = ioContext.provider->getNetwork();
        kj::WaitScope& waitScope = ioContext.waitScope;

        std::cout << "[CLIENT DEBUG] Connecting to " << server_address_str << "..." << std::endl;
        kj::Own<kj::AsyncIoStream> stream = network.parseAddress(server_address_str)
                                                .then([&](kj::Own<kj::NetworkAddress> addr){
                                                    return addr->connect();
                                                }).wait(waitScope);
        std::cout << "[CLIENT DEBUG] Connected." << std::endl;

        capnp::TwoPartyClient client(*stream);
        FileProcessor::Client fileProcessor = client.bootstrap().castAs<FileProcessor>();
        std::cout << "[CLIENT DEBUG] Bootstrap interface obtained." << std::endl;

        std::cout << "[CLIENT DEBUG] Calling startStreaming..." << std::endl;
        auto ssRequest = fileProcessor.startStreamingRequest();
        auto ssResponse = ssRequest.send().wait(waitScope);
        FileProcessor::ChunkHandler::Client chunkHandler = ssResponse.getHandler();
        std::cout << "[CLIENT DEBUG] Got ChunkHandler." << std::endl;

        // --- Чтение и отправка файла по чанкам ---
        benchmark_common::ChunkReader reader(test_filename, chunk_size_bytes);
        // Открытие происходит в конструкторе ChunkReader в вашей реализации

        std::vector<char> chunk_buffer;
        size_t chunks_sent = 0;
        size_t total_bytes_verified_payload = 0; // Только полезная нагрузка

        auto overall_start_time = std::chrono::high_resolution_clock::now();

        while (true) {
            chunk_buffer = reader.next_chunk();
            if (chunk_buffer.empty() && reader.eof()) {
                break;
            }
            if (chunk_buffer.empty() && !reader.eof()) {
                std::string error_msg = "Read empty chunk but not EOF.";
                std::cerr << "[CLIENT ERROR] " << error_msg << " Aborting." << std::endl;
                metrics.log_error(error_msg);
                break;
            }

            size_t current_payload_size = chunk_buffer.size();
            size_t estimated_on_wire_for_chunk_data = current_payload_size; // УПРОЩЕНИЕ!
            metrics.record_chunk_sent(current_payload_size, estimated_on_wire_for_chunk_data);

            std::vector<char> expected_reversed_chunk = chunk_buffer;
            benchmark_common::reverse_bytes(expected_reversed_chunk);

            auto pcRequest = chunkHandler.processChunkRequest();
            kj::ArrayPtr<const kj::byte> request_bytes_ptr(reinterpret_cast<const kj::byte*>(chunk_buffer.data()), chunk_buffer.size());
            pcRequest.getRequest().setData(request_bytes_ptr);

            auto chunk_rtt_start_time = std::chrono::high_resolution_clock::now();
            auto pcPromise = pcRequest.send();
            auto pcResponse = pcPromise.wait(waitScope);
            auto chunk_rtt_end_time = std::chrono::high_resolution_clock::now();
            auto rtt_duration_us = std::chrono::duration_cast<std::chrono::microseconds>(chunk_rtt_end_time - chunk_rtt_start_time);
            metrics.record_chunk_rtt_us(rtt_duration_us.count());

            capnp::Data::Reader response_data_reader = pcResponse.getResponse().getData();
            if (response_data_reader.size() != expected_reversed_chunk.size()) {
                std::string error_msg = "Verification FAILED for chunk " + std::to_string(chunks_sent + 1)
                                      + ": Size mismatch. Expected " + std::to_string(expected_reversed_chunk.size())
                                      + ", Got " + std::to_string(response_data_reader.size());
                std::cerr << "[CLIENT ERROR] " << error_msg << std::endl;
                metrics.log_error(error_msg);
                break;
            }

            std::vector<char> received_reversed_chunk(response_data_reader.size());
            memcpy(received_reversed_chunk.data(), response_data_reader.begin(), response_data_reader.size());

            if (received_reversed_chunk != expected_reversed_chunk) {
                 std::string error_msg = "Verification FAILED for chunk " + std::to_string(chunks_sent + 1) + ": Content mismatch.";
                std::cerr << "[CLIENT ERROR] " << error_msg << std::endl;
                metrics.log_error(error_msg);
                break;
            }
            total_bytes_verified_payload += current_payload_size;
            chunks_sent++;
             if (chunks_sent % 500 == 0 || chunks_sent == 1) { // Логируем прогресс
                 std::cout << "[CLIENT PROGRESS] Processed " << chunks_sent << " chunks. Verified "
                           << std::fixed << std::setprecision(2) << (total_bytes_verified_payload / (1024.0*1024.0)) << " MB. Last RTT: "
                           << rtt_duration_us.count() << " us." << std::endl;
             }
        }

        auto overall_end_time = std::chrono::high_resolution_clock::now();
        auto total_duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(overall_end_time - overall_start_time);
        metrics.set_total_transaction_time_ms(total_duration_ms.count());

        std::cout << "[CLIENT DEBUG] Calling doneStreaming..." << std::endl;
        auto doneRequest = chunkHandler.doneStreamingRequest();
        doneRequest.send().wait(waitScope);
        std::cout << "[CLIENT DEBUG] doneStreaming completed." << std::endl;

        std::cout << "[CLIENT INFO] File transfer processing finished by client." << std::endl;

        if (total_bytes_verified_payload != target_file_size_bytes) {
             std::string warn_msg = "Not all data was verified! Verified (payload): " + std::to_string(total_bytes_verified_payload) +
                                   " Expected: " + std::to_string(target_file_size_bytes);
             std::cerr << "[CLIENT WARNING] " << warn_msg << std::endl;
             metrics.log_error(warn_msg);
        } else {
             std::cout << "[CLIENT INFO] All data successfully processed and verified by client." << std::endl;
        }

    } catch (const kj::Exception& e) {
        std::string error_msg = std::string("KJ Exception: ") + e.getDescription().cStr();
        std::cerr << "[CLIENT ERROR] " << error_msg << std::endl;
        metrics.log_error(error_msg);
        // Вывод метрик даже при ошибке
        metrics.print_summary_to_console();
        metrics.save_summary_csv("capnp_summary_results_kj_error.csv");
        metrics.save_detailed_rtt_csv("capnp_detailed_rtt_kj_error.csv");
        return 1;
    } catch (const std::exception& e) {
        std::string error_msg = std::string("STD Exception: ") + e.what();
        std::cerr << "[CLIENT ERROR] " << error_msg << std::endl;
        metrics.log_error(error_msg);
        metrics.print_summary_to_console();
        metrics.save_summary_csv("capnp_summary_results_std_error.csv");
        metrics.save_detailed_rtt_csv("capnp_detailed_rtt_std_error.csv");
        return 1;
    } catch (...) {
        std::string error_msg = "Unknown exception.";
        std::cerr << "[CLIENT ERROR] " << error_msg << std::endl;
        metrics.log_error(error_msg);
        metrics.print_summary_to_console();
        metrics.save_summary_csv("capnp_summary_results_unknown_error.csv");
        metrics.save_detailed_rtt_csv("capnp_detailed_rtt_unknown_error.csv");
        return 1;
    }

    // Вывод и сохранение метрик при успешном завершении
    metrics.print_summary_to_console();
    metrics.save_summary_csv("capnp_summary_results.csv");
    metrics.save_detailed_rtt_csv("capnp_detailed_rtt_results.csv");

    std::cout << "[CLIENT INFO] Client finished successfully." << std::endl;
    return 0;
}
