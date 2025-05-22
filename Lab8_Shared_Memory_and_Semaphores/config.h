#define SHM_NAME "/my_shared_memory"
#define SEM_ANY "/sem_any"
#define SEM_EMPTY "/sem_empty"
#define SEM_MUTEX "/sem_mutex"
#define MAX_MSG_SIZE 11
#define QUEUE_SIZE 10

typedef struct {
    char messages[QUEUE_SIZE][MAX_MSG_SIZE];
    int head;
    int tail;
} SharedQueue;