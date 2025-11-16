CC = gcc
CFLAGS = -Wall -pthread
TARGETS = chat

all: $(TARGETS)

server: chat.c
	$(CC) $(CFLAGS) -o chat chat.c

clean:
	rm -f $(TARGETS)
