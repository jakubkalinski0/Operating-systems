#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>

#include "config.h"

int main() {
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open (printer)");
        exit(EXIT_FAILURE);
    }

    SharedQueue *queue = mmap(NULL, sizeof(SharedQueue), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (queue == MAP_FAILED) {
        perror("mmap (printer)");
        close(shm_fd);
        exit(EXIT_FAILURE);
    }

    sem_t *sem_any = sem_open(SEM_ANY, 0);
    sem_t *sem_empty = sem_open(SEM_EMPTY, 0);
    sem_t *sem_mutex = sem_open(SEM_MUTEX, 0);

    if (sem_any == SEM_FAILED || sem_empty == SEM_FAILED || sem_mutex == SEM_FAILED) {
        perror("sem_open (printer)");
        if(sem_any != SEM_FAILED) sem_close(sem_any);
        if(sem_empty != SEM_FAILED) sem_close(sem_empty);
        if(sem_mutex != SEM_FAILED) sem_close(sem_mutex);
        munmap(queue, sizeof(SharedQueue));
        close(shm_fd);
        exit(EXIT_FAILURE);
    }

    printf("Printer %d started.\n", getpid());
    fflush(stdout);

    while (1) {
        char buffer[MAX_MSG_SIZE];

        if (sem_wait(sem_any) == -1) {
            perror("sem_wait SEM_ANY (printer)");
            break;
        }
        if (sem_wait(sem_mutex) == -1) {
            perror("sem_wait SEM_MUTEX (printer)");
            sem_post(sem_any); // Release if mutex failed
            break;
        }

        strcpy(buffer, queue->messages[queue->head]);
        queue->head = (queue->head + 1) % QUEUE_SIZE;

        sem_post(sem_mutex);
        sem_post(sem_empty);

        printf("Printer %d received message: ", getpid());
        for (size_t i = 0; i < strlen(buffer); i++) {
            putchar(buffer[i]);
            fflush(stdout);
            sleep(1);
        }
        printf("\n");
        fflush(stdout);
    }

    munmap(queue, sizeof(SharedQueue));
    close(shm_fd);
    sem_close(sem_any);
    sem_close(sem_empty);
    sem_close(sem_mutex);

    printf("Printer %d finished.\n", getpid());
    fflush(stdout);
    return 0;
}