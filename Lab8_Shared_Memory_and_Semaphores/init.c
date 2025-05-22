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
    // Attempt to unlink existing IPC objects for a clean start
    shm_unlink(SHM_NAME);
    sem_unlink(SEM_ANY);
    sem_unlink(SEM_EMPTY);
    sem_unlink(SEM_MUTEX);

    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open (init)");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(shm_fd, sizeof(SharedQueue)) == -1) {
        perror("ftruncate (init)");
        close(shm_fd);
        shm_unlink(SHM_NAME);
        exit(EXIT_FAILURE);
    }

    SharedQueue *queue = mmap(NULL, sizeof(SharedQueue), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (queue == MAP_FAILED) {
        perror("mmap (init)");
        close(shm_fd);
        shm_unlink(SHM_NAME);
        exit(EXIT_FAILURE);
    }

    queue->head = 0;
    queue->tail = 0;

    // Use O_EXCL to ensure we are creating new semaphores
    sem_t *sem_any = sem_open(SEM_ANY, O_CREAT | O_EXCL, 0666, 0);
    if (sem_any == SEM_FAILED) {
        perror("sem_open SEM_ANY (init)");
        munmap(queue, sizeof(SharedQueue));
        close(shm_fd);
        shm_unlink(SHM_NAME);
        exit(EXIT_FAILURE);
    }

    sem_t *sem_empty = sem_open(SEM_EMPTY, O_CREAT | O_EXCL, 0666, QUEUE_SIZE);
    if (sem_empty == SEM_FAILED) {
        perror("sem_open SEM_EMPTY (init)");
        sem_close(sem_any);
        sem_unlink(SEM_ANY);
        munmap(queue, sizeof(SharedQueue));
        close(shm_fd);
        shm_unlink(SHM_NAME);
        exit(EXIT_FAILURE);
    }

    sem_t *sem_mutex = sem_open(SEM_MUTEX, O_CREAT | O_EXCL, 0666, 1);
    if (sem_mutex == SEM_FAILED) {
        perror("sem_open SEM_MUTEX (init)");
        sem_close(sem_any);
        sem_unlink(SEM_ANY);
        sem_close(sem_empty);
        sem_unlink(SEM_EMPTY);
        munmap(queue, sizeof(SharedQueue));
        close(shm_fd);
        shm_unlink(SHM_NAME);
        exit(EXIT_FAILURE);
    }

    printf("Shared memory and semaphores initialized successfully.\n");

    munmap(queue, sizeof(SharedQueue));
    close(shm_fd);
    sem_close(sem_any);
    sem_close(sem_empty);
    sem_close(sem_mutex);

    return 0;
}