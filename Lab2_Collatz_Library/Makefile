CC=gcc
BUILD_DIR=build
CFLAGS= -Wall

.PHONY: all client_static client_shared client_dynamic clean

all:
	make client_static
	make client_shared
	make client_dynamic

client_static: client.c
	mkdir -p $(BUILD_DIR)
	make -C lib collatz_static
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/$@ $< lib/libcollatz.a

client_shared: client.c
	mkdir -p $(BUILD_DIR)
	make collatz_shared -C lib
	$(CC) $(CFLAGS) -L ./lib $< -lcollatz_shared -Wl,-rpath=./lib -o $(BUILD_DIR)/$@

client_dynamic: client.c
	mkdir -p $(BUILD_DIR)
	make collatz_shared -C lib
	$(CC) $(CFLAGS) -DDYNAMIC_LOAD -o $(BUILD_DIR)/$@ $< -ldl

test_static:
	./$(BUILD_DIR)/client_static

test_shared:
	./$(BUILD_DIR)/client_shared

test_dynamic:
	./$(BUILD_DIR)/client_dynamic

clean:
	rm -rf $(BUILD_DIR)
	make -C lib clean