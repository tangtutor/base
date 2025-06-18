# Makefile for Base64 Network Transfer System

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pthread
GTK_FLAGS = `pkg-config --cflags --libs gtk+-3.0`
SQLITE_FLAGS = -lsqlite3

# Directories
SRC_DIR = src
BUILD_DIR = build
CLIENT_DIR = $(SRC_DIR)/client
SERVER_DIR = $(SRC_DIR)/server
COMMON_DIR = $(SRC_DIR)/common

# Source files
CLIENT_SOURCES = $(CLIENT_DIR)/main.c $(CLIENT_DIR)/network.c $(CLIENT_DIR)/update.c $(CLIENT_DIR)/gui.c $(CLIENT_DIR)/file_handler.c $(CLIENT_DIR)/config.c $(COMMON_DIR)/utils.c $(COMMON_DIR)/base64.c
SERVER_SOURCES = $(SERVER_DIR)/main.c $(SERVER_DIR)/network.c $(SERVER_DIR)/database.c $(SERVER_DIR)/file_handler.c $(SERVER_DIR)/message_handler.c $(COMMON_DIR)/utils.c $(COMMON_DIR)/base64.c

# Object files
CLIENT_OBJECTS = $(CLIENT_SOURCES:%.c=$(BUILD_DIR)/%.o)
SERVER_OBJECTS = $(SERVER_SOURCES:%.c=$(BUILD_DIR)/%.o)

# Executables
CLIENT_TARGET = $(BUILD_DIR)/client
SERVER_TARGET = $(BUILD_DIR)/server

# Default target
all: directories $(CLIENT_TARGET) $(SERVER_TARGET)

# Create directories
directories:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/$(CLIENT_DIR)
	@mkdir -p $(BUILD_DIR)/$(SERVER_DIR)
	@mkdir -p $(BUILD_DIR)/$(COMMON_DIR)
	@mkdir -p data/database
	@mkdir -p data/uploads

# Client target
$(CLIENT_TARGET): $(CLIENT_OBJECTS)
	$(CC) $(CLIENT_OBJECTS) -o $@ $(CFLAGS) $(GTK_FLAGS)

# Server target
$(SERVER_TARGET): $(SERVER_OBJECTS)
	$(CC) $(SERVER_OBJECTS) -o $@ $(CFLAGS) $(SQLITE_FLAGS)

# Compile client source files
$(BUILD_DIR)/$(CLIENT_DIR)/%.o: $(CLIENT_DIR)/%.c
	$(CC) $(CFLAGS) `pkg-config --cflags gtk+-3.0` -c $< -o $@

# Compile server source files
$(BUILD_DIR)/$(SERVER_DIR)/%.o: $(SERVER_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile common source files
$(BUILD_DIR)/$(COMMON_DIR)/%.o: $(COMMON_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build files
clean:
	rm -rf $(BUILD_DIR)

# Install dependencies (Ubuntu/Debian)
install-deps:
	sudo apt-get update
	sudo apt-get install build-essential libsqlite3-dev libgtk-3-dev

# Run server
run-server: $(SERVER_TARGET)
	./$(SERVER_TARGET)

# Run client
run-client: $(CLIENT_TARGET)
	./$(CLIENT_TARGET)

.PHONY: all clean directories install-deps run-server run-client