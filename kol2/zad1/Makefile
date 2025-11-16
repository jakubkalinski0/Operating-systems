CFLAGS = -Wall -lrt
CC = gcc

zad1_sender: zad1_sender.c
	$(CC) $^ -o $@ $(CFLAGS)

zad1_receiver: zad1_receiver.c
	$(CC) $^ -o $@ $(CFLAGS)

clean:
	rm -f zad1_sender.o zad1_sender zad1_receiver.o zad1_receiver

test:  zad1_sender zad1_receiver 
	@echo -e "\nTESTING...\n"
	./zad1_receiver &
	./zad1_sender 999 
	./zad1_receiver &
	./zad1_sender 33
	./zad1_receiver &
	./zad1_sender -100 
	./zad1_receiver &
	./zad1_sender napis
	
all: zad1_sender zad1_receiver