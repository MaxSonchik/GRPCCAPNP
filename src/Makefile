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
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -pthread # -O2 для релиза, можно -g для отладки
LDFLAGS =

# Исходные файлы и директории
COMMON_DIR = common
COMMON_SRC_DIR = $(COMMON_DIR)/src
COMMON_INCLUDE_DIR = $(COMMON_DIR)/include
COMMON_SRCS = $(wildcard $(COMMON_SRC_DIR)/*.cpp)
COMMON_OBJS = $(COMMON_SRCS:.cpp=.o)

GRPC_DIR = grpc_app
GRPC_GEN_DIR = gen_proto
PROTOC = protoc
GRPC_CPP_PLUGIN = grpc_cpp_plugin
GRPC_CPP_PLUGIN_PATH = $(shell which $(GRPC_CPP_PLUGIN))
PROTO_FILE = benchmark.proto

# ИСПРАВЛЕНИЕ для GRPC_CFLAGS
# pkg-config --cflags grpc++ на вашей системе не выдает -I путей. Добавим их вручную.
# /usr/include должен содержать grpcpp/, grpc/, google/protobuf/ и т.д.
# Если у вас grpc и protobuf установлены в другие нестандартные места, укажите их здесь.
GRPC_INCLUDES = -I/usr/include
# GRPC_INCLUDES = -I/usr/include/grpcpp -I/usr/include/grpc++ -I/usr/include/google/protobuf (альтернативный, более явный вариант, если просто -I/usr/include не сработает для всех)
PKG_CONFIG_GRPC_CFLAGS = $(shell pkg-config --cflags grpc++) # Сохраняем то, что дает pkg-config (например, -DNOMINMAX)
GRPC_CFLAGS = $(PKG_CONFIG_GRPC_CFLAGS) $(GRPC_INCLUDES)

GRPC_LIBS = $(shell pkg-config --libs grpc++ protobuf) -lgrpc++_reflection
GRPC_SERVER_SRC = $(GRPC_DIR)/grpc_server.cpp
GRPC_CLIENT_SRC = $(GRPC_DIR)/grpc_client.cpp
GRPC_GEN_SRCS = $(GRPC_GEN_DIR)/benchmark.pb.cc $(GRPC_GEN_DIR)/benchmark.grpc.pb.cc
GRPC_GEN_OBJS = $(GRPC_GEN_SRCS:.cc=.o)
GRPC_SERVER_OBJ = $(GRPC_SERVER_SRC:.cpp=.o)
GRPC_CLIENT_OBJ = $(GRPC_CLIENT_SRC:.cpp=.o)

# Cap'n Proto
# ... (другие части Makefile) ...

# Cap'n Proto
CAPNP_DIR = capnp_app
CAPNP_GEN_DIR = gen_capnp
CAPNPC = capnp
CAPNP_FILE = benchmark.capnp

# УДАЛЯЕМ эти переменные, так как системная установка в /usr
# CAPNP_INCLUDE_PATH = -I/usr/local/include
# CAPNP_LIB_PATH = -L/usr/local/lib
# KJ_INCLUDE_PATH = -I/usr/local/include
# KJ_LIB_PATH = -L/usr/local/lib

CAPNP_INCLUDES = -I/usr/local/include # Путь, где лежат директории capnp/ и kj/
CAPNP_CFLAGS = $(CAPNP_INCLUDES) -DCAPNP_HEADER_INSTALL_DIR=""
# LIBS для Cap'n Proto - линковщик должен найти их в /usr/lib
CAPNP_LIBS = -lcapnp -lkj -lkj-async -lcapnp-rpc

CAPNP_SERVER_SRC = $(CAPNP_DIR)/capnp_server.cpp
CAPNP_CLIENT_SRC = $(CAPNP_DIR)/capnp_client.cpp
CAPNP_GEN_SRCS = $(CAPNP_GEN_DIR)/benchmark.capnp.c++
CAPNP_GEN_OBJS = $(CAPNP_GEN_SRCS:.c++=.o)
CAPNP_SERVER_OBJ = $(CAPNP_SERVER_SRC:.cpp=.o)
CAPNP_CLIENT_OBJ = $(CAPNP_CLIENT_SRC:.cpp=.o)


# Цели
.PHONY: all clean grpc_gen capnp_gen grpc capnp common_lib clangcheck clangfix

all: common_lib grpc_gen capnp_gen grpc_server grpc_client capnp_server capnp_client

common_lib: $(COMMON_OBJS)

$(COMMON_DIR)/src/%.o: $(COMMON_DIR)/src/%.cpp
	$(CXX) $(CXXFLAGS) -I$(COMMON_INCLUDE_DIR) -c $< -o $@

# --- gRPC ---
grpc_gen: $(GRPC_GEN_DIR)/benchmark.pb.h $(GRPC_GEN_DIR)/benchmark.grpc.pb.h

$(GRPC_GEN_DIR)/benchmark.pb.h: $(PROTO_FILE)
	mkdir -p $(GRPC_GEN_DIR)
	$(PROTOC) -I. --cpp_out=$(GRPC_GEN_DIR) $(PROTO_FILE)

$(GRPC_GEN_DIR)/benchmark.grpc.pb.h: $(PROTO_FILE)
	mkdir -p $(GRPC_GEN_DIR)
	$(PROTOC) -I. --grpc_out=$(GRPC_GEN_DIR) \
	--plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN_PATH) $(PROTO_FILE)

$(GRPC_GEN_DIR)/%.o: $(GRPC_GEN_DIR)/%.cc
	$(CXX) $(CXXFLAGS) $(GRPC_CFLAGS) -I$(GRPC_GEN_DIR) -I. -c $< -o $@

grpc_server: $(GRPC_SERVER_OBJ) $(GRPC_GEN_OBJS) $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(GRPC_LIBS)

grpc_client: $(GRPC_CLIENT_OBJ) $(GRPC_GEN_OBJS) $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(GRPC_LIBS)

$(GRPC_DIR)/%.o: $(GRPC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(GRPC_CFLAGS) -I$(COMMON_INCLUDE_DIR) -I$(GRPC_GEN_DIR) -I. -c $< -o $@
# --- Cap'n Proto ---
capnp_gen: $(CAPNP_GEN_DIR)/benchmark.capnp.c++

$(CAPNP_GEN_DIR)/benchmark.capnp.c++: $(CAPNP_FILE)
	mkdir -p $(CAPNP_GEN_DIR)
	# УДАЛЯЕМ -I/usr/local/include из команды компиляции схемы
	$(CAPNPC) compile -oc++:$(CAPNP_GEN_DIR) $(CAPNP_FILE)

$(CAPNP_GEN_DIR)/%.o: $(CAPNP_GEN_DIR)/%.c++
	$(CXX) $(CXXFLAGS) $(CAPNP_CFLAGS) -I$(CAPNP_GEN_DIR) -I. -c $< -o $@ # -I$(COMMON_INCLUDE_DIR) здесь не нужен, если только генерируемый код не включает напрямую что-то из common

capnp_server: $(CAPNP_SERVER_OBJ) $(CAPNP_GEN_OBJS) $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(CAPNP_LIBS)

capnp_client: $(CAPNP_CLIENT_OBJ) $(CAPNP_GEN_OBJS) $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(CAPNP_LIBS)

$(CAPNP_DIR)/%.o: $(CAPNP_DIR)/%.cpp
	$(CXX) -v $(CXXFLAGS) $(CAPNP_CFLAGS) -I$(COMMON_INCLUDE_DIR) -I$(CAPNP_GEN_DIR) -I. -c $< -o $@

# --- Утилиты ---
CLANG_FORMAT = clang-format -i
CLANG_TIDY = clang-tidy # Добавьте нужные проверки
SOURCE_FILES = $(wildcard $(COMMON_SRC_DIR)/*.cpp) $(wildcard $(COMMON_INCLUDE_DIR)/*.hpp) \
               $(wildcard $(GRPC_DIR)/*.cpp) $(wildcard $(CAPNP_DIR)/*.cpp)

clangcheck:
	@echo "Checking format with clang-format..."
	@clang-format --dry-run --Werror $(SOURCE_FILES)
	@echo "Format check passed."
	# @echo "Running clang-tidy (configure checks as needed)..."
	# @$(CLANG_TIDY) $(SOURCE_FILES) -- $(CXXFLAGS) -I$(COMMON_INCLUDE_DIR) -I$(GRPC_GEN_DIR) -I$(CAPNP_GEN_DIR) $(GRPC_CFLAGS) $(CAPNP_CFLAGS)

clangfix:
	@echo "Applying clang-format..."
	@$(CLANG_FORMAT) $(SOURCE_FILES)
	@echo "Formatting applied."
	# @echo "Applying clang-tidy fixes (configure checks as needed)..."
	# @$(CLANG_TIDY) -fix $(SOURCE_FILES) -- $(CXXFLAGS) -I$(COMMON_INCLUDE_DIR) -I$(GRPC_GEN_DIR) -I$(CAPNP_GEN_DIR) $(GRPC_CFLAGS) $(CAPNP_CFLAGS)

clean:
	rm -f $(COMMON_OBJS) \
	      $(GRPC_SERVER_OBJ) $(GRPC_CLIENT_OBJ) $(GRPC_GEN_OBJS) grpc_server grpc_client \
	      $(CAPNP_SERVER_OBJ) $(CAPNP_CLIENT_OBJ) $(CAPNP_GEN_OBJS) capnp_server capnp_client
	rm -rf $(GRPC_GEN_DIR) $(CAPNP_GEN_DIR)
	rm -f *.csv test_file.dat
