
GO := go
GOFMT := gofmt -w
GOBUILD := $(GO) build
GORUN := $(GO) run
GOCLEAN := $(GO) clean


CLIENT_EXE := bin/go_tcp_client
SERVER_EXE := bin/go_tcp_server


CLIENT_SRC_DIR := ./client
SERVER_SRC_DIR := ./server
PKG_DIR := ./pkg

]GO_FILES := $(shell find $(CLIENT_SRC_DIR) $(SERVER_SRC_DIR) $(PKG_DIR) -name '*.go')

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

clean:
	@echo "Cleaning Go build artifacts..."
	$(GOCLEAN)
	rm -f $(CLIENT_EXE) $(SERVER_EXE)
	rm -f $(config.TestFileName)
	rm -rf bin

