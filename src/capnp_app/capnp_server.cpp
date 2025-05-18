// capnp_app/capnp_server.cpp
#include <iostream>
#include <vector>

// KJ includes
#include <kj/async-io.h>     // Для AsyncIoContext, Network, AsyncIoStream, ConnectionReceiver
#include <kj/async.h>        // Для Promise, TaskSet (часто включает tasks.h), evalLater, NEVER_DONE
#include <kj/debug.h>        // Для KJ_LOG
#include <kj/memory.h>       // Для kj::heap, kj::Own
#include <kj/exception.h>    // Для kj::Exception, kj::jedoch (хотя мы перешли на throw)

// Cap'n Proto includes
#include <capnp/capability.h>   // Для Capability::Server, Capability::Client
#include <capnp/rpc.h>          // Для RpcSystem (если бы мы использовали его напрямую по-старому)
#include <capnp/rpc-twoparty.h> // Для TwoPartyVatNetwork, rpc::twoparty::Side, rpc::twoparty::VatId

// Сгенерированный код и общие утилиты
#include "benchmark.capnp.h" // Убедитесь, что #include "benchmark.capnp.h", а не "gen_capnp/..."
#include "common/include/config.hpp"
#include "common/include/reversal_utils.hpp"

#ifndef UNUSED_PARAM
#define UNUSED_PARAM(x) (void)(x)
#endif

// --- Реализации интерфейсов Cap'n Proto ---
class ChunkHandlerImpl final : public FileProcessor::ChunkHandler::Server {
public:
    kj::Promise<void> processChunk(ProcessChunkContext context) override {
        KJ_LOG(INFO, "Cap'n Proto Server: processChunk called.");
        try {
            capnp::Data::Reader request_data = context.getParams().getRequest().getData();
            std::vector<char> chunk_vec(request_data.size());
            if (!chunk_vec.empty()) {
                memcpy(chunk_vec.data(), request_data.begin(), request_data.size());
            }

            KJ_LOG(INFO, "Cap'n Proto Server: Received chunk of size: ", chunk_vec.size());
            benchmark_common::reverse_bytes(chunk_vec);
            KJ_LOG(INFO, "Cap'n Proto Server: Reversed chunk.");

            auto results = context.getResults();
            auto response_builder = results.initResponse();
            response_builder.setData(kj::arrayPtr(reinterpret_cast<const kj::byte*>(chunk_vec.data()), chunk_vec.size()));
            KJ_LOG(INFO, "Cap'n Proto Server: Sending reversed chunk of size: ", chunk_vec.size());
        } catch (const kj::Exception& e) {
            KJ_LOG(ERROR, "Cap'n Proto Server: Exception in processChunk: ", e.getDescription().cStr());
            throw; // Перевыбрасываем исключение, KJ Promise API это обработает
        }
        return kj::READY_NOW;
    }

    kj::Promise<void> doneStreaming(DoneStreamingContext context) override {
        UNUSED_PARAM(context);
        KJ_LOG(INFO, "Cap'n Proto Server: doneStreaming called by client.");
        return kj::READY_NOW;
    }
};

class FileProcessorImpl final : public FileProcessor::Server {
public:
    kj::Promise<void> startStreaming(StartStreamingContext context) override {
        KJ_LOG(INFO, "Cap'n Proto Server: startStreaming called.");
        // Создаем новый экземпляр ChunkHandler для каждого вызова startStreaming
        FileProcessor::ChunkHandler::Client handler_capability = kj::heap<ChunkHandlerImpl>();
        context.getResults().setHandler(handler_capability);
        return kj::READY_NOW;
    }
};

// Простой обработчик ошибок для TaskSet, который логирует ошибки
struct LoggingTaskErrorHandler final : public kj::TaskSet::ErrorHandler {
    void taskFailed(kj::Exception&& exception) override {
        KJ_LOG(ERROR, "Cap'n Proto Server: Unhandled exception in a Task: ", exception.getDescription().cStr());
        // В реальном приложении здесь может быть более сложная логика.
        // Для бенчмарка просто логируем. Можно было бы вызвать kj::throwFatalException(),
        // если ошибка в задаче должна останавливать весь сервер.
    }
};

int main(int argc, char* argv[]) {

    UNUSED_PARAM(argc);
    UNUSED_PARAM(argv);

    std::cout << "[DEBUG] Server main: Program started." << std::endl;

    std::string bind_address_str = benchmark_common::CAPNP_SERVER_ADDRESS + ":" + std::to_string(benchmark_common::CAPNP_SERVER_PORT);
    if (benchmark_common::CAPNP_SERVER_ADDRESS == "0.0.0.0") {
        bind_address_str = "*:" + std::to_string(benchmark_common::CAPNP_SERVER_PORT);
    }
    std::cout << "[DEBUG] Server main: Bind address configured: " << bind_address_str << std::endl;

    try { // Внешний try-catch для инициализации
        std::cout << "[DEBUG] Server main: Entering outer try block." << std::endl;

        kj::AsyncIoContext ioContext = kj::setupAsyncIo();
        std::cout << "[DEBUG] Server main: kj::setupAsyncIo() done." << std::endl;
        kj::Network& network = ioContext.provider->getNetwork();
        std::cout << "[DEBUG] Server main: getNetwork() done." << std::endl;
        kj::WaitScope& waitScope = ioContext.waitScope;
        std::cout << "[DEBUG] Server main: getWaitScope() done." << std::endl;

        kj::Own<kj::NetworkAddress> addr = network.parseAddress(bind_address_str).wait(waitScope);
        std::cout << "[DEBUG] Server main: parseAddress().wait() done." << std::endl;
        kj::Own<kj::ConnectionReceiver> listener = addr->listen();
        std::cout << "[DEBUG] Server main: listen() done." << std::endl;

        KJ_LOG(INFO, "Cap'n Proto Server listening on ", bind_address_str);
        std::cout << "[DEBUG] Server main: KJ_LOG for listening executed." << std::endl;

        LoggingTaskErrorHandler taskErrorHandler;
        kj::TaskSet tasks(taskErrorHandler);

        std::cout << "[DEBUG] Server main: Entering while(true) loop." << std::endl;
        while (true) {
            KJ_LOG(INFO, "Cap'n Proto Server: Waiting for a new connection...");
            std::cout << "[DEBUG] Server loop: Waiting for accept()." << std::endl;

            kj::Own<kj::AsyncIoStream> current_connection_owner; // Объявляем здесь
            try {
                current_connection_owner = listener->accept().wait(waitScope);
            } catch (const kj::Exception& e) {
                KJ_LOG(ERROR, "Exception in accept(): ", e.getDescription().cStr());
                std::cerr << "[ERROR] Server loop: KJ Exception in accept(): " << e.getDescription().cStr() << std::endl;
                // Решаем, что делать: продолжить цикл или выйти?
                // Для простоты, если accept падает, серверу, вероятно, конец.
                break; // Выходим из while(true)
            } catch (const std::exception& e) {
                std::cerr << "[ERROR] Server loop: STD Exception in accept(): " << e.what() << std::endl;
                break; // Выходим из while(true)
            } catch (...) {
                std::cerr << "[ERROR] Server loop: Unknown exception in accept()." << std::endl;
                break; // Выходим из while(true)
            }

            // Если мы здесь, accept() прошел успешно
            if (!current_connection_owner) { // Дополнительная проверка, хотя accept() должен либо вернуть Own, либо кинуть исключение
                 KJ_LOG(ERROR, "Accept returned null connection without exception, exiting loop.");
                 std::cerr << "[ERROR] Server loop: Accept returned null connection, exiting." << std::endl;
                 break;
            }

            KJ_LOG(INFO, "Cap'n Proto Server: Accepted connection with a client.");
            std::cout << "[DEBUG] Server loop: Accepted connection." << std::endl;

            // Лямбда для обработки соединения
            auto handleConnectionLambda = [&ioContext, // Захватываем ioContext для EventLoop (если понадобится)
                                           conn_param = kj::mv(current_connection_owner) // ЗАХВАТЫВАЕМ ПО ЗНАЧЕНИЮ (перемещаем)
                                          ]() mutable -> kj::Promise<void> {
                // Владение соединением теперь у conn_param (переименовал для ясности)
                kj::Own<kj::AsyncIoStream> conn = kj::mv(conn_param);

                KJ_LOG(INFO, "Task started for a connection.");
                std::cout << "[DEBUG] Task: Started for connection." << std::endl;


                auto vatNetwork = kj::heap<capnp::TwoPartyVatNetwork>(
                    *conn,
                    capnp::rpc::twoparty::Side::SERVER,
                    capnp::ReaderOptions()
                );
                KJ_LOG(INFO, "Task: VatNetwork created.");
                std::cout << "[DEBUG] Task: VatNetwork created." << std::endl;


                FileProcessor::Client serviceImpl = kj::heap<FileProcessorImpl>();
                KJ_LOG(INFO, "Task: ServiceImpl created.");
                std::cout << "[DEBUG] Task: ServiceImpl created." << std::endl;


                auto rpcSystem = kj::heap<capnp::RpcSystem<capnp::rpc::twoparty::VatId>>(
                    *vatNetwork,
                    kj::Maybe<capnp::Capability::Client>(kj::mv(serviceImpl))
                );
                KJ_LOG(INFO, "Task: RpcSystem created.");
                std::cout << "[DEBUG] Task: RpcSystem created." << std::endl;


                kj::Promise<void> disconnectPromise = vatNetwork->onDisconnect();
                KJ_LOG(INFO, "Task: onDisconnect promise obtained.");
                std::cout << "[DEBUG] Task: onDisconnect promise obtained." << std::endl;


                return disconnectPromise.attach(kj::mv(conn), kj::mv(vatNetwork), kj::mv(rpcSystem))
                    .then(
                        [](){ KJ_LOG(INFO, "Task: disconnected cleanly."); std::cout << "[DEBUG] Task: disconnected cleanly." << std::endl; },
                        [](kj::Exception&& e){ KJ_LOG(ERROR, "Task: disconnected with error: ", e.getDescription().cStr()); std::cout << "[ERROR] Task: disconnected with error: " << e.getDescription().cStr() << std::endl; }
                    );
            };

            tasks.add(handleConnectionLambda());
            KJ_LOG(INFO, "Cap'n Proto Server: RPC task for new client added to TaskSet. Ready for next client.");
            std::cout << "[DEBUG] Server loop: Task added. Back to waiting for accept()." << std::endl;
        } // конец while(true)
        std::cout << "[DEBUG] Server main: Exited while(true) loop." << std::endl;

        // Если вышли из цикла, возможно, стоит подождать завершения задач перед выходом из main
        // if (!tasks.isEmpty()) {
        //     KJ_LOG(INFO, "Waiting for tasks to complete...");
        //     tasks. ότανEmpty().wait(waitScope); // Ожидаем, пока все задачи не завершатся
        // }

    } catch (const kj::Exception& e) {
        std::cerr << "[ERROR] Server main: Outer KJ Exception caught: " << e.getDescription().cStr() << std::endl;
        KJ_LOG(ERROR, "Cap'n Proto Server: KJ Exception at setup: ", e.getDescription().cStr());
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Server main: Outer STD Exception caught: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "[ERROR] Server main: Outer Unknown exception caught." << std::endl;
        return 1;
    }
    std::cout << "[DEBUG] Server main: Program finished normally (should not happen for a server if loop was infinite)." << std::endl;
    return 0;
}
