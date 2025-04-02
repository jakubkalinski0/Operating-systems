#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

void create_process(int n) {
    for (int i = 0; i < n; i++) {
        pid_t child_pid = fork();
        if (child_pid == 0) {
            printf("Parent PID: %d, Child PID: %d\n", getppid(), getpid());
            exit(EXIT_SUCCESS);
        }
        else if (child_pid < 0) {
            perror("fork failed");
            exit(EXIT_FAILURE);
        }
    }
    for (int i = 0; i < n; i++) {
        wait(NULL);
    }
    printf("%d\n", n);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <num_processes>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int num_processes = atoi(argv[1]);
    if (num_processes <= 0) {
        fprintf(stderr, "Invalid number of processes. Number of processes must be greater than zero\n");
        return EXIT_FAILURE;
    }

    create_process(num_processes);
    return EXIT_SUCCESS;
}