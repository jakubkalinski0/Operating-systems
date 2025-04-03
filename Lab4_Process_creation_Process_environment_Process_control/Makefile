CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11

SRC = process_creator.c process_executor.c
EXEC = process_creator process_executor

all: $(EXEC)

process_creator: process_creator.c
	$(CC) $(CFLAGS) -o $@ $<

process_executor: process_executor.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(EXEC)
