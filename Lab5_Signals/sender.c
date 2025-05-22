#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

volatile sig_atomic_t confirmation_received = 0;
volatile int confirmed_mode = 0;

void sigusr1_handler(int sig, siginfo_t *info, void *ucontext) {
    (void)sig;
    (void)ucontext;
    confirmation_received = 1;
    confirmed_mode = info->si_value.sival_int;
    printf("Received confirmation. Catcher is in mode: %d\n", confirmed_mode);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <catcher_PID> <mode 1-5>\n", argv[0]);
        return 1;
    }

    pid_t catcher_pid = atoi(argv[1]);
    int mode = atoi(argv[2]);

    if (catcher_pid <= 0) {
        fprintf(stderr, "Invalid PID: %s\n", argv[1]);
        return 1;
    }

    if (mode < 1 || mode > 5) {
        fprintf(stderr, "Invalid mode. Allowed modes: 1-5\n");
        return 1;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = sigusr1_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("Error setting handler for SIGUSR1");
        return 1;
    }

    // Block SIGUSR1 signal
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    // Send signal with the specified mode
    printf("Sending mode %d to catcher (PID: %d)...\n", mode, catcher_pid);

    union sigval value;
    value.sival_int = mode;

    if (sigqueue(catcher_pid, SIGUSR1, value) == -1) {
        perror("Error sending signal");
        return 1;
    }

    // Wait for confirmation
    printf("Waiting for confirmation...\n");

    // Create a mask for sigsuspend
    sigset_t suspend_mask;
    sigfillset(&suspend_mask);
    sigdelset(&suspend_mask, SIGUSR1); // Only unblock SIGUSR1

    // Wait for confirmation using sigsuspend
    while (!confirmation_received) {
        sigsuspend(&suspend_mask);
    }

    // Restore old mask
    sigprocmask(SIG_SETMASK, &oldmask, NULL);

    printf("Confirmation received. Exiting.\n");

    return 0;
}