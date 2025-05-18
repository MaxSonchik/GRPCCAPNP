// grpc_app/grpc_server.cpp
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <thread>     // Не используется в этом простом сервере, но может быть в будущем
#include <algorithm>  // Для std::reverse (если бы использовался напрямую)
#include <iomanip>    // Для std::hex, std::setw, std::setfill (для отладочного вывода)

#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
// Попытка включить заголовок для старого API рефлексии
// Если этот файл не найден, или класс ProtoServerReflectionPlugin не найден,
// то ваша версия gRPC может использовать другой механизм.
#include <grpcpp/ext/proto_server_reflection_plugin.h>

// Сгенерированный код (путь от корня проекта)
#include "gen_proto/benchmark.grpc.pb.h"

// Общие утилиты (пути от корня проекта)
#include "common/include/config.hpp"
#include "common/include/reversal_utils.hpp"

#ifndef UNUSED_PARAM
#define UNUSED_PARAM(x) (void)(x)
#endif

// Используем пространства имен из сгенерированного proto файла и gRPC
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReaderWriter;
using grpc::Status;
using benchmark_grpc::ChunkRequest;
using benchmark_grpc::ChunkResponse;
using benchmark_grpc::FileProcessor;

// Вспомогательная функция для вывода HEX-дампа (для отладки на сервере)
void print_server_hex_data_debug(const std::string& title, const std::vector<char>& data, size_t count = 32) {
    // Закомментировано по умолчанию, чтобы не засорять вывод, если не нужно
    /*
    std::cout << "[SERVER DEBUG HEX] " << title << " (first " << std::min(count, data.size()) << " of " << data.size() << " bytes): ";
    for (size_t i = 0; i < std::min(count, data.size()); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(static_cast<unsigned char>(data[i])) << " ";
    }
    std::cout << std::dec << std::endl;
    */
}

// Реализация сервиса FileProcessor
class FileProcessorServiceImpl final : public FileProcessor::Service {
public:
    Status ProcessFileChunks(ServerContext* context,
                             ServerReaderWriter<ChunkResponse, ChunkRequest>* stream) override {
        UNUSED_PARAM(context); // Контекст может использоваться для метаданных, отмены и т.д.

        std::cout << "[gRPC SERVER INFO] Client connection established. Starting to process chunks." << std::endl;
        ChunkRequest request;
        long long server_processed_chunk_count = 0;

        // Цикл чтения запросов от клиента
        while (stream->Read(&request)) {
            // server_processed_chunk_count++; // Этот счетчик теперь не так важен для ответа

            long long client_id_from_request = request.client_assigned_chunk_id(); // Получаем ID от клиента

            if (server_processed_chunk_count % 500 == 0 || server_processed_chunk_count == 1 || client_id_from_request % 500 == 0) {
                 std::cout << "[gRPC SERVER PROGRESS] Received client_id: " << client_id_from_request
                           << ", size: " << request.data_chunk().length() << " bytes." << std::endl;
            }
            server_processed_chunk_count++; // Все еще считаем для логов сервера


            const std::string& chunk_str_data = request.data_chunk();
            std::vector<char> chunk_vec_data(chunk_str_data.begin(), chunk_str_data.end());
            benchmark_common::reverse_bytes(chunk_vec_data);

            ChunkResponse response;
            response.set_reversed_chunk_data(chunk_vec_data.data(), chunk_vec_data.size());
            response.set_original_client_chunk_id(client_id_from_request); // Возвращаем ID клиента
            // Отправка ответа клиенту
            if (!stream->Write(response)) {
                std::cerr << "[gRPC SERVER ERROR] Failed to write response to stream for server_id " << server_processed_chunk_count << "." << std::endl;
                return Status(grpc::StatusCode::UNKNOWN, "Server failed to write response to stream.");
            }
        }

        std::cout << "[gRPC SERVER INFO] Client finished streaming or stream broken. Total chunks processed in this session: " << server_processed_chunk_count << "." << std::endl;
        return Status::OK; // Сигнализируем об успешном завершении RPC
    }
};

// Функция запуска сервера
void RunServer() {
    std::string server_address = benchmark_common::GRPC_SERVER_ADDRESS + ":" + std::to_string(benchmark_common::GRPC_SERVER_PORT);
    FileProcessorServiceImpl service_impl; // Экземпляр нашей реализации сервиса

    // Включаем стандартный сервис проверки состояния (health checking)
    grpc::EnableDefaultHealthCheckService(true);

    // --- Попытка включения серверной рефлексии (самый простой способ) ---
    // Создаем статический экземпляр плагина рефлексии.
    // В некоторых версиях gRPC этого достаточно для активации.
    // Если этот класс не найден или вызывает ошибку, значит, API другой.
    static grpc::reflection::ProtoServerReflectionPlugin reflection_plugin_instance;
    UNUSED_PARAM(reflection_plugin_instance); // Чтобы избежать предупреждения о неиспользуемой переменной, если она ничего не делает явно

    // Используем ServerBuilder для конфигурации и запуска сервера
    ServerBuilder builder;

    // Устанавливаем максимальные размеры принимаемых и отправляемых сообщений
    // -1 означает отсутствие ограничений (или очень большое значение по умолчанию)
    builder.SetMaxReceiveMessageSize(-1);
    builder.SetMaxSendMessageSize(-1);

    // Добавляем порт для прослушивания без шифрования
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());

    // Регистрируем нашу реализацию сервиса
    builder.RegisterService(&service_impl);

    // Собираем и запускаем сервер
    std::unique_ptr<Server> server(builder.BuildAndStart());

    if (!server) {
        std::cerr << "[gRPC SERVER ERROR] Failed to build or start server on " << server_address << "." << std::endl;
        return;
    }
    std::cout << "[gRPC SERVER INFO] Server listening on " << server_address << "." << std::endl;
    std::cout << "[gRPC SERVER INFO] Reflection service hopefully enabled (via static plugin instance)." << std::endl;

    // Ожидаем завершения работы сервера (блокирующий вызов)
    // Сервер будет работать, пока его не остановят (например, Ctrl+C)
    server->Wait();
}

int main(int argc, char** argv) {
    UNUSED_PARAM(argc);
    UNUSED_PARAM(argv);

    std::cout << "[gRPC SERVER INFO] Server process starting..." << std::endl;
    RunServer(); // Запускаем сервер
    std::cout << "[gRPC SERVER INFO] Server process shut down." << std::endl;
    return 0;
}
