#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <signal.h>
#include <errno.h>
#include <stddef.h>

volatile sig_atomic_t keep_running = 1;
int client_sockfd = -1;

void client_shutdown_signal(int sig) {
    (void)sig;
    printf("\nClient: SIGINT. Sending STOP...\n");
    keep_running = 0;
    if (client_sockfd != -1) {
        send_full_message(client_sockfd, "STOP");
    }
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <name> <server_ip> <port>\n", argv[0]);
        return 1;
    }
    char client_name[MAX_NAME_LEN];
    strncpy(client_name, argv[1], MAX_NAME_LEN -1); client_name[MAX_NAME_LEN-1]='\0';
    const char *server_ip = argv[2];
    int server_port = atoi(argv[3]);

    char buffer_recv[BUFFER_SIZE];
    char buffer_send[BUFFER_SIZE];

    signal(SIGINT, client_shutdown_signal);
    signal(SIGPIPE, SIG_IGN);

    client_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sockfd < 0) { perror("socket"); return 1; }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("inet_pton"); close(client_sockfd); return 1;
    }

    if (connect(client_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect"); close(client_sockfd); return 1;
    }
    printf("Client: Connected as '%s'.\n", client_name);

    snprintf(buffer_send, BUFFER_SIZE, "LOGIN %s", client_name);
    if (send_full_message(client_sockfd, buffer_send) < 0) {
        fprintf(stderr, "Client: Failed to send LOGIN.\n");
        close(client_sockfd); return 1;
    }

    struct pollfd fds[2];
    fds[0].fd = STDIN_FILENO; fds[0].events = POLLIN;
    fds[1].fd = client_sockfd; fds[1].events = POLLIN;

    printf("Cmds: LIST, 2ALL <msg>, 2ONE <nick> <msg>, STOP (or 'exit')\n> ");
    fflush(stdout);

    while (keep_running) {
        int poll_count = poll(fds, 2, -1);
        if (poll_count < 0) {
            if (errno == EINTR && !keep_running) break;
            if (errno == EINTR) continue;
            perror("poll"); break;
        }
        if (!keep_running) break;

        if (fds[1].revents & (POLLHUP | POLLERR | POLLNVAL)) {
            printf("\nClient: Server connection error.\n");
            keep_running = 0; break;
        }
        if (fds[1].revents & POLLIN) {
            ssize_t n = read_line_from_socket(client_sockfd, buffer_recv, BUFFER_SIZE);
            if (n > 0) {
                if (strcmp(buffer_recv, "ALIVE_PING") == 0) {
                    send_full_message(client_sockfd, "ALIVE_RESPONSE");
                } else {
                    printf("\n%s\n> ", buffer_recv);
                    fflush(stdout);
                }
            } else if (n == 0) {
                printf("\nClient: Server closed connection.\n");
                keep_running = 0; break;
            } else {
                if(errno != EINTR) perror("read from server");
                keep_running = 0; break;
            }
        }

        if (fds[0].revents & POLLIN) {
            if (fgets(buffer_send, BUFFER_SIZE, stdin) != NULL) {
                buffer_send[strcspn(buffer_send, "\n")] = 0;
                if (strlen(buffer_send) == 0) { printf("> "); fflush(stdout); continue; }

                if (strcmp(buffer_send, "exit") == 0) strcpy(buffer_send, "STOP");

                if (send_full_message(client_sockfd, buffer_send) < 0) {
                    fprintf(stderr, "Client: Failed to send message.\n");
                    keep_running = 0; break;
                }
                if (strcmp(buffer_send, "STOP") == 0) {
                    keep_running = 0;
                } else {
                    printf("> "); fflush(stdout);
                }
            } else {
                printf("\nClient: Input error/EOF. Sending STOP.\n");
                if(keep_running && client_sockfd != -1) send_full_message(client_sockfd, "STOP");
                keep_running = 0; break;
            }
        }
        if (!keep_running) break;
    }

    printf("Client: Disconnecting.\n");
    if (client_sockfd != -1) { close(client_sockfd); client_sockfd = -1; }
    return 0;
}