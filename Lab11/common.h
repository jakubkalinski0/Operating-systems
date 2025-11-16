#ifndef COMMON_H
#define COMMON_H

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
#include <stddef.h> // Dla NULL

#define MAX_CLIENTS 5
#define MAX_NAME_LEN 32
#define MAX_MSG_LEN 256
#define BUFFER_SIZE (MAX_NAME_LEN + MAX_MSG_LEN + 64)

#define ALIVE_PING_INTERVAL 15
#define ALIVE_TIMEOUT 40

typedef struct {
    int sockfd;
    char name[MAX_NAME_LEN];
    int active_status;
    time_t last_response_time;
} client_info_t;

static inline int send_full_message(int sockfd, const char *message) {
    char temp_buffer[BUFFER_SIZE];
    strncpy(temp_buffer, message, BUFFER_SIZE - 2);
    temp_buffer[BUFFER_SIZE - 2] = '\0';

    if (strlen(temp_buffer) > 0 && temp_buffer[strlen(temp_buffer) - 1] != '\n') {
        strcat(temp_buffer, "\n");
    } else if (strlen(temp_buffer) == 0) {
        return 0;
    }

    if (send(sockfd, temp_buffer, strlen(temp_buffer), 0) < 0) {
        return -1;
    }
    return 0;
}

static inline ssize_t read_line_from_socket(int sockfd, char *buffer, size_t max_len) {
    ssize_t n_read;
    size_t total_read = 0;
    char ch;

    memset(buffer, 0, max_len);

    while (total_read < max_len - 1) {
        n_read = recv(sockfd, &ch, 1, 0);
        if (n_read > 0) {
            if (ch == '\n') {
                buffer[total_read] = '\0';
                return total_read;
            }
            buffer[total_read++] = ch;
        } else if (n_read == 0) {
            return 0;
        } else {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue;
            return -1;
        }
    }
    buffer[total_read] = '\0';
    return total_read;
}

#endif // COMMON_H