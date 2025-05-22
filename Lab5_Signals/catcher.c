#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

volatile int mode = 0;           // Program's operating mode
volatile int mode_changes = 0;   // Counter for mode changes
volatile sig_atomic_t sender_pid = 0;  // PID of the sender program
volatile sig_atomic_t counter = 0;     // Counter for mode 2
volatile int counting_active = 0;      // Flag for counter activity

// Declaration of Ctrl+C handler function
void ctrl_c_handler(int sig);

// Handler for SIGUSR1
void sigusr1_handler(int sig, siginfo_t *info, void *ucontext) {
    (void)sig;      // Mark parameter as intentionally unused
    (void)ucontext; // Mark parameter as intentionally unused

    // Save sender's PID
    sender_pid = info->si_pid;

    // Read mode from si_value
    int received_mode = info->si_value.sival_int;

    // Confirm signal reception
    union sigval value;
    value.sival_int = received_mode; // Send back the received mode in the confirmation
    if (sender_pid > 0) { // Ensure sender_pid is valid
        if (sigqueue(sender_pid, SIGUSR1, value) == -1) {
            perror("Error sending confirmation");
            // In case of an error, don't exit immediately, but print the error
        }
    } else {
        printf("Received signal without sender PID or PID is invalid. Not sending confirmation.\n");
        // In practice, if you use sigqueue in sender, PID will always be available
    }

    if (received_mode >= 1 && received_mode <= 5) {
        mode = received_mode;
        mode_changes++;

        printf("Received SIGUSR1 signal with mode: %d\n", mode);

        // Mode handling
        switch (mode) {
            case 1:
                printf("Mode 1: Number of mode changes: %d\n", mode_changes);
                break;

            case 2:
                printf("Mode 2: Starting to print numbers every second\n");
                counting_active = 1;
                counter = 0;
                break;

            case 3:
                printf("Mode 3: Ignoring Ctrl+C\n");
                signal(SIGINT, SIG_IGN);
                break;

            case 4:
                printf("Mode 4: Setting custom reaction to Ctrl+C\n");
                signal(SIGINT, SIG_DFL);  // First, reset to default
                struct sigaction sa_int; // Use a different name to avoid conflict
                memset(&sa_int, 0, sizeof(sa_int));
                sa_int.sa_handler = ctrl_c_handler;
                sigemptyset(&sa_int.sa_mask);
                sigaction(SIGINT, &sa_int, NULL);
                break;

            case 5:
                printf("Mode 5: Exiting\n");
                exit(0);
                break;
        }
    }
}

// Handler for Ctrl+C (SIGINT)
void ctrl_c_handler(int sig) {
    (void)sig;  // Mark parameter as intentionally unused
    printf("CTRL+C pressed\n");
}

int main() {
    struct sigaction sa;

    // Configure handler for SIGUSR1
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = sigusr1_handler;
    sa.sa_flags = SA_SIGINFO;  // Use sa_sigaction instead of sa_handler
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("Error setting handler for SIGUSR1");
        exit(1);
    }

    printf("Catcher program started. PID: %d\n", getpid());

    // Main program loop
    while (1) {
        if (mode == 2 && counting_active) {
            printf("Number: %d\n", counter++);
            sleep(1);
        } else {
            // Wait for signals
            pause();
        }
    }

    return 0;
}