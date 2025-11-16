CC=gcc
BUILD_DIR=build
CFLAGS := -Wall -g -pthread
LDFLAGS := -lrt

.PHONY: all clean run_client run_server

all: $(BUILD_DIR)/client $(BUILD_DIR)/server

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/client: client.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(BUILD_DIR)/server: server.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

run_client: $(BUILD_DIR)/client
	./$(BUILD_DIR)/client

run_server: $(BUILD_DIR)/server
	./$(BUILD_DIR)/server

clean:
	rm -f /dev/mqueue/simple_chat_server_queue
	rm -f /dev/mqueue/simple_chat_client_queue_*
	rm -rf $(BUILD_DIR)