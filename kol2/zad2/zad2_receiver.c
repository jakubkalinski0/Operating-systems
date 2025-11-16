#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/*
Zadanie 2 (30% calosci punktow)

Tresc zadania
Napisz program, który sklada się z sendera (czesc 1) i receivera (czesc2), jako 2 osobne procesy z komunikacją przy użyciu kolejki komunikatów, każdy z procesów powinien być zdefiniowany w osobnym pliku, za całość zadania można dostać 30% punktów. W ramach tego procesu:

Czesc2 (receiver):
1) Otworzy kolejke systemu V "reprezentowana" przez KEY
2) W petli odczytuj zapisane w kolejce wartosci i przypisz je do zmiennej "variable"
	2.1) jesli nie ma oczekuj na nowe wartosci
	2.2) w przypadku otrzymania wartosci 999 zakoncz dzialanie
3) Posprzataj na koniec
*/

#define KEY "./zad2key"

struct message {
    long msg_type;
    int value;
};

int main() {
    sleep(1);
    int variable = 0;

    FILE *keyfile = fopen(KEY, "a");
    if (keyfile != NULL) {
        fclose(keyfile);
    }

    key_t key = ftok(KEY, 65);
    if (key == -1) {
        perror("ftok");
        return 1;
    }

    int retVal = msgget(key, 0666);
    if (retVal < 0) {
        perror("msgget");
        return 1;
    }

    while(1)
    {
        struct message msg;
        if (msgrcv(retVal, &msg, sizeof(msg.value), 0, 0) == -1) {
            perror("msgrcv");
            break;
        }
        variable = msg.value;
        printf("Variable received: %d\n", variable);
        if (variable == 999) {
            printf("Received value 999 - terminating operation\n");
            break;
        }
    }

    if (msgctl(retVal, IPC_RMID, NULL) == -1) {
        perror("msgctl");
        return 1;
    }
    printf("Message queue deleted\n");

    return 0;
}