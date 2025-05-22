CC = gcc
CFLAGS = -Wall -std=c11 -g
LDLIBS = -lrt -pthread

.PHONY: all clean

all: init user printer cleanup

init: init.c config.h
	$(CC) $(CFLAGS) -o init init.c $(LDLIBS)

user: user.c config.h
	$(CC) $(CFLAGS) -o user user.c $(LDLIBS)

printer: printer.c config.h
	$(CC) $(CFLAGS) -o printer printer.c $(LDLIBS)

cleanup: cleanup.c config.h
	$(CC) $(CFLAGS) -o cleanup cleanup.c $(LDLIBS)

clean:
	rm -f init user printer cleanup