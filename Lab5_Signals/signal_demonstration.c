#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

// Handler dla sygnału SIGUSR1
void sigusr1_handler(int sig) {
    printf("Otrzymano sygnał SIGUSR1 (%d)\n", sig);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Użycie: %s <none|ignore|handler|mask>\n", argv[0]);
        return 1;
    }

    sigset_t mask;
    sigset_t pending;
    struct sigaction sa;

    printf("PID: %d\n", getpid());

    if (strcmp(argv[1], "none") == 0) {
        printf("Tryb: none - używanie domyślnej reakcji na sygnał\n");

    } else if (strcmp(argv[1], "ignore") == 0) {
        printf("Tryb: ignore - ignorowanie sygnału\n");
        signal(SIGUSR1, SIG_IGN);

    } else if (strcmp(argv[1], "handler") == 0) {
        printf("Tryb: handler - instalacja handlera\n");
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = sigusr1_handler;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGUSR1, &sa, NULL);

    } else if (strcmp(argv[1], "mask") == 0) {
        printf("Tryb: mask - maskowanie sygnału\n");
        sigemptyset(&mask);
        sigaddset(&mask, SIGUSR1);
        sigprocmask(SIG_BLOCK, &mask, NULL);

    } else {
        fprintf(stderr, "Nieznany argument: %s\n", argv[1]);
        return 1;
    }

    printf("Wysyłanie sygnału SIGUSR1 do siebie...\n");
    raise(SIGUSR1);
    printf("Sygnał został wysłany\n");

    // Sprawdzanie czy sygnał jest oczekujący (tylko dla trybu mask)
    if (strcmp(argv[1], "mask") == 0) {
        // Sprawdzamy czy sygnał jest widoczny w zbiorze sygnałów oczekujących
        sigpending(&pending);
        if (sigismember(&pending, SIGUSR1)) {
            printf("Sygnał SIGUSR1 jest oczekujący\n");
        } else {
            printf("Sygnał SIGUSR1 NIE jest oczekujący\n");
        }
    }

    printf("Koniec programu\n");
    return 0;
}