#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int global = 0;

void handle_process(const char *path) {
    int local = 0;

    printf("Program name: %s\n", path);

    pid_t child_pid = fork();
    if (child_pid < 0) {
        perror("fork failed");
        exit(EXIT_FAILURE);
    } else if (child_pid == 0) {
        printf("Child process\n");
        global++;
        local++;
        printf("Child PID = %d, Parent PID = %d\n", getpid(), getppid());
        printf("Child's local = %d, Child's global = %d\n", local, global);
        execl("/bin/ls", "ls", path, NULL);
        perror("execl failed");
        exit(EXIT_FAILURE);
    } else {
        int status;
        waitpid(child_pid, &status, 0);
        printf("Parent process\n");
        printf("Parent PID = %d, Child PID = %d\n", getpid(), child_pid);
        printf("Child exit code: %d\n", WEXITSTATUS(status));
        printf("Parent's local = %d, Parent's global = %d\n", local, global);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <directory>\n", argv[0]);
        return EXIT_FAILURE;
    }

    handle_process(argv[1]);
    return EXIT_SUCCESS;
}
