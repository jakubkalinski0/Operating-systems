#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

int main() {
    double a, b;
    scanf("%lf %lf", &a, &b); // pobieranie krancow przedzialu

    int fd1 = open("pipe1", O_WRONLY); // pobranie desktryptora
    write(fd1, &a, sizeof(double)); // otworzenie pliku i wpisanie wartosci
    write(fd1, &b, sizeof(double));
    close(fd1); // zamykamy potok

    int fd2 = open("pipe2", O_RDONLY);
    double result;
    read(fd2, &result, sizeof(double)); // zczytanie wyniku z durgiego potoku
    close(fd2);

    printf("Total  %f\n", result);

    return 0;
}