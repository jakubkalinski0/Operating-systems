#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

volatile sig_atomic_t confirmation_received = 0;
volatile int confirmed_mode = 0;

// Handler dla SIGUSR1 - potwierdzenie odbioru
void sigusr1_handler(int sig, siginfo_t *info, void *ucontext) {
    confirmation_received = 1;
    confirmed_mode = info->si_value.sival_int;
    printf("Otrzymano potwierdzenie odbioru. Catcher jest w trybie: %d\n", confirmed_mode);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Użycie: %s <PID_catchera> <tryb 1-5>\n", argv[0]);
        return 1;
    }

    // Parsowanie argumentów
    pid_t catcher_pid = atoi(argv[1]);
    int mode = atoi(argv[2]);

    if (catcher_pid <= 0) {
        fprintf(stderr, "Nieprawidłowy PID: %s\n", argv[1]);
        return 1;
    }

    if (mode < 1 || mode > 5) {
        fprintf(stderr, "Nieprawidłowy tryb. Dozwolone tryby: 1-5\n");
        return 1;
    }

    // Konfiguracja handlera dla SIGUSR1
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = sigusr1_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("Błąd przy ustawianiu handlera dla SIGUSR1");
        return 1;
    }

    // Blokowanie sygnału SIGUSR1
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    // Wysłanie sygnału z określonym trybem
    printf("Wysyłanie trybu %d do catchera (PID: %d)...\n", mode, catcher_pid);

    union sigval value;
    value.sival_int = mode;

    if (sigqueue(catcher_pid, SIGUSR1, value) == -1) {
        perror("Błąd przy wysyłaniu sygnału");
        return 1;
    }

    // Czekanie na potwierdzenie
    printf("Czekam na potwierdzenie...\n");

    // Tworzymy maskę dla sigsuspend
    sigset_t suspend_mask;
    sigfillset(&suspend_mask);
    sigdelset(&suspend_mask, SIGUSR1);

    // Czekamy na potwierdzenie przy użyciu sigsuspend
    while (!confirmation_received) {
        sigsuspend(&suspend_mask);
    }

    // Przywracanie starej maski
    sigprocmask(SIG_SETMASK, &oldmask, NULL);

    printf("Potwierdzenie otrzymane. Kończę pracę.\n");

    return 0;
}