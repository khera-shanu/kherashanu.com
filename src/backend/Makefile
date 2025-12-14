# Kherashanu Backend - C HTTP/1.1 Server + Framework + Database
CC = gcc

# Detect OS for OpenSSL paths
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    # macOS with Homebrew OpenSSL
    OPENSSL_PREFIX := $(shell brew --prefix openssl@3 2>/dev/null || echo /usr/local/opt/openssl)
    CFLAGS = -O2 -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L -I$(OPENSSL_PREFIX)/include
    LDFLAGS = -L$(OPENSSL_PREFIX)/lib -lssl -lcrypto -lpthread -lcurl -lz
else
    # Linux (OpenSSL in system paths)
    CFLAGS = -O2 -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L
    LDFLAGS = -lssl -lcrypto -lpthread -lm -lcurl -lz
endif

SRC_DIR = src
DB_SRC_DIR = src/db
JSON_SRC_DIR = src/json
FRAMEWORK_SRC_DIR = src/framework
ORM_SRC_DIR = src/orm
BUILD_DIR = build
TESTS_DIR = tests
TARGET = kherashanu-server

# Server sources (top-level src files)
SERVER_SRCS = $(wildcard $(SRC_DIR)/*.c)
SERVER_OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SERVER_SRCS))

# Database sources
DB_SRCS = $(wildcard $(DB_SRC_DIR)/*.c)
DB_OBJS = $(patsubst $(DB_SRC_DIR)/%.c,$(BUILD_DIR)/db_%.o,$(DB_SRCS))

# JSON sources
JSON_SRCS = $(wildcard $(JSON_SRC_DIR)/*.c)
JSON_OBJS = $(patsubst $(JSON_SRC_DIR)/%.c,$(BUILD_DIR)/json_%.o,$(JSON_SRCS))

# Framework sources
FRAMEWORK_SRCS = $(wildcard $(FRAMEWORK_SRC_DIR)/*.c)
FRAMEWORK_OBJS = $(patsubst $(FRAMEWORK_SRC_DIR)/%.c,$(BUILD_DIR)/framework_%.o,$(FRAMEWORK_SRCS))

# ORM sources
ORM_SRCS = $(wildcard $(ORM_SRC_DIR)/*.c)
ORM_OBJS = $(patsubst $(ORM_SRC_DIR)/%.c,$(BUILD_DIR)/orm_%.o,$(ORM_SRCS))

# All library objects (everything except main.o)
LIB_OBJS = $(filter-out $(BUILD_DIR)/main.o,$(SERVER_OBJS)) $(DB_OBJS) $(JSON_OBJS) $(FRAMEWORK_OBJS) $(ORM_OBJS)

.PHONY: all clean test test-all test-json test-framework test-orm test-db run dev-cert

all: $(BUILD_DIR) $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Main server target
$(TARGET): $(SERVER_OBJS) $(DB_OBJS) $(JSON_OBJS) $(FRAMEWORK_OBJS) $(ORM_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Server object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Database object files
$(BUILD_DIR)/db_%.o: $(DB_SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# JSON object files
$(BUILD_DIR)/json_%.o: $(JSON_SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Framework object files
$(BUILD_DIR)/framework_%.o: $(FRAMEWORK_SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# ORM object files
$(BUILD_DIR)/orm_%.o: $(ORM_SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# ========== Test targets ==========

# Test DB (existing)
test_db: $(BUILD_DIR) $(DB_OBJS) $(BUILD_DIR)/test_db.o
	$(CC) $(CFLAGS) -o $@ $(DB_OBJS) $(BUILD_DIR)/test_db.o $(LDFLAGS)

$(BUILD_DIR)/test_db.o: $(TESTS_DIR)/test_db.c
	$(CC) $(CFLAGS) -c $< -o $@

# Test JSON
test_json: $(BUILD_DIR) $(JSON_OBJS) $(BUILD_DIR)/test_json.o
	$(CC) $(CFLAGS) -o $@ $(JSON_OBJS) $(BUILD_DIR)/test_json.o $(LDFLAGS)

$(BUILD_DIR)/test_json.o: $(TESTS_DIR)/test_json.c
	$(CC) $(CFLAGS) -c $< -o $@

# Test Framework (needs JSON)
test_framework: $(BUILD_DIR) $(JSON_OBJS) $(FRAMEWORK_OBJS) $(BUILD_DIR)/test_framework.o
	$(CC) $(CFLAGS) -o $@ $(JSON_OBJS) $(FRAMEWORK_OBJS) $(BUILD_DIR)/test_framework.o $(LDFLAGS)

$(BUILD_DIR)/test_framework.o: $(TESTS_DIR)/test_framework.c
	$(CC) $(CFLAGS) -c $< -o $@

# Test ORM (needs JSON + DB)
test_orm: $(BUILD_DIR) $(DB_OBJS) $(JSON_OBJS) $(ORM_OBJS) $(BUILD_DIR)/test_orm.o
	$(CC) $(CFLAGS) -o $@ $(DB_OBJS) $(JSON_OBJS) $(ORM_OBJS) $(BUILD_DIR)/test_orm.o $(LDFLAGS)

$(BUILD_DIR)/test_orm.o: $(TESTS_DIR)/test_orm.c
	$(CC) $(CFLAGS) -c $< -o $@

# Run individual test suites
test: test_db
	./test_db

test-json: test_json
	./test_json

test-framework: test_framework
	./test_framework

test-orm: test_orm
	./test_orm

# Run all tests
test-all: test_db test_json test_framework test_orm
	@echo ""
	@echo "========================================="
	@echo "Running all test suites..."
	@echo "========================================="
	@echo ""
	./test_db && ./test_json && ./test_framework && ./test_orm
	@echo ""
	@echo "========================================="
	@echo "All tests completed!"
	@echo "========================================="

clean:
	rm -rf $(BUILD_DIR) $(TARGET) test_db test_json test_framework test_orm

# Development helpers
run: $(TARGET)
	./$(TARGET) --port 3000 --webroot ../frontend/dist

dev-cert:
	mkdir -p certs
	openssl req -x509 -newkey rsa:2048 -keyout certs/key.pem -out certs/cert.pem -days 365 -nodes -subj "/CN=localhost"

# Tools
create_session: $(BUILD_DIR) $(LIB_OBJS) $(BUILD_DIR)/create_session.o
	$(CC) $(CFLAGS) -o $@ $(LIB_OBJS) $(BUILD_DIR)/create_session.o $(LDFLAGS)

$(BUILD_DIR)/create_session.o: src/tools/create_session.c
	$(CC) $(CFLAGS) -c $< -o $@

debug_dump: $(BUILD_DIR) $(LIB_OBJS) $(BUILD_DIR)/debug_dump.o
	$(CC) $(CFLAGS) -o $@ $(LIB_OBJS) $(BUILD_DIR)/debug_dump.o $(LDFLAGS)

$(BUILD_DIR)/debug_dump.o: src/tools/debug_dump.c
	$(CC) $(CFLAGS) -c $< -o $@

