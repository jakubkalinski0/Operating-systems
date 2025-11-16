#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <mqueue.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>

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
    char msg_prefix[50];
    snprintf(msg_prefix, sizeof(msg_prefix), "Client (PID %d): Caught signal %d", getpid(), signum);
    write(STDERR_FILENO, msg_prefix, strlen(msg_prefix));
    char newline = '\n';
    write(STDERR_FILENO, &newline, 1);

    should_close = true;
}

#define MIN(a, b) (a < b ? a : b)

int main() {
    pid_t pid = getpid();
    char queue_name[CLIENT_QUEUE_NAME_SIZE] = {0};
    snprintf(queue_name, CLIENT_QUEUE_NAME_SIZE, "/simple_chat_client_queue_%d", pid);

    struct sigaction sa;
    sa.sa_handler = SIGNAL_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Client: Error setting SIGINT handler");
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("Client: Error setting SIGTERM handler");
        exit(EXIT_FAILURE);
    }

    struct mq_attr attributes = {
        .mq_flags = 0,
        .mq_msgsize = sizeof(message_t),
        .mq_maxmsg = 10
    };

    mqd_t mq_client_descriptor = mq_open(queue_name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR, &attributes);
    if (mq_client_descriptor < 0) {
        perror("mq_open client");
        exit(EXIT_FAILURE);
    }

    mqd_t mq_server_descriptor = mq_open(SERVER_QUEUE_NAME, O_WRONLY);
    if (mq_server_descriptor < 0) {
        printf("Most likely server not opened!\n");
        perror("mq_open server");
        mq_close(mq_client_descriptor);
        mq_unlink(queue_name);
        exit(EXIT_FAILURE);
    }

    message_t message_init = {
        .type = INIT,
        .identifier = -1
    };
    strncpy(message_init.text, queue_name, MESSAGE_BUFFER_SIZE - 1);
    message_init.text[MESSAGE_BUFFER_SIZE - 1] = '\0';


    if (mq_send(mq_server_descriptor, (char*)&message_init, sizeof(message_init), 10) < 0) {
        perror("mq_send init");
        mq_close(mq_server_descriptor);
        mq_close(mq_client_descriptor);
        mq_unlink(queue_name);
        exit(EXIT_FAILURE);
    }

    int to_parent_pipe[2];
    if (pipe(to_parent_pipe) < 0) {
        perror("pipe");
        mq_close(mq_server_descriptor);
        mq_close(mq_client_descriptor);
        mq_unlink(queue_name);
        exit(EXIT_FAILURE);
    }


    pid_t listener_pid = fork();
    if (listener_pid < 0) {
        perror("fork listener");
        close(to_parent_pipe[0]);
        close(to_parent_pipe[1]);
        mq_close(mq_server_descriptor);
        mq_close(mq_client_descriptor);
        mq_unlink(queue_name);
        exit(EXIT_FAILURE);
    } else if (listener_pid == 0) {
        close(to_parent_pipe[0]);
        message_t receive_message;

        printf("Listener process (PID %d) started.\n", getpid());
        while (!should_close) {
            ssize_t bytes_read = mq_receive(mq_client_descriptor, (char*)&receive_message, sizeof(message_t), NULL);
            if (bytes_read < 0) {
                if (errno == EINTR && should_close) {
                    break;
                }
                if (should_close) break;
                perror("mq_receive in child");
                break;
            }

            if (bytes_read == sizeof(message_t)) {
                 switch (receive_message.type) {
                    case MESSAGE_TEXT:
                        printf("Received from id: %d message: %s\n", receive_message.identifier, receive_message.text);
                        break;
                    case IDENTIFIER:
                        printf("Received identifier from server: %d\n", receive_message.identifier);
                        if (write(to_parent_pipe[1], &receive_message.identifier, sizeof(receive_message.identifier)) < 0) {
                            perror("Child: write to pipe error");
                        }
                        break;
                    default:
                        printf("Unknown message type in client queue: %d\n", receive_message.type);
                        break;
                }
            } else if (bytes_read > 0) {
                 printf("Child: Received partial message (%zd bytes), expected %zu\n", bytes_read, sizeof(message_t));
            }
        }
        printf("Listener process (PID %d) exiting from receive loop.\n", getpid());
        close(to_parent_pipe[1]);
        mq_close(mq_client_descriptor);
        exit(0);
    } else {
        close(to_parent_pipe[1]);
        int identifier = -1;

        printf("Parent process (PID %d) waiting for identifier...\n", getpid());
        ssize_t read_bytes_pipe = read(to_parent_pipe[0], &identifier, sizeof(identifier));
        if (read_bytes_pipe <= 0) {
            if (read_bytes_pipe == 0) {
                fprintf(stderr, "Parent: read identifier from pipe: EOF (child closed pipe prematurely)\n");
            } else {
                perror("Parent: read identifier from pipe");
            }
            should_close = true;
            if (listener_pid > 0) kill(listener_pid, SIGTERM);
        } else {
            printf("Parent process (PID %d) received identifier: %d\n", getpid(), identifier);
        }
        close(to_parent_pipe[0]);

        char* buffer = NULL;
        size_t buffer_len = 0;

        while (!should_close && identifier != -1) {
            struct mq_attr current_server_attrs;
            if (mq_getattr(mq_server_descriptor, &current_server_attrs) == -1) {
                 if (errno == EINTR && should_close) break;
                 if (should_close) break;
                 perror("mq_getattr server in parent");
                 break;
            }

            if (current_server_attrs.mq_curmsgs >= current_server_attrs.mq_maxmsg) {
                printf("Server queue is full, please wait...\n");
                sleep(1);
                continue;
            }

            printf("Enter message: ");
            fflush(stdout);

            ssize_t nread = getline(&buffer, &buffer_len, stdin);

            if (nread == -1) {
                if (feof(stdin)) {
                    printf("EOF detected, closing.\n");
                } else if (errno == EINTR && should_close) {
                     printf("getline interrupted by signal, closing.\n");
                }
                else {
                    perror("getline input");
                }
                should_close = true;
                break;
            }

            if (nread > 0 && buffer[nread - 1] == '\n') {
                buffer[nread - 1] = '\0';
                nread--;
            }

            if (nread == 0) {
                printf("Empty message, not sending.\n");
                continue;
            }

            message_t send_message = {
                .type = MESSAGE_TEXT,
                .identifier = identifier
            };
            strncpy(send_message.text, buffer, MESSAGE_BUFFER_SIZE - 1);
            send_message.text[MESSAGE_BUFFER_SIZE - 1] = '\0';


            if (mq_send(mq_server_descriptor, (char*)&send_message, sizeof(send_message), 10) < 0) {
                if (errno == EINTR && should_close) {
                    break;
                }
                if(should_close) break;
                perror("mq_send to server in parent");
                break;
            }
        }
        if (buffer) {
            free(buffer);
        }

        printf("Parent process (PID %d) exiting from sending loop.\n", getpid());

        if (identifier != -1) {
            message_t message_close = {
                .type = CLIENT_CLOSE,
                .identifier = identifier
            };
            if (mq_send(mq_server_descriptor, (char*)&message_close, sizeof(message_close), 10) < 0) {
                if (!(errno == EINTR && should_close)) {
                    perror("mq_send CLIENT_CLOSE notification");
                }
            }
        }

        if (listener_pid > 0) {
            if (!should_close) {
                 kill(listener_pid, SIGTERM);
            }
            int status;
            waitpid(listener_pid, &status, 0);
            printf("Listener process (PID %d) terminated with status %d.\n", listener_pid, status);
        }

        mq_close(mq_server_descriptor);

        if (mq_unlink(queue_name) == -1) {
            perror("mq_unlink client queue");
        }
        printf("Client (PID %d) finished.\n", getpid());
    }
    return 0;
}