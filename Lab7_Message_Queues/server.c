#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#define MESSAGE_BUFFER_SIZE 2048
#define SERVER_QUEUE_NAME "/simple_chat_server_queue"
#define CLIENT_QUEUE_NAME_SIZE 40
#define MAX_CLIENTS_COUNT 3

typedef enum {
    INIT,
    IDENTIFIER,
    MESSAGE_TEXT,
    CLIENT_CLOSE
} message_type_t;

typedef struct {
    message_type_t type;
    int identifier;
    char text[MESSAGE_BUFFER_SIZE];
} message_t;

volatile sig_atomic_t should_close = false;

void SIGNAL_handler(int signum) {
    char msg[60];
    snprintf(msg, sizeof(msg), "Server (PID %d): Caught signal %d", getpid(), signum);
    write(STDERR_FILENO, msg, strlen(msg));
    char newline = '\n';
    write(STDERR_FILENO, &newline, 1);

    should_close = true;
}

int main() {
    struct sigaction sa;
    sa.sa_handler = SIGNAL_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Server: Error setting SIGINT handler");
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("Server: Error setting SIGTERM handler");
        exit(EXIT_FAILURE);
    }

    struct mq_attr attributes = {
        .mq_flags = 0,
        .mq_msgsize = sizeof(message_t),
        .mq_maxmsg = 10
    };

    mq_unlink(SERVER_QUEUE_NAME);

    mqd_t mq_descriptor = mq_open(SERVER_QUEUE_NAME, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR, &attributes);
    if (mq_descriptor < 0) {
        perror("mq_open server");
        exit(EXIT_FAILURE);
    }

    message_t receive_message;
    mqd_t client_queues[MAX_CLIENTS_COUNT];
    for (int i = 0; i < MAX_CLIENTS_COUNT; i++) {
        client_queues[i] = (mqd_t)-1;
    }

    printf("Server started (PID %d). Waiting for clients on %s...\n", getpid(), SERVER_QUEUE_NAME);

    while (!should_close) {
        ssize_t bytes_read = mq_receive(mq_descriptor, (char*)&receive_message, sizeof(message_t), NULL);

        if (bytes_read < 0) {
            if (errno == EINTR && should_close) {
                break;
            }
            if(should_close) break;
            perror("mq_receive in server");
            break;
        }

        if (bytes_read != sizeof(message_t)) {
            fprintf(stderr, "Server: Received message of incorrect size (%zd bytes), expected %zu. Discarding.\n", bytes_read, sizeof(message_t));
            continue;
        }

        switch (receive_message.type) {
            case INIT: {
                int id = -1;
                for (int i = 0; i < MAX_CLIENTS_COUNT; i++) {
                    if (client_queues[i] == (mqd_t)-1) {
                        id = i;
                        break;
                    }
                }

                if (id == -1) {
                    printf("Max number of clients has connected, can't open another connection for client queue: %s\n", receive_message.text);
                    continue;
                }

                receive_message.text[CLIENT_QUEUE_NAME_SIZE-1] = '\0';

                client_queues[id] = mq_open(receive_message.text, O_WRONLY);
                if (client_queues[id] < 0) {
                    fprintf(stderr, "Server: Failed to open client queue '%s' (id %d): %s\n", receive_message.text, id, strerror(errno));
                    client_queues[id] = (mqd_t)-1;
                    continue;
                }

                message_t send_message_id = {
                    .type = IDENTIFIER,
                    .identifier = id
                };
                memset(send_message_id.text, 0, MESSAGE_BUFFER_SIZE);


                if (mq_send(client_queues[id], (char*)&send_message_id, sizeof(send_message_id), 10) < 0) {
                    fprintf(stderr, "Server: Failed to send IDENTIFIER to client id %d (queue %s): %s\n", id, receive_message.text, strerror(errno));
                    mq_close(client_queues[id]);
                    client_queues[id] = (mqd_t)-1;
                } else {
                    printf("Registered connection with client at id: %d (Queue: %s)\n", id, receive_message.text);
                }
                break;
            }
            case MESSAGE_TEXT:
                if (receive_message.identifier < 0 || receive_message.identifier >= MAX_CLIENTS_COUNT ||
                    client_queues[receive_message.identifier] == (mqd_t)-1) {
                    fprintf(stderr, "Server: Received MESSAGE_TEXT from invalid/unknown client id %d. Discarding.\n", receive_message.identifier);
                    continue;
                }

                printf("Broadcasting message from client %d: %s\n", receive_message.identifier, receive_message.text);
                for (int target_id = 0; target_id < MAX_CLIENTS_COUNT; target_id++) {
                    if (client_queues[target_id] != (mqd_t)-1 && target_id != receive_message.identifier) {
                        if (mq_send(client_queues[target_id], (char*)&receive_message, sizeof(receive_message), 10) < 0) {
                            fprintf(stderr, "Server: Failed to broadcast to client id %d: %s. Closing connection.\n", target_id, strerror(errno));
                            mq_close(client_queues[target_id]);
                            client_queues[target_id] = (mqd_t)-1;
                            printf("Closed problematic connection with client %d due to send error\n", target_id);
                        }
                    }
                }
                break;
            case CLIENT_CLOSE:
                if (receive_message.identifier >= 0 && receive_message.identifier < MAX_CLIENTS_COUNT &&
                    client_queues[receive_message.identifier] != (mqd_t)-1) {
                    printf("Closing connection with client at id: %d\n", receive_message.identifier);
                    mq_close(client_queues[receive_message.identifier]);
                    client_queues[receive_message.identifier] = (mqd_t)-1;
                } else {
                    printf("Received CLIENT_CLOSE for invalid or already closed client id: %d\n", receive_message.identifier);
                }
                break;
            default:
                printf("Unexpected message type (%d) in server queue from id: %d \n", receive_message.type, receive_message.identifier);
                break;
        }
    }

    printf("Exiting server...\n");

    for (int i = 0; i < MAX_CLIENTS_COUNT; i++) {
        if (client_queues[i] != (mqd_t)-1) {
            printf("Closing client queue for ID %d upon server shutdown.\n", i);
            mq_close(client_queues[i]);
            client_queues[i] = (mqd_t)-1;
        }
    }

    mq_close(mq_descriptor);
    if (mq_unlink(SERVER_QUEUE_NAME) == -1) {
        perror("mq_unlink server queue");
    }
    printf("Server finished.\n");
    return EXIT_SUCCESS;
}