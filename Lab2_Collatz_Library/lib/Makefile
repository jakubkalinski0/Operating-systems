TARGET = collatz

CC = gcc
CFLAGS = -Wall
AR = ar

.PHONY: all collatz_static collatz_shared clean

all:
	make collatz_static
	make collatz_shared

collatz_static: $(TARGET).c
	$(CC) $(CFLAGS) -c $(TARGET).c -o $(TARGET).o
	$(AR) rcs libcollatz.a $(TARGET).o

collatz_shared: $(TARGET).c
	$(CC) $(CFLAGS) -fPIC -c $(TARGET).c -o $(TARGET).o
	$(CC) -shared -o libcollatz_shared.so $(TARGET).o

clean:
	rm -f $(TARGET).o libcollatz.a libcollatz_shared.so
