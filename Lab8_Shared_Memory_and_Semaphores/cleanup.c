#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <semaphore.h>

#include "config.h"

int main() {
    int success = 1;

    if (shm_unlink(SHM_NAME) == -1) {
        perror("shm_unlink");
        success = 0;
    } else {
        printf("Shared memory %s unlinked.\n", SHM_NAME);
    }

    if (sem_unlink(SEM_ANY) == -1) {
        perror("sem_unlink SEM_ANY");
        success = 0;
    } else {
        printf("Semaphore %s unlinked.\n", SEM_ANY);
    }

    if (sem_unlink(SEM_EMPTY) == -1) {
        perror("sem_unlink SEM_EMPTY");
        success = 0;
    } else {
        printf("Semaphore %s unlinked.\n", SEM_EMPTY);
    }

    if (sem_unlink(SEM_MUTEX) == -1) {
        perror("sem_unlink SEM_MUTEX");
        success = 0;
    } else {
        printf("Semaphore %s unlinked.\n", SEM_MUTEX);
    }

    if (success) {
        printf("All IPC resources unlinked successfully.\n");
        return EXIT_SUCCESS;
    } else {
        printf("Failed to unlink one or more IPC resources.\n");
        return EXIT_FAILURE;
    }
}