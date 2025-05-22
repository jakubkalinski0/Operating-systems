CC = gcc
CFLAGS = -g -Wall -Wextra -std=c11 -O2
LDFLAGS = -pthread
LDLIBS = -lm

SRC = integrator_threaded.c
TARGET = integrator_threaded

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(LDLIBS)

clean:
	rm -f $(TARGET) *.o

.PHONY: all clean