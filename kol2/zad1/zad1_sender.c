#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

/* 
Zadanie 1 (30 % punktow calosci)

Tresc zadania
Napisz program, który sklada się z sendera (czesc 1) i receivera (czesc2), jako 2 osobne procesy z komunikacją przy użyciu socketów, każdy z procesów powinien być zdefiniowany w osobnym pliku, za całość zadania można dostać 30% punktów. W ramach tego procesu:

1) Utworz socket domeny unixowej typu datagramowego
2) Utworz strukture adresowa ustawiajac adres/sciezke komunikacji na SOCK_PATH
3) Polacz sie korzystajac ze zdefiniowanych socketu i struktury adresowej
4) Wyslij wiadomość poprzez socket
5) Posprzataj po sockecie na koniec


   if(argc !=2){
    printf("Not a suitable number of program parameters\n");
    return(1);
   }
    sleep(1);

    ...
    int fd = ...
    ...

    char buff[20];
    int to_send = sprintf(buff, argv[1]);

    if(write(fd, buff, to_send+1) == -1) {
        perror("Error sending msg to server");
    }
*/

#define UNIX_PATH_MAX 108
#define SOCK_PATH "sock_path"


int main(int argc, char *argv[]) {
    int fd;
    struct sockaddr_un addr;
    char buff[20];
    int to_send;

    if (argc != 2) {
        printf("Not a suitable number of program parameters\n");
        return 1;
    }
    sleep(1);

    fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd == -1) {
        perror("Error creating socket");
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCK_PATH, sizeof(addr.sun_path) - 1);
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("Error connecting to socket");
        close(fd);
        return 1;
    }

    to_send = sprintf(buff, "%s", argv[1]);
    if (write(fd, buff, to_send + 1) == -1) {
        perror("Error sending msg to server");
        close(fd);
        return 1;
    }

    close(fd);

    return 0;
}