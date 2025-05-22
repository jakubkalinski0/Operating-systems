#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <time.h>

#include "config.h"

void generate_message(char *message, int size) {
    for (int i = 0; i < size - 1; i++) {
        message[i] = 'a' + rand() % 26;
    }
    message[size - 1] = '\0';
}

int main() {
    srand(time(NULL) ^ getpid());

    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open (user)");
        exit(EXIT_FAILURE);
    }

    SharedQueue *queue = mmap(NULL, sizeof(SharedQueue), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (queue == MAP_FAILED) {
        perror("mmap (user)");
        close(shm_fd);
        exit(EXIT_FAILURE);
    }

    sem_t *sem_any = sem_open(SEM_ANY, 0);
    sem_t *sem_empty = sem_open(SEM_EMPTY, 0);
    sem_t *sem_mutex = sem_open(SEM_MUTEX, 0);

    if (sem_any == SEM_FAILED || sem_empty == SEM_FAILED || sem_mutex == SEM_FAILED) {
        perror("sem_open (user)");
        if(sem_any != SEM_FAILED) sem_close(sem_any);
        if(sem_empty != SEM_FAILED) sem_close(sem_empty);
        if(sem_mutex != SEM_FAILED) sem_close(sem_mutex);
        munmap(queue, sizeof(SharedQueue));
        close(shm_fd);
        exit(EXIT_FAILURE);
    }

    char message[MAX_MSG_SIZE];

    while (1) {
        generate_message(message, MAX_MSG_SIZE);

        if (sem_wait(sem_empty) == -1) {
            perror("sem_wait SEM_EMPTY (user)");
            break;
        }
        if (sem_wait(sem_mutex) == -1) {
            perror("sem_wait SEM_MUTEX (user)");
            sem_post(sem_empty); // Release if mutex failed
            break;
        }

        strcpy(queue->messages[queue->tail], message);
        queue->tail = (queue->tail + 1) % QUEUE_SIZE;

        sem_post(sem_mutex);
        sem_post(sem_any);

        printf("User %d sent message: %s\n", getpid(), message);
        fflush(stdout);

        sleep(rand() % 3 + 1); // Sleep 1-3 seconds
    }

    munmap(queue, sizeof(SharedQueue));
    close(shm_fd);
    sem_close(sem_any);
    sem_close(sem_empty);
    sem_close(sem_mutex);

    return 0;
}