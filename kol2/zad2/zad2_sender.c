#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/*
Zadanie 2 (30% calosci punktow)

Tresc zadania
Napisz program, który sklada się z sendera (czesc 1) i receivera (czesc2), jako 2 osobne procesy z komunikacją przy użyciu kolejki komunikatów, każdy z procesów powinien być zdefiniowany w osobnym pliku, za całość zadania można dostać 30% punktów. W ramach tego procesu:

Czesc1 (sender):
1) Utworz kolejke komunikatow systemu V "reprezentowana" przez KEY
2) Wyslij do niej wartosc przekazana jako parametr wywolania programu
*/

#define KEY "./zad2key"

// Structure for message queue
struct message {
    long msg_type;
    int value;
};

int main(int argc, char* argv[]){

    if(argc != 2){
        printf("Not a suitable number of program parameters\n");
        return 1;
    }

    FILE *keyfile = fopen(KEY, "a");
    if (keyfile != NULL) {
        fclose(keyfile);
    }

    key_t key = ftok(KEY, 65);
    if (key == -1) {
        perror("ftok");
        return 1;
    }

    int msgid = msgget(key, 0666 | IPC_CREAT);
    if (msgid == -1) {
        perror("msgget");
        return 1;
    }

    struct message msg;
    msg.msg_type = 1;
    char *endptr;
    long val = strtol(argv[1], &endptr, 10);
    if (*endptr != '\0') {
        printf("Invalid number format, sending 0\n");
        msg.value = 0;
    } else {
        msg.value = (int)val;
    }

    if (msgsnd(msgid, &msg, sizeof(msg.value), 0) == -1) {
        perror("msgsnd");
        return 1;
    }

    printf("Value sent: %d\n", msg.value);

    return 0;
}