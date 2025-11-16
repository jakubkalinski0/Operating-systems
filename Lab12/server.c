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
struct pollfd poll_fds[1];
int server_sockfd;

void initialize_server_state() {
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        clients[i].active_status = 0;
        memset(&clients[i].addr, 0, sizeof(clients[i].addr));
        clients[i].addr_len = 0;
        strcpy(clients[i].name, "");
    }
    poll_fds[0].fd = server_sockfd;
    poll_fds[0].events = POLLIN;
}

int add_client_to_server_slot(const struct sockaddr_in* client_addr, socklen_t client_addr_len) {
    int client_idx = -1;
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i].active_status == 0) {
            client_idx = i;
            break;
        }
    }

    if (client_idx == -1) {
        send_full_message(server_sockfd, "SERVER: Max clients reached.",
                          (struct sockaddr*)client_addr, client_addr_len);
        return -1;
    }

    clients[client_idx].addr = *client_addr;
    clients[client_idx].addr_len = client_addr_len;
    clients[client_idx].active_status = 1;
    clients[client_idx].last_response_time = time(NULL);
    strcpy(clients[client_idx].name, "");

    char client_ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr->sin_addr, client_ip_str, INET_ADDRSTRLEN);
    printf("Server: New client slot %d for %s:%d.\n", client_idx, client_ip_str, ntohs(client_addr->sin_port));
    return client_idx;
}

void remove_client_from_server(int client_idx) {
    if (client_idx < 0 || client_idx >= MAX_CLIENTS || clients[client_idx].active_status == 0) return;

    char client_ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clients[client_idx].addr.sin_addr, client_ip_str, INET_ADDRSTRLEN);
    printf("Server: Removing client '%s' from %s:%d (slot %d).\n",
           clients[client_idx].name, client_ip_str, ntohs(clients[client_idx].addr.sin_port), client_idx);

    clients[client_idx].active_status = 0;
}

int find_client_idx_by_addr(const struct sockaddr_in* addr, socklen_t addr_len) {
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i].active_status != 0 &&
            clients[i].addr_len == addr_len &&
            clients[i].addr.sin_family == addr->sin_family &&
            clients[i].addr.sin_addr.s_addr == addr->sin_addr.s_addr &&
            clients[i].addr.sin_port == addr->sin_port) {
            return i;
        }
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
        char new_name[MAX_NAME_LEN];
        sscanf(message + 6, "%31s", new_name);
        new_name[MAX_NAME_LEN-1] = '\0';

        for(int i=0; i < MAX_CLIENTS; ++i) {
            if (i != client_idx && clients[i].active_status == 2 && strcmp(clients[i].name, new_name) == 0) {
                send_full_message(server_sockfd, "SERVER: Name taken. Disconnecting.",
                                  (struct sockaddr*)&clients[client_idx].addr, clients[client_idx].addr_len);
                remove_client_from_server(client_idx);
                return;
            }
        }
        strncpy(clients[client_idx].name, new_name, MAX_NAME_LEN -1);
        clients[client_idx].name[MAX_NAME_LEN-1] = '\0';
        clients[client_idx].active_status = 2;

        char client_ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clients[client_idx].addr.sin_addr, client_ip_str, INET_ADDRSTRLEN);
        printf("Server: Client from %s:%d (slot %d) logged in as '%s'.\n",
               client_ip_str, ntohs(clients[client_idx].addr.sin_port), client_idx, clients[client_idx].name);
        send_full_message(server_sockfd, "SERVER: Login successful.",
                          (struct sockaddr*)&clients[client_idx].addr, clients[client_idx].addr_len);
        return;
    }

    if (clients[client_idx].active_status != 2) {
        send_full_message(server_sockfd, "SERVER: Please LOGIN first.",
                          (struct sockaddr*)&clients[client_idx].addr, clients[client_idx].addr_len);
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
        send_full_message(server_sockfd, response,
                          (struct sockaddr*)&clients[client_idx].addr, clients[client_idx].addr_len);
    } else if (strncmp(message, "2ALL ", 5) == 0) {
        strncpy(body, message + 5, MAX_MSG_LEN -1); body[MAX_MSG_LEN-1] = '\0';
        time_t now = time(NULL); char time_str[9]; strftime(time_str, sizeof(time_str), "%H:%M:%S", localtime(&now));
        snprintf(response, BUFFER_SIZE, "MSG [%s %s]: %s", clients[client_idx].name, time_str, body);
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (clients[i].active_status == 2 && i != client_idx) {
                send_full_message(server_sockfd, response,
                                  (struct sockaddr*)&clients[i].addr, clients[i].addr_len);
            }
        }
    } else if (strncmp(message, "2ONE ", 5) == 0) {
        char* target_name_ptr = message + 5;
        char* body_ptr = strchr(target_name_ptr, ' ');
        if (body_ptr && (body_ptr - target_name_ptr) < MAX_NAME_LEN && strlen(body_ptr + 1) < MAX_MSG_LEN) {
            *body_ptr = '\0';
            strncpy(arg1, target_name_ptr, MAX_NAME_LEN-1); arg1[MAX_NAME_LEN-1]='\0';
            *body_ptr = ' ';

            strncpy(body, body_ptr + 1, MAX_MSG_LEN-1); body[MAX_MSG_LEN-1]='\0';

            int target_idx = find_client_idx_by_name(arg1);
            if (target_idx != -1) {
                time_t now = time(NULL); char time_str[9]; strftime(time_str, sizeof(time_str), "%H:%M:%S", localtime(&now));
                snprintf(response, BUFFER_SIZE, "PVT_MSG [%s %s]: %s", clients[client_idx].name, time_str, body);
                send_full_message(server_sockfd, response,
                                  (struct sockaddr*)&clients[target_idx].addr, clients[target_idx].addr_len);
            } else {
                snprintf(response, BUFFER_SIZE, "SERVER: Client '%s' not found.", arg1);
                send_full_message(server_sockfd, response,
                                  (struct sockaddr*)&clients[client_idx].addr, clients[client_idx].addr_len);
            }
        } else {
             send_full_message(server_sockfd, "SERVER: Usage: 2ONE <nick> <message> (check lengths)",
                              (struct sockaddr*)&clients[client_idx].addr, clients[client_idx].addr_len);
        }
    } else if (strcmp(message, "STOP") == 0) {
        printf("Server: Client '%s' sent STOP.\n", clients[client_idx].name);
        send_full_message(server_sockfd, "SERVER: Disconnecting by request.",
                          (struct sockaddr*)&clients[client_idx].addr, clients[client_idx].addr_len);
        remove_client_from_server(client_idx);
    } else if (strcmp(message, "ALIVE_RESPONSE") == 0) {
    } else {
        snprintf(response, BUFFER_SIZE, "SERVER: Unknown command: %s", message);
        send_full_message(server_sockfd, response,
                          (struct sockaddr*)&clients[client_idx].addr, clients[client_idx].addr_len);
    }
}

void manage_alive_clients() {
    static time_t last_ping_time = 0;
    time_t current_time = time(NULL);

    if (difftime(current_time, last_ping_time) >= ALIVE_PING_INTERVAL) {
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (clients[i].active_status == 2) { // Only ping logged-in clients
                if(send_full_message(server_sockfd, "ALIVE_PING", (struct sockaddr*)&clients[i].addr, clients[i].addr_len) == -1) {
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
            send_full_message(server_sockfd, "SERVER: Disconnected (timeout).",
                              (struct sockaddr*)&clients[i].addr, clients[i].addr_len);
            remove_client_from_server(i);
        } else if (clients[i].active_status == 1 && time_diff > ALIVE_TIMEOUT) {
            char client_ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &clients[i].addr.sin_addr, client_ip_str, INET_ADDRSTRLEN);
            printf("Server: Client from %s:%d (unidentified, slot %d) login timeout.\n",
                   client_ip_str, ntohs(clients[i].addr.sin_port), i);
            send_full_message(server_sockfd, "SERVER: Disconnected (login timeout).",
                              (struct sockaddr*)&clients[i].addr, clients[i].addr_len);
            remove_client_from_server(i);
        }
    }
}

volatile sig_atomic_t server_keep_running = 1;
void server_shutdown_signal(int sig) {
    (void)sig;
    printf("\nServer: Shutting down...\n");
    server_keep_running = 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) { fprintf(stderr, "Usage: %s <port>\n", argv[0]); return 1; }
    int port = atoi(argv[1]);
    char buffer[BUFFER_SIZE];

    signal(SIGINT, server_shutdown_signal);
    signal(SIGPIPE, SIG_IGN);

    server_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_sockfd < 0) { perror("socket"); return 1; }

    int opt = 1;
    if (setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
    }

    struct sockaddr_in server_addr_info;
    memset(&server_addr_info, 0, sizeof(server_addr_info));
    server_addr_info.sin_family = AF_INET;
    server_addr_info.sin_addr.s_addr = INADDR_ANY;
    server_addr_info.sin_port = htons(port);

    if (bind(server_sockfd, (struct sockaddr *)&server_addr_info, sizeof(server_addr_info)) < 0) {
        perror("bind"); close(server_sockfd); return 1;
    }

    printf("Server listening on UDP port %d\n", port);
    initialize_server_state();

    while (server_keep_running) {
        int poll_count = poll(poll_fds, 1, 1000);

        if (poll_count < 0) {
            if (errno == EINTR && !server_keep_running) break;
            if (errno == EINTR) continue;
            perror("poll"); break;
        }

        manage_alive_clients();
        if (!server_keep_running) break;


        if (poll_fds[0].revents & POLLIN) {
            struct sockaddr_in client_addr_recv;
            socklen_t client_addr_len_recv = sizeof(client_addr_recv);

            ssize_t n = read_datagram_from_socket(server_sockfd, buffer, BUFFER_SIZE,
                                                (struct sockaddr*)&client_addr_recv, &client_addr_len_recv);

            if (n > 0 || (n == 0 && strlen(buffer) > 0)) {
                 int client_idx = find_client_idx_by_addr(&client_addr_recv, client_addr_len_recv);
                 char client_ip_str[INET_ADDRSTRLEN];
                 inet_ntop(AF_INET, &client_addr_recv.sin_addr, client_ip_str, INET_ADDRSTRLEN);

                if (strncmp(buffer, "LOGIN ", 6) == 0) {
                    if (client_idx == -1) {
                        client_idx = add_client_to_server_slot(&client_addr_recv, client_addr_len_recv);
                        if (client_idx != -1) {
                             printf("Server RX from new %s:%d (slot %d): %s\n", client_ip_str, ntohs(client_addr_recv.sin_port), client_idx, buffer);
                            process_client_message(client_idx, buffer);
                        }
                    } else {
                        printf("Server RX from known '%s' (%s:%d, slot %d): %s\n", clients[client_idx].name, client_ip_str, ntohs(client_addr_recv.sin_port), client_idx, buffer);
                        process_client_message(client_idx, buffer); // Let it handle name collision etc.
                    }
                } else {
                    if (client_idx != -1) {
                        printf("Server RX from '%s' (%s:%d, slot %d): %s\n", clients[client_idx].name, client_ip_str, ntohs(client_addr_recv.sin_port), client_idx, buffer);
                        process_client_message(client_idx, buffer);
                    } else {
                        printf("Server: Ignoring non-LOGIN message from unknown %s:%d: %s\n",
                               client_ip_str, ntohs(client_addr_recv.sin_port), buffer);
                    }
                }
            } else if (n == 0) {
                 char client_ip_str[INET_ADDRSTRLEN];
                 inet_ntop(AF_INET, &client_addr_recv.sin_addr, client_ip_str, INET_ADDRSTRLEN);
                 printf("Server: Received empty datagram from %s:%d.\n", client_ip_str, ntohs(client_addr_recv.sin_port));
            } else {
                if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
                    perror("read_datagram_from_socket failed");
                }
            }
        }
        if (poll_fds[0].revents & (POLLERR | POLLNVAL)) {
            perror("Error/NVAL on server socket, shutting down");
            server_keep_running = 0; // Trigger shutdown
        }
         if (poll_fds[0].revents & POLLHUP) { // Highly unlikely for UDP
            fprintf(stderr, "Server socket POLLHUP (unexpected for UDP), shutting down.\n");
            server_keep_running = 0;
        }
    }

    printf("Server: Finalizing shutdown...\n");
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i].active_status != 0) {
            send_full_message(server_sockfd, "SERVER: Server is shutting down.",
                              (struct sockaddr*)&clients[i].addr, clients[i].addr_len);
        }
    }
    if (server_sockfd != -1) {
        close(server_sockfd);
        server_sockfd = -1;
    }
    printf("Server: Exited.\n");
    return 0;
}