CC = gcc
CFLAGS = -Wall -std=c11
LDFLAGS = -lm
TIME = /usr/bin/time

all: integrate

integrate: integrate.c
	$(CC) $(CFLAGS) -o integrate integrate.c $(LDFLAGS)

test: integrate
	@echo "20" `$(TIME) -f '%e' ./integrate 0.000000001 20 2>&1` "[s]"
	@echo "10" `$(TIME) -f '%e' ./integrate 0.000000001 10 2>&1` "[s]"
	@echo "5" `$(TIME) -f '%e' ./integrate 0.000000001 5  2>&1`  "[s]"
	@echo "1" `$(TIME) -f '%e' ./integrate 0.000000001 1  2>&1`  "[s]"

clean:
	rm -f integrate

.PHONY: all clean