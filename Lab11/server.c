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
#include <time.h>
#include <errno.h>
#include <stddef.h>

client_info_t clients[MAX_CLIENTS];
struct pollfd poll_fds[MAX_CLIENTS + 1];
int num_active_poll_fds = 0;
int listener_fd;

void initialize_server() {
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        clients[i].sockfd = -1;
        clients[i].active_status = 0;
        strcpy(clients[i].name, "");
    }
    poll_fds[0].fd = listener_fd;
    poll_fds[0].events = POLLIN;
    num_active_poll_fds = 1;
    for (int i = 1; i <= MAX_CLIENTS; ++i) {
        poll_fds[i].fd = -1;
        poll_fds[i].events = 0;
    }
}

int add_client_to_server(int client_sockfd) {
    int client_idx = -1;
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i].active_status == 0) {
            client_idx = i;
            break;
        }
    }
    if (client_idx == -1) {
        send_full_message(client_sockfd, "SERVER: Max clients reached.");
        close(client_sockfd);
        return -1;
    }

    clients[client_idx].sockfd = client_sockfd;
    clients[client_idx].active_status = 1;
    clients[client_idx].last_response_time = time(NULL);
    strcpy(clients[client_idx].name, "");

    int poll_idx = -1;
    for(int i=1; i <= MAX_CLIENTS; ++i) {
        if (poll_fds[i].fd == -1) {
            poll_idx = i;
            break;
        }
    }
    if(poll_idx == -1) {
        fprintf(stderr, "Server: No poll_fd slot for new client!\n");
        clients[client_idx].active_status = 0;
        close(client_sockfd);
        return -1;
    }
    poll_fds[poll_idx].fd = client_sockfd;
    poll_fds[poll_idx].events = POLLIN;

    if(poll_idx >= num_active_poll_fds) num_active_poll_fds = poll_idx + 1;

    printf("Server: New client fd %d on clients[%d] and poll_fds[%d].\n", client_sockfd, client_idx, poll_idx);
    return client_idx;
}

void remove_client_from_server(int client_idx) {
    if (client_idx < 0 || client_idx >= MAX_CLIENTS || clients[client_idx].active_status == 0) return;

    int client_sockfd = clients[client_idx].sockfd;
    printf("Server: Removing client '%s' (fd %d).\n", clients[client_idx].name, client_sockfd);

    close(client_sockfd);
    clients[client_idx].active_status = 0;
    clients[client_idx].sockfd = -1;
    strcpy(clients[client_idx].name, "");

    for (int i = 1; i < num_active_poll_fds; ++i) {
        if (poll_fds[i].fd == client_sockfd) {
            poll_fds[i].fd = -1;
            if (i < num_active_poll_fds -1 && poll_fds[num_active_poll_fds - 1].fd != -1) {
                 poll_fds[i] = poll_fds[num_active_poll_fds - 1];
                 poll_fds[num_active_poll_fds -1].fd = -1;
            }
            num_active_poll_fds--;
            while (num_active_poll_fds > 1 && poll_fds[num_active_poll_fds - 1].fd == -1) {
                num_active_poll_fds--;
            }
            break;
        }
    }
}

int find_client_idx_by_sockfd(int sockfd) {
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i].active_status != 0 && clients[i].sockfd == sockfd) return i;
    }
    return -1;
}

int find_client_idx_by_name(const char* name) {
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i].active_status == 2 && strcmp(clients[i].name, name) == 0) return i;
    }
    return -1;
}

void process_client_message(int client_idx, char* message) {
    char response[BUFFER_SIZE];
    char arg1[MAX_NAME_LEN];
    char body[MAX_MSG_LEN];

    clients[client_idx].last_response_time = time(NULL);

    if (strncmp(message, "LOGIN ", 6) == 0) {
        sscanf(message + 6, "%s", clients[client_idx].name);
        for(int i=0; i < MAX_CLIENTS; ++i) {
            if (i != client_idx && clients[i].active_status == 2 && strcmp(clients[i].name, clients[client_idx].name) == 0) {
                send_full_message(clients[client_idx].sockfd, "SERVER: Name taken. Disconnecting.");
                remove_client_from_server(client_idx);
                return;
            }
        }
        clients[client_idx].active_status = 2;
        printf("Server: Client fd %d logged in as '%s'.\n", clients[client_idx].sockfd, clients[client_idx].name);
        send_full_message(clients[client_idx].sockfd, "SERVER: Login successful.");
        return;
    }

    if (clients[client_idx].active_status != 2) {
        send_full_message(clients[client_idx].sockfd, "SERVER: Please LOGIN first.");
        return;
    }

    if (strcmp(message, "LIST") == 0) {
        strcpy(response, "CLIENTS:");
        int count = 0;
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (clients[i].active_status == 2) {
                strcat(response, " "); strcat(response, clients[i].name); count++;
            }
        }
        if (count == 0) strcat(response, " (none)");
        send_full_message(clients[client_idx].sockfd, response);
    } else if (strncmp(message, "2ALL ", 5) == 0) {
        strncpy(body, message + 5, MAX_MSG_LEN -1); body[MAX_MSG_LEN-1] = '\0';
        time_t now = time(NULL); char time_str[9]; strftime(time_str, sizeof(time_str), "%H:%M:%S", localtime(&now));
        snprintf(response, BUFFER_SIZE, "MSG [%s %s]: %s", clients[client_idx].name, time_str, body);
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (clients[i].active_status == 2 && i != client_idx) send_full_message(clients[i].sockfd, response);
        }
    } else if (strncmp(message, "2ONE ", 5) == 0) {
        char* target_name_ptr = message + 5;
        char* body_ptr = strchr(target_name_ptr, ' ');
        if (body_ptr) {
            *body_ptr = '\0'; strncpy(arg1, target_name_ptr, MAX_NAME_LEN-1); arg1[MAX_NAME_LEN-1]='\0';
            strncpy(body, body_ptr + 1, MAX_MSG_LEN-1); body[MAX_MSG_LEN-1]='\0';
            int target_idx = find_client_idx_by_name(arg1);
            if (target_idx != -1) {
                time_t now = time(NULL); char time_str[9]; strftime(time_str, sizeof(time_str), "%H:%M:%S", localtime(&now));
                snprintf(response, BUFFER_SIZE, "PVT_MSG [%s %s]: %s", clients[client_idx].name, time_str, body);
                send_full_message(clients[target_idx].sockfd, response);
            } else {
                snprintf(response, BUFFER_SIZE, "SERVER: Client '%s' not found.", arg1);
                send_full_message(clients[client_idx].sockfd, response);
            }
        } else send_full_message(clients[client_idx].sockfd, "SERVER: Usage: 2ONE <nick> <message>");
    } else if (strcmp(message, "STOP") == 0) {
        printf("Server: Client '%s' sent STOP.\n", clients[client_idx].name);
        send_full_message(clients[client_idx].sockfd, "SERVER: Disconnecting by request.");
        remove_client_from_server(client_idx);
    } else if (strcmp(message, "ALIVE_RESPONSE") == 0) {
        /* last_response_time already updated */
    } else {
        snprintf(response, BUFFER_SIZE, "SERVER: Unknown command: %s", message);
        send_full_message(clients[client_idx].sockfd, response);
    }
}

void manage_alive_clients() {
    static time_t last_ping_time = 0;
    time_t current_time = time(NULL);

    if (difftime(current_time, last_ping_time) >= ALIVE_PING_INTERVAL) {
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (clients[i].active_status == 2) {
                if(send_full_message(clients[i].sockfd, "ALIVE_PING") == -1) {
                    printf("Server: Failed to ping '%s', removing.\n", clients[i].name);
                    remove_client_from_server(i);
                }
            }
        }
        last_ping_time = current_time;
    }

    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i].active_status == 0) continue;
        double time_diff = difftime(current_time, clients[i].last_response_time);

        if (clients[i].active_status == 2 && time_diff > ALIVE_TIMEOUT) {
            printf("Server: Client '%s' timed out.\n", clients[i].name);
            send_full_message(clients[i].sockfd, "SERVER: Disconnected (timeout).");
            remove_client_from_server(i);
        } else if (clients[i].active_status == 1 && time_diff > ALIVE_TIMEOUT * 2) {
            printf("Server: Client fd %d (unidentified) login timeout.\n", clients[i].sockfd);
            send_full_message(clients[i].sockfd, "SERVER: Disconnected (login timeout).");
            remove_client_from_server(i);
        }
    }
}

void server_shutdown_signal(int sig) {
    (void)sig;
    printf("\nServer: Shutting down...\n");
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i].active_status != 0) {
            send_full_message(clients[i].sockfd, "SERVER: Server is shutting down.");
            close(clients[i].sockfd);
        }
    }
    close(listener_fd);
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc != 2) { fprintf(stderr, "Usage: %s <port>\n", argv[0]); return 1; }
    int port = atoi(argv[1]);
    char buffer[BUFFER_SIZE];

    signal(SIGINT, server_shutdown_signal);
    signal(SIGPIPE, SIG_IGN);

    listener_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listener_fd < 0) { perror("socket"); return 1; }
    int opt = 1;
    if (setsockopt(listener_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(listener_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind"); close(listener_fd); return 1;
    }
    if (listen(listener_fd, MAX_CLIENTS) < 0) {
        perror("listen"); close(listener_fd); return 1;
    }
    printf("Server listening on port %d\n", port);
    initialize_server();

    while (1) {
        int poll_count = poll(poll_fds, num_active_poll_fds, 1000);
        if (poll_count < 0) {
            if (errno == EINTR) continue;
            perror("poll"); break;
        }

        manage_alive_clients();

        if (poll_fds[0].revents & POLLIN) {
            struct sockaddr_in client_addr_info; socklen_t len = sizeof(client_addr_info);
            int client_sock = accept(listener_fd, (struct sockaddr*)&client_addr_info, &len);
            if (client_sock < 0) {
                if (errno != EINTR) perror("accept");
            } else {
                add_client_to_server(client_sock);
            }
        }

        for (int i = 1; i < num_active_poll_fds; ++i) {
             if (poll_fds[i].fd == -1) continue;

            if (poll_fds[i].revents & (POLLHUP | POLLERR | POLLNVAL)) {
                int c_idx = find_client_idx_by_sockfd(poll_fds[i].fd);
                printf("Server: Poll error/hangup for client fd %d (idx %d).\n", poll_fds[i].fd, c_idx);
                if (c_idx != -1) remove_client_from_server(c_idx);
                else {
                    close(poll_fds[i].fd);
                    poll_fds[i].fd = -1;
                }
                continue;
            }

            if (poll_fds[i].revents & POLLIN) {
                int client_sock_curr = poll_fds[i].fd;
                int client_idx_curr = find_client_idx_by_sockfd(client_sock_curr);
                if (client_idx_curr != -1) {
                    ssize_t n = read_line_from_socket(client_sock_curr, buffer, BUFFER_SIZE);
                    if (n > 0) {
                        printf("Server RX from '%s': %s\n", clients[client_idx_curr].name, buffer);
                        process_client_message(client_idx_curr, buffer);
                    } else if (n == 0) {
                        printf("Server: Client '%s' disconnected.\n", clients[client_idx_curr].name);
                        remove_client_from_server(client_idx_curr);
                    } else {
                        if (errno != EINTR) perror("read from client");
                        remove_client_from_server(client_idx_curr);
                    }
                }
            }
        }
    }
    close(listener_fd);
    return 0;
}