CFLAGS = -Wall
CC = gcc

zad2_sender: zad2_sender.o

zad2_receiver: zad2_receiver.o

clean:
	rm -f zad2_sender.o zad2_sender zad2_receiver.o zad2_receiver

test:  zad2_sender zad2_receiver 
	@echo -e "\nTESTING...\n"
	ipcs -q
	./zad2_sender 9 &
	ipcs -q
	./zad2_receiver
	ipcs -q
	./zad2_sender 13 &
	ipcs -q
	./zad2_receiver
	ipcs -q
	./zad2_sender -169 &
	ipcs -q
	./zad2_receiver
	ipcs -q
	./zad2_sender test &
	ipcs -q
	./zad2_receiver
	ipcs -q
all: zad2_sender zad2_receiver