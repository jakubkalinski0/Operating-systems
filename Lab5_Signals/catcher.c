#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

volatile int mode = 0;           // Tryb pracy programu
volatile int mode_changes = 0;   // Licznik zmian trybu
volatile sig_atomic_t sender_pid = 0;  // PID programu sender
volatile sig_atomic_t counter = 0;     // Licznik do trybu 2
volatile int counting_active = 0;      // Flaga aktywności licznika

// Deklaracja funkcji obsługi Ctrl+C
void ctrl_c_handler(int sig);

// Handler dla SIGUSR1
void sigusr1_handler(int sig, siginfo_t *info, void *ucontext) {
    (void)sig;      // Oznaczenie parametru jako celowo nieużywany
    (void)ucontext; // Oznaczenie parametru jako celowo nieużywany

    // Zapisanie PID nadawcy
    sender_pid = info->si_pid;

    // Odczytanie trybu z si_value
    int received_mode = info->si_value.sival_int;

    if (received_mode >= 1 && received_mode <= 5) {
        mode = received_mode;
        mode_changes++;

        printf("Otrzymano sygnał SIGUSR1 z trybem: %d\n", mode);

        // Obsługa trybu
        switch (mode) {
            case 1:
                printf("Tryb 1: Liczba zmian trybu: %d\n", mode_changes);
                break;

            case 2:
                printf("Tryb 2: Rozpoczynam wypisywanie liczb co sekundę\n");
                counting_active = 1;
                counter = 0;
                break;

            case 3:
                printf("Tryb 3: Ignorowanie Ctrl+C\n");
                signal(SIGINT, SIG_IGN);
                break;

            case 4:
                printf("Tryb 4: Ustawiam reakcję na Ctrl+C\n");
                signal(SIGINT, SIG_DFL);  // Najpierw resetujemy do domyślnej
                struct sigaction sa;
                memset(&sa, 0, sizeof(sa));
                sa.sa_handler = ctrl_c_handler;
                sigemptyset(&sa.sa_mask);
                sigaction(SIGINT, &sa, NULL);
                break;

            case 5:
                printf("Tryb 5: Kończę działanie\n");
                exit(0);
                break;
        }
    }

    // Potwierdzenie otrzymania sygnału
    union sigval value;
    value.sival_int = mode;
    sigqueue(sender_pid, SIGUSR1, value);
}

// Handler dla Ctrl+C (SIGINT)
void ctrl_c_handler(int sig) {
    (void)sig;  // Oznaczenie parametru jako celowo nieużywany
    printf("Wciśnięto CTRL+C\n");
}

int main() {
    struct sigaction sa;

    // Konfiguracja handlera dla SIGUSR1
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = sigusr1_handler;
    sa.sa_flags = SA_SIGINFO;  // Używamy sa_sigaction zamiast sa_handler
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("Błąd przy ustawianiu handlera dla SIGUSR1");
        exit(1);
    }

    printf("Program catcher uruchomiony. PID: %d\n", getpid());

    // Główna pętla programu
    while (1) {
        if (mode == 2 && counting_active) {
            printf("Liczba: %d\n", counter++);
            sleep(1);
        } else {
            // Czekamy na sygnały
            pause();
        }
    }

    return 0;
}