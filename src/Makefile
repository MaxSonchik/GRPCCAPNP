# CXX = g++
# CXXFLAGS = -std=c++17 -Wall -g -pthread
# LDFLAGS = -lboost_system -lboost_thread -lstdc++fs # Add other boost libs if needed by your specific asio setup

# # Directories
# COMMON_DIR = common
# COMMON_INCLUDE_DIR = $(COMMON_DIR)/include
# COMMON_SRC_DIR = $(COMMON_DIR)/src

# TCP_APP_DIR = tcp_app
# TCP_INCLUDE_DIR = $(TCP_APP_DIR)/include

# # Source files
# COMMON_SRCS = $(wildcard $(COMMON_SRC_DIR)/*.cpp)
# COMMON_OBJS = $(COMMON_SRCS:.cpp=.o)

# TCP_SERVER_SRC = $(TCP_APP_DIR)/tcp_server.cpp
# TCP_CLIENT_SRC = $(TCP_APP_DIR)/tcp_client.cpp

# TCP_SERVER_OBJ = $(TCP_SERVER_SRC:.cpp=.o)
# TCP_CLIENT_OBJ = $(TCP_CLIENT_SRC:.cpp=.o)

# # Targets
# TCP_SERVER_TARGET = tcp_server
# TCP_CLIENT_TARGET = tcp_client

# # Include paths
# INCLUDES = -I$(COMMON_INCLUDE_DIR) -I$(TCP_INCLUDE_DIR)

# .PHONY: all clean tcp_server tcp_client format check_format generate_file

# all: tcp_server tcp_client

# # Common objects
# $(COMMON_SRC_DIR)/%.o: $(COMMON_SRC_DIR)/%.cpp
# 	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# # TCP Server
# $(TCP_SERVER_TARGET): $(TCP_SERVER_OBJ) $(COMMON_OBJS)
# 	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

# $(TCP_APP_DIR)/tcp_server.o: $(TCP_APP_DIR)/tcp_server.cpp
# 	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# # TCP Client
# $(TCP_CLIENT_TARGET): $(TCP_CLIENT_OBJ) $(COMMON_OBJS)
# 	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

# $(TCP_APP_DIR)/tcp_client.o: $(TCP_APP_DIR)/tcp_client.cpp
# 	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# # Generate test file (manual step, or can be integrated into client)
# GENERATE_FILE_UTIL_SRC = $(COMMON_SRC_DIR)/file_utils.cpp # Assuming main is guarded
# GENERATE_FILE_UTIL_OBJ = $(COMMON_SRC_DIR)/file_utils_main.o # Temp object for utility
# GENERATE_FILE_TARGET = generate_test_file

# # This is a bit of a hack for a standalone generator.
# # Better to have a small separate main.cpp or integrate into client.
# # For now, we'll assume file_utils.cpp has a conditional main for generation.
# # Or, you call generate_test_file() from your client/server setup if needed.
# # Let's make a dummy generate_file target for now which does nothing,
# # as file generation is called from client code.
# generate_file:
# 	@echo "Test file generation is handled by the client on its first run if file doesn't exist."
# 	@echo "Or, you can modify file_utils.cpp to have a standalone main()."

# clean:
# 	rm -f $(COMMON_OBJS) $(TCP_SERVER_OBJ) $(TCP_CLIENT_OBJ)
# 	rm -f $(TCP_SERVER_TARGET) $(TCP_CLIENT_TARGET)
# 	rm -f results.csv test_file.dat
# 	@echo "Cleaned up."

# # Clang-format targets
# FORMAT_FILES = $(wildcard $(COMMON_INCLUDE_DIR)/*.hpp) \
#                $(wildcard $(COMMON_SRC_DIR)/*.cpp) \
#                $(wildcard $(TCP_INCLUDE_DIR)/*.hpp) \
#                $(wildcard $(TCP_APP_DIR)/*.cpp)

# format: # This is like clangfix
# 	clang-format -i $(FORMAT_FILES)
# 	@echo "Formatted code with clang-format."

# check_format: # This is like clangcheck for format
# 	clang-format --dry-run --Werror $(FORMAT_FILES)
# 	@echo "Code formatting is correct."


# Makefile

# Компиляторы и флаги
# Makefile

# Компиляторы и флаги
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -pthread # -O2 для релиза, можно -g для отладки
LDFLAGS = # Общие флаги линкера, если нужны

# Исходные файлы и директории
COMMON_DIR = common
COMMON_SRC_DIR = $(COMMON_DIR)/src
COMMON_INCLUDE_DIR = $(COMMON_DIR)/include
COMMON_SRCS = $(wildcard $(COMMON_SRC_DIR)/*.cpp)
COMMON_OBJS = $(COMMON_SRCS:.cpp=.o)

# --- gRPC Конфигурация ---
GRPC_DIR = grpc_app
GRPC_GEN_DIR = gen_proto
PROTOC = protoc
GRPC_CPP_PLUGIN = grpc_cpp_plugin
GRPC_CPP_PLUGIN_PATH = $(shell which $(GRPC_CPP_PLUGIN)) # Должен найти /usr/bin/grpc_cpp_plugin
PROTO_FILE = benchmark.proto

# Флаги для gRPC C++ кода
# -I/usr/include для системных gRPC/protobuf заголовков
# -I$(GRPC_GEN_DIR) для сгенерированных
# -I. для benchmark.proto, если он импортирует другие proto из текущей директории
GRPC_CFLAGS_PKG = $(shell pkg-config --cflags grpc++ protobuf)
GRPC_CFLAGS = $(GRPC_CFLAGS_PKG) -I/usr/include -I$(GRPC_GEN_DIR) -I.

# Библиотеки для gRPC
# На Arch, pkg-config для grpc++ может быть неполным. Добавляем зависимости явно.
GRPC_LIBS_PKG = $(shell pkg-config --libs grpc++ protobuf) # Основные
# Рефлексия не имеет .pc файла, но libgrpc++_reflection.a/so должна быть, если нужна
# Пока не добавляем рефлексию явно, если основные примеры не требуют.
# Если серверу нужна рефлексия, добавьте -lgrpc++_reflection

# Abseil библиотеки (примерный список, может потребоваться уточнение)
# Убедитесь, что пакет abseil-cpp установлен
ABSL_LIBS = \
    -labsl_log_internal_check_op -labsl_log_internal_message -labsl_log_internal_format \
    -labsl_log_internal_nullguard -labsl_log_internal_conditions -labsl_log_internal_globals \
    -labsl_log_globals -labsl_log_severity -labsl_raw_logging_internal \
    -labsl_synchronization -labsl_time -labsl_strings -labsl_base -labsl_int128 \
    -labsl_throw_delegate -labsl_status -labsl_cord -labsl_city -labsl_hash \
    -labsl_low_level_hash -labsl_raw_hash_set \
    -labsl_stacktrace -labsl_symbolize -labsl_debugging_internal -labsl_demangle_internal \
    -labsl_bad_optional_access -labsl_bad_variant_access -labsl_str_format_internal \
    -labsl_exponential_biased -labsl_cord_internal -labsl_cordz_functions \
    -labsl_cordz_handle -labsl_cordz_info \
    -labsl_crc32c -labsl_crc_cpu_detect -labsl_crc_internal \
    -labsl_flags_internal -labsl_flags_marshalling -labsl_flags_reflection \
    -labsl_flags_private_handle_accessor -labsl_flags_commandlineflag \
    -labsl_flags_commandlineflag_internal -labsl_flags_config \
    -labsl_flags_program_name \
    -labsl_graphcycles_internal -labsl_kernel_timeout_internal \
    -labsl_leak_check -labsl_malloc_internal \
    -labsl_random_distributions -labsl_random_internal_platform \
    -labsl_random_internal_pool_urbg -labsl_random_internal_randen \
    -labsl_random_internal_randen_hwaes -labsl_random_internal_randen_hwaes_impl \
    -labsl_random_internal_randen_slow -labsl_random_internal_seed_material \
    -labsl_random_seed_gen_exception -labsl_random_seed_sequences \
    -labsl_spinlock_wait -labsl_statusor \
    -labsl_strings_internal -labsl_string_view \
    -labsl_time_zone -labsl_civil_time \
    -labsl_utf8_for_code_point \
    -labsl_vlog_config_internal

# gRPC Core зависимости, которые могут быть не подтянуты pkg-config
GRPC_CORE_DEPS = -lgrpc -lgpr -laddress_sorting -lcares -lre2 -lssl -lcrypto -lz
GRPC_REFLECTION_LIB = -lgrpc++_reflection # <--- ДОБАВЛЯЕМ ЭТУ СТРОКУ
GRPC_LIBS = $(GRPC_LIBS_PKG) $(ABSL_LIBS) $(GRPC_CORE_DEPS) $(GRPC_REFLECTION_LIB) -lpthread -ldl -lrt

GRPC_SERVER_SRC = $(GRPC_DIR)/grpc_server.cpp
GRPC_CLIENT_SRC = $(GRPC_DIR)/grpc_client.cpp
GRPC_GENERATED_HEADERS = $(GRPC_GEN_DIR)/benchmark.pb.h $(GRPC_GEN_DIR)/benchmark.grpc.pb.h
GRPC_GENERATED_SRCS = $(GRPC_GEN_DIR)/benchmark.pb.cc $(GRPC_GEN_DIR)/benchmark.grpc.pb.cc
GRPC_GENERATED_OBJS = $(GRPC_GENERATED_SRCS:.cc=.o)
GRPC_SERVER_OBJ = $(GRPC_SERVER_SRC:.cpp=.o)
GRPC_CLIENT_OBJ = $(GRPC_CLIENT_SRC:.cpp=.o)


# --- Cap'n Proto Конфигурация ---
CAPNP_DIR = capnp_app
CAPNP_GEN_DIR = gen_capnp
CAPNPC = capnp # Должен быть в /usr/local/bin/capnp
CAPNP_FILE = benchmark.capnp

CAPNP_INCLUDES = -I/usr/local/include # Для Cap'n Proto/KJ из /usr/local
CAPNP_CFLAGS = $(CAPNP_INCLUDES) -I$(CAPNP_GEN_DIR) -I.
CAPNP_LIBS = -L/usr/local/lib -lcapnp -lkj -lkj-async -lcapnp-rpc # Явно указываем -L

CAPNP_SERVER_SRC = $(CAPNP_DIR)/capnp_server.cpp
CAPNP_CLIENT_SRC = $(CAPNP_DIR)/capnp_client.cpp
CAPNP_GENERATED_HEADERS = $(CAPNP_GEN_DIR)/benchmark.capnp.h
CAPNP_GENERATED_SRCS = $(CAPNP_GEN_DIR)/benchmark.capnp.c++
CAPNP_GENERATED_OBJS = $(CAPNP_GENERATED_SRCS:.c++=.o)
CAPNP_SERVER_OBJ = $(CAPNP_SERVER_SRC:.cpp=.o)
CAPNP_CLIENT_OBJ = $(CAPNP_CLIENT_SRC:.cpp=.o)


# --- Цели ---
.PHONY: all clean grpc_gen capnp_gen common_lib clangcheck clangfix grpc_targets capnp_targets

# Основная цель для сборки всего
all: common_lib grpc_targets capnp_targets

grpc_targets: grpc_server grpc_client
capnp_targets: capnp_server capnp_client

# Компиляция общих файлов
common_lib: $(COMMON_OBJS)

$(COMMON_DIR)/src/%.o: $(COMMON_DIR)/src/%.cpp $(wildcard $(COMMON_INCLUDE_DIR)/*.hpp)
	@echo "Compiling Common: $<"
	$(CXX) $(CXXFLAGS) -I$(COMMON_INCLUDE_DIR) -c $< -o $@


# --- gRPC Секция ---
# Генерация gRPC кода
grpc_gen: $(GRPC_GENERATED_HEADERS) $(GRPC_GENERATED_SRCS)

$(GRPC_GENERATED_HEADERS) $(GRPC_GENERATED_SRCS): $(PROTO_FILE) | $(GRPC_GEN_DIR)
	@echo "Generating gRPC C++ from $(PROTO_FILE)..."
	$(PROTOC) -I. --cpp_out=$(GRPC_GEN_DIR) $(PROTO_FILE)
	$(PROTOC) -I. --grpc_out=$(GRPC_GEN_DIR) \
	--plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN_PATH) $(PROTO_FILE)

$(GRPC_GEN_DIR):
	mkdir -p $(GRPC_GEN_DIR)

# Компиляция сгенерированных gRPC файлов
$(GRPC_GEN_DIR)/benchmark.pb.o: $(GRPC_GEN_DIR)/benchmark.pb.cc $(GRPC_GEN_DIR)/benchmark.pb.h
	@echo "Compiling gRPC Generated: $<"
	$(CXX) $(CXXFLAGS) $(GRPC_CFLAGS) -c $< -o $@

$(GRPC_GEN_DIR)/benchmark.grpc.pb.o: $(GRPC_GEN_DIR)/benchmark.grpc.pb.cc $(GRPC_GEN_DIR)/benchmark.grpc.pb.h
	@echo "Compiling gRPC Generated: $<"
	$(CXX) $(CXXFLAGS) $(GRPC_CFLAGS) -c $< -o $@

# Компиляция gRPC приложений
$(GRPC_DIR)/grpc_server.o: $(GRPC_DIR)/grpc_server.cpp $(GRPC_GENERATED_HEADERS) $(wildcard $(COMMON_INCLUDE_DIR)/*.hpp)
	@echo "Compiling gRPC App: $<"
	$(CXX) $(CXXFLAGS) $(GRPC_CFLAGS) -I$(COMMON_INCLUDE_DIR) -c $< -o $@

$(GRPC_DIR)/grpc_client.o: $(GRPC_DIR)/grpc_client.cpp $(GRPC_GENERATED_HEADERS) $(wildcard $(COMMON_INCLUDE_DIR)/*.hpp)
	@echo "Compiling gRPC App: $<"
	$(CXX) $(CXXFLAGS) $(GRPC_CFLAGS) -I$(COMMON_INCLUDE_DIR) -c $< -o $@

# Линковка gRPC исполняемых файлов
grpc_server: $(GRPC_SERVER_OBJ) $(GRPC_GENERATED_OBJS) $(COMMON_OBJS)
	@echo "Linking $@"
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(GRPC_LIBS)

grpc_client: $(GRPC_CLIENT_OBJ) $(GRPC_GENERATED_OBJS) $(COMMON_OBJS)
	@echo "Linking $@"
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(GRPC_LIBS)


# --- Cap'n Proto Секция ---
# Генерация Cap'n Proto кода
capnp_gen: $(CAPNP_GENERATED_HEADERS) $(CAPNP_GENERATED_SRCS)

$(CAPNP_GENERATED_HEADERS) $(CAPNP_GENERATED_SRCS): $(CAPNP_FILE) | $(CAPNP_GEN_DIR)
	@echo "Generating Cap'n Proto C++ from $(CAPNP_FILE)..."
	$(CAPNPC) compile -oc++:$(CAPNP_GEN_DIR) $(CAPNP_FILE)

$(CAPNP_GEN_DIR):
	mkdir -p $(CAPNP_GEN_DIR)

# Компиляция сгенерированного Cap'n Proto файла
$(CAPNP_GEN_DIR)/benchmark.capnp.o: $(CAPNP_GEN_DIR)/benchmark.capnp.c++ $(CAPNP_GEN_DIR)/benchmark.capnp.h
	@echo "Compiling Cap'n Proto Generated: $<"
	$(CXX) $(CXXFLAGS) $(CAPNP_CFLAGS) -c $< -o $@

# Компиляция Cap'n Proto приложений
$(CAPNP_DIR)/capnp_server.o: $(CAPNP_DIR)/capnp_server.cpp $(CAPNP_GENERATED_HEADERS) $(wildcard $(COMMON_INCLUDE_DIR)/*.hpp)
	@echo "Compiling Cap'n Proto App: $<"
	$(CXX) $(CXXFLAGS) $(CAPNP_CFLAGS) -I$(COMMON_INCLUDE_DIR) -c $< -o $@ # Убрал -v, можно вернуть для отладки

$(CAPNP_DIR)/capnp_client.o: $(CAPNP_DIR)/capnp_client.cpp $(CAPNP_GENERATED_HEADERS) $(wildcard $(COMMON_INCLUDE_DIR)/*.hpp)
	@echo "Compiling Cap'n Proto App: $<"
	$(CXX) $(CXXFLAGS) $(CAPNP_CFLAGS) -I$(COMMON_INCLUDE_DIR) -c $< -o $@

# Линковка Cap'n Proto исполняемых файлов
capnp_server: $(CAPNP_SERVER_OBJ) $(CAPNP_GENERATED_OBJS) $(COMMON_OBJS)
	@echo "Linking $@"
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(CAPNP_LIBS)

capnp_client: $(CAPNP_CLIENT_OBJ) $(CAPNP_GENERATED_OBJS) $(COMMON_OBJS)
	@echo "Linking $@"
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(CAPNP_LIBS)


# --- Утилиты ---
CLANG_FORMAT_TOOL = clang-format
ALL_CPP_HPP_FILES = $(shell find . \( -name '*.cpp' -or -name '*.hpp' \) -not -path "./$(GRPC_GEN_DIR)/*" -not -path "./$(CAPNP_GEN_DIR)/*")

clangcheck:
	@echo "Checking format with $(CLANG_FORMAT_TOOL)... (excluding generated files)"
	@$(CLANG_FORMAT_TOOL) --dry-run --Werror $(ALL_CPP_HPP_FILES)
	@echo "Format check passed."

clangfix:
	@echo "Applying $(CLANG_FORMAT_TOOL)... (excluding generated files)"
	@$(CLANG_FORMAT_TOOL) -i $(ALL_CPP_HPP_FILES)
	@echo "Formatting applied."

clean:
	@echo "Cleaning up..."
	rm -f $(COMMON_OBJS) \
	      $(GRPC_SERVER_OBJ) $(GRPC_CLIENT_OBJ) $(GRPC_GENERATED_OBJS) grpc_server grpc_client \
	      $(CAPNP_SERVER_OBJ) $(CAPNP_CLIENT_OBJ) $(CAPNP_GENERATED_OBJS) capnp_server capnp_client
	rm -rf $(GRPC_GEN_DIR) $(CAPNP_GEN_DIR)
	rm -f *.csv test_file.dat # Удаляем также результаты и тестовый файл
	@echo "Cleaned."
