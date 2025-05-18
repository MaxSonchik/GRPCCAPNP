// common/include/config.hpp
#pragma once

#include <string>
#include <cstddef> // Для size_t

namespace benchmark_common {

// --- Общие настройки бенчмарка ---
const std::string TEST_FILE_NAME = "test_file.dat";
const size_t TEST_FILE_SIZE_GB = 10;
const size_t ACTUAL_FILE_SIZE_BYTES =TEST_FILE_SIZE_GB * 1024 * 1024 * 1024;
const size_t CHUNK_SIZE_BYTES = 64 * 1024;

// ВОТ ЭТА КОНСТАНТА ДОЛЖНА БЫТЬ ТАКОЙ:
const std::string CSV_OUTPUT_FILE_PREFIX = "benchmark"; // <--- ПРОВЕРЬТЕ ЭТО ИМЯ

// --- Настройки сервера gRPC ---
const std::string GRPC_SERVER_ADDRESS = "212.67.17.60";
const int GRPC_SERVER_PORT = 50051;

// --- Настройки сервера Cap'n Proto ---
const std::string CAPNP_SERVER_ADDRESS = "0.0.0.0";
const int CAPNP_SERVER_PORT = 50052;
const std::string CAPNP_CLIENT_CONNECT_TO ="212.67.17.60";

// --- Настройки клиента ---
const std::string TARGET_SERVER_IP = "212.67.17.60";


// --- Настройки для метрик ---
enum class Protocol {
    GRPC,
    CAPNP
};

inline std::string protocol_to_string(Protocol p) {
    switch (p) {
        case Protocol::GRPC: return "gRPC";
        case Protocol::CAPNP: return "Cap'nProto";
        default: return "Unknown";
    }
}

#ifndef UNUSED_PARAM
#define UNUSED_PARAM(x) (void)(x)
#endif

} // namespace benchmark_common
