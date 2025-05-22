#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

// Handler for SIGUSR1 signal
void sigusr1_handler(int sig) {
    printf("Received SIGUSR1 signal (%d)\n", sig);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <none|ignore|handler|mask>\n", argv[0]);
        return 1;
    }

    sigset_t mask_set; // Renamed to avoid conflict with 'mask' argument in some functions
    sigset_t pending;
    struct sigaction sa;

    printf("PID: %d\n", getpid());

    if (strcmp(argv[1], "none") == 0) {
        printf("Mode: none - using default signal reaction\n");
        // Default action for SIGUSR1 is to terminate the process.
    } else if (strcmp(argv[1], "ignore") == 0) {
        printf("Mode: ignore - ignoring the signal\n");
        signal(SIGUSR1, SIG_IGN);

    } else if (strcmp(argv[1], "handler") == 0) {
        printf("Mode: handler - installing a handler\n");
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = sigusr1_handler;
        sigemptyset(&sa.sa_mask); // Initialize the mask for the handler
        sigaction(SIGUSR1, &sa, NULL);

    } else if (strcmp(argv[1], "mask") == 0) {
        printf("Mode: mask - masking the signal\n");
        sigemptyset(&mask_set);
        sigaddset(&mask_set, SIGUSR1);
        sigprocmask(SIG_BLOCK, &mask_set, NULL);

    } else {
        fprintf(stderr, "Unknown argument: %s\n", argv[1]);
        return 1;
    }

    printf("Sending SIGUSR1 signal to self...\n");
    raise(SIGUSR1);
    // If the signal was not ignored or masked, and if the default action is termination,
    // or if the handler calls exit(), the program might terminate before this line.
    printf("Signal has been sent\n");

    if (strcmp(argv[1], "mask") == 0) {
        sigpending(&pending);
        if (sigismember(&pending, SIGUSR1)) {
            printf("SIGUSR1 signal is pending\n");
        } else {
            printf("SIGUSR1 signal is NOT pending\n");
            // This might happen if the signal was somehow processed before sigpending
            // or if there's a race condition, though less likely here.
            // More likely, if the signal was unblocked and handled (e.g. default termination).
        }
    }

    // If the 'none' mode was chosen, the program likely terminated due to SIGUSR1's default action.
    // If 'handler' mode was chosen and the handler exits, it also won't reach here.
    // If 'ignore' or 'mask' was chosen, it should reach here.
    printf("End of program\n");
    return 0;
}