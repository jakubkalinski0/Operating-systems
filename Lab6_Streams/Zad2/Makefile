CC=gcc
CFLAGS := -Wall

all:  int man

man: man.c
	$(CC) $(CFLAGS) -o man man.c

int: int.c
	$(CC) $(CFLAGS) -o int int.c

clean:
	rm -rf all
	rm -rf *.fifo

.PHONY: all clean