#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

/* 
Zadanie 1 (30 % punktow calosci)

Tresc zadania
Napisz program, który sklada się z sendera (czesc 1) i receivera (czesc2), jako 2 osobne procesy z komunikacją przy użyciu socketów, każdy z procesów powinien być zdefiniowany w osobnym pliku, za całość zadania można dostać 30% punktów. W ramach tego procesu:

1) Utworz socket domeny unixowej typu datagramowego
2) Utworz strukture adresowa ustawiajac adres/sciezke komunikacji na SOCK_PATH
3) Zbinduj socket z adresem/sciezka SOCK_PATH
4) Wypisz otrzymana wiadomosc z socketu
5) Posprzataj po sockecie na koniec

    ...	
    int fd = ...
    ...   
    char buf[20];
    if(read(fd, buf, 20) == -1)
        perror("Error receiving message");
    int val = atoi(buf);
    printf("Zad1 received: %d \n", val);
    ...   
*/

#define UNIX_PATH_MAX 108
#define SOCK_PATH "sock_path"


int main() {
    int fd;
    struct sockaddr_un addr;
    char buf[20];

    fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd == -1) {
        perror("Error creating socket");
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCK_PATH, sizeof(addr.sun_path) - 1);

    unlink(SOCK_PATH);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("Error binding socket");
        close(fd);
        return 1;
    }

    if (read(fd, buf, 20) == -1) {
        perror("Error receiving message");
        close(fd);
        unlink(SOCK_PATH);
        return 1;
    }

    int val = atoi(buf);
    printf("Zad1 received: %d \n", val);

    close(fd);
    unlink(SOCK_PATH);

    return 0;
}