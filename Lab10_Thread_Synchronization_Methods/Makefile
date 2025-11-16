CC = gcc
CFLAGS = -Wall -Wextra -std=c11
LDFLAGS = -pthread
TARGET = hospital

all: $(TARGET)

$(TARGET): hospital.c
	$(CC) $(CFLAGS) hospital.c -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)

run: all
	./$(TARGET) $(PATIENTS) $(PHARMACISTS)

.PHONY: all clean run