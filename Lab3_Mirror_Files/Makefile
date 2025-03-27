CC=gcc
CFLAGS=-Wall -Wextra -std=c11
TARGET=flipper
SRC=flipper.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

clean-dir:
	@if [ -z "$(DEST_DIR)" ]; then \
		echo "Usage: make clean-dir DEST_DIR=<directory>"; \
	else \
		rm -rf $(DEST_DIR); \
		echo "Removed directory: $(DEST_DIR)"; \
	fi

.PHONY: all clean clean-dir
