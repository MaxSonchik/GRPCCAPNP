# Makefile for Go TCP Benchmark

# Определяем компилятор и утилиты Go
GO := go
GOFMT := gofmt -w # -w для записи изменений в файлы
GOBUILD := $(GO) build
GORUN := $(GO) run
GOCLEAN := $(GO) clean

# Имена исполняемых файлов
CLIENT_EXE := bin/go_tcp_client
SERVER_EXE := bin/go_tcp_server

# Директории исходников
CLIENT_SRC_DIR := ./client
SERVER_SRC_DIR := ./server
PKG_DIR := ./pkg

# Получаем все .go файлы из pkg (рекурсивно, если нужно, но здесь просто)
# Для простоты, предполагаем, что все .go файлы в pkg/.../ нужны.
# Go build сам разберется с зависимостями.
GO_FILES := $(shell find $(CLIENT_SRC_DIR) $(SERVER_SRC_DIR) $(PKG_DIR) -name '*.go')

.PHONY: all client server run_client run_server fmt clean test_file

all: client server

client: $(CLIENT_EXE)
server: $(SERVER_EXE)

$(CLIENT_EXE): $(CLIENT_SRC_DIR)/main.go $(wildcard $(PKG_DIR)/*/*.go)
	@echo "Building Go client..."
	@mkdir -p $(@D) # Создаем директорию bin, если ее нет
	$(GOBUILD) -o $@ $(CLIENT_SRC_DIR)/main.go

$(SERVER_EXE): $(SERVER_SRC_DIR)/main.go $(wildcard $(PKG_DIR)/*/*.go)
	@echo "Building Go server..."
	@mkdir -p $(@D)
	$(GOBUILD) -o $@ $(SERVER_SRC_DIR)/main.go

run_client: $(CLIENT_EXE)
	@echo "Running Go client..."
	./$(CLIENT_EXE) $(ARGS) # ARGS можно передавать, например: make run_client ARGS="-server_ip=192.168.1.100"

run_server: $(SERVER_EXE)
	@echo "Running Go server..."
	./$(SERVER_EXE)

fmt:
	@echo "Formatting Go files..."
	$(GOFMT) $(GO_FILES)

# Генерация тестового файла (вызывает клиент с флагом, если бы он был, или просто запускает клиент, который генерирует файл)
# Проще, если клиент всегда проверяет/генерирует файл при запуске.
# Эта цель больше для явного указания действия.
test_file: $(CLIENT_EXE) # Убедимся, что клиент собран
	@echo "Ensuring test file exists (client will generate it if needed)..."
	# Запускаем клиента, который сам проверит/создаст файл.
	# Можно добавить специальный флаг для клиента, чтобы он только генерировал файл и выходил,
	# но текущая реализация клиента уже делает это при обычном запуске.
	# ./$(CLIENT_EXE) --generate-file-only # (если бы такой флаг был)
	# Пока просто запускаем клиента, который создаст файл перед основной работой.
	# Для чистоты, можно сделать отдельную утилиту или команду.
	# Сейчас клиент делает это автоматически.

clean:
	@echo "Cleaning Go build artifacts..."
	$(GOCLEAN)
	rm -f $(CLIENT_EXE) $(SERVER_EXE)
	rm -f $(config.TestFileName)
	rm -rf bin


# Пример того, как можно было бы настроить переменные из config.go в Makefile,
# если бы они были нужны для команд (здесь они не нужны, т.к. Go код их использует напрямую).
# TEST_FILE_NAME_FROM_CONFIG := $(shell $(GO) run ./getconfigval TotalFileSize)
# где getconfigval.go это маленькая программка, печатающая значение константы.
