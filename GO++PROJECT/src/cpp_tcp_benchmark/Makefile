# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -pthread
LDFLAGS = -lboost_system -lboost_thread -pthread # Add -lboost_filesystem if needed by your Boost version for std::filesystem equivalent

# Directories
COMMON_INCLUDE_DIR = common/include
COMMON_SRC_DIR = common/src
CLIENT_DIR = client
SERVER_DIR = server
BUILD_DIR = build
RESULTS_DIR = results # For CSV files

# Source files
COMMON_SRCS = $(wildcard $(COMMON_SRC_DIR)/*.cpp)
CLIENT_SRC = $(CLIENT_DIR)/tcp_client.cpp
SERVER_SRC = $(SERVER_DIR)/tcp_server.cpp

# Object files
COMMON_OBJS = $(patsubst $(COMMON_SRC_DIR)/%.cpp, $(BUILD_DIR)/common/%.o, $(COMMON_SRCS))
CLIENT_OBJ = $(BUILD_DIR)/client/tcp_client.o
SERVER_OBJ = $(BUILD_DIR)/server/tcp_server.o

# Executables
CLIENT_EXE = tcp_client
SERVER_EXE = tcp_server

# Targets
all: $(CLIENT_EXE) $(SERVER_EXE)

$(CLIENT_EXE): $(CLIENT_OBJ) $(COMMON_OBJS)
	@mkdir -p $(RESULTS_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	@echo "Built TCP Client: $@"

$(SERVER_EXE): $(SERVER_OBJ) $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	@echo "Built TCP Server: $@"

# Rule to compile common object files
$(BUILD_DIR)/common/%.o: $(COMMON_SRC_DIR)/%.cpp | $(BUILD_DIR)/common
	$(CXX) $(CXXFLAGS) -I$(COMMON_INCLUDE_DIR) -c $< -o $@

# Rule to compile client object file
$(BUILD_DIR)/client/%.o: $(CLIENT_DIR)/%.cpp | $(BUILD_DIR)/client
	$(CXX) $(CXXFLAGS) -I$(COMMON_INCLUDE_DIR) -c $< -o $@

# Rule to compile server object file
$(BUILD_DIR)/server/%.o: $(SERVER_DIR)/%.cpp | $(BUILD_DIR)/server
	$(CXX) $(CXXFLAGS) -I$(COMMON_INCLUDE_DIR) -c $< -o $@

# Create build subdirectories
$(BUILD_DIR)/common $(BUILD_DIR)/client $(BUILD_DIR)/server:
	@mkdir -p $@

# Clang-format check
clangcheck:
	@echo "Running clang-format check..."
	@clang-format --dry-run -Werror $(CLIENT_DIR)/*.cpp $(SERVER_DIR)/*.cpp $(COMMON_INCLUDE_DIR)/*.hpp $(COMMON_SRC_DIR)/*.cpp
	@echo "clang-format check passed."

# Clang-format fix
clangfix:
	@echo "Applying clang-format..."
	@clang-format -i $(CLIENT_DIR)/*.cpp $(SERVER_DIR)/*.cpp $(COMMON_INCLUDE_DIR)/*.hpp $(COMMON_SRC_DIR)/*.cpp
	@echo "clang-format applied."

# Clean
clean:
	@echo "Cleaning build files..."
	@rm -rf $(BUILD_DIR) $(CLIENT_EXE) $(SERVER_EXE)
	@echo "Cleaning test_file.dat (if exists)..."
	@rm -f test_file.dat
	@echo "Cleaning results CSV files (if any)..."
	@rm -rf $(RESULTS_DIR)
	@echo "Clean finished."

.PHONY: all clean clangcheck clangfix
