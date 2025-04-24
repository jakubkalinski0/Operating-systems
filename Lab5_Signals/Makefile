CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11

SRC = signal_demonstration.c catcher.c sender.c
BIN = signal_demonstration catcher sender

all: $(BIN)

signal_demonstration: signal_demonstration.c
	$(CC) $(CFLAGS) -o $@ $<

catcher: catcher.c
	$(CC) $(CFLAGS) -o $@ $<

sender: sender.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(BIN)