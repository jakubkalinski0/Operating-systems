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
#include <stddef.h>

#define MAX_CLIENTS 5
#define MAX_NAME_LEN 32
#define MAX_MSG_LEN 256
#define BUFFER_SIZE (MAX_NAME_LEN + MAX_MSG_LEN + 64)

#define ALIVE_PING_INTERVAL 15
#define ALIVE_TIMEOUT 40

typedef struct {
    struct sockaddr_in addr;
    socklen_t addr_len;
    char name[MAX_NAME_LEN];
    int active_status;
    time_t last_response_time;
} client_info_t;

static inline int send_full_message(int sockfd, const char *message,
                                    const struct sockaddr *dest_addr, socklen_t addrlen) {
    char temp_buffer[BUFFER_SIZE];
    strncpy(temp_buffer, message, BUFFER_SIZE - 2);
    temp_buffer[BUFFER_SIZE - 2] = '\0';

    size_t current_len = strlen(temp_buffer);
    if (current_len > 0 && temp_buffer[current_len - 1] == '\n') {
    } else if (current_len < BUFFER_SIZE - 2) {
        temp_buffer[current_len] = '\n';
        temp_buffer[current_len + 1] = '\0';
    } else if (current_len == 0 && BUFFER_SIZE > 1) {
        temp_buffer[0] = '\n';
        temp_buffer[1] = '\0';
    }

    ssize_t sent_bytes;
    if (dest_addr) {
        sent_bytes = sendto(sockfd, temp_buffer, strlen(temp_buffer), 0, dest_addr, addrlen);
    } else {
        sent_bytes = send(sockfd, temp_buffer, strlen(temp_buffer), 0);
    }

    if (sent_bytes < 0) {
        return -1;
    }
    return 0;
}


static inline ssize_t read_datagram_from_socket(int sockfd, char *buffer, size_t max_len,
                                              struct sockaddr *src_addr, socklen_t *addrlen) {
    memset(buffer, 0, max_len);
    ssize_t n_read;

    if (src_addr && addrlen) {
        n_read = recvfrom(sockfd, buffer, max_len - 1, 0, src_addr, addrlen);
    } else {
        n_read = recv(sockfd, buffer, max_len - 1, 0);
    }

    if (n_read > 0) {
        buffer[n_read] = '\0';
        if (n_read > 0 && buffer[n_read - 1] == '\n') {
            buffer[n_read - 1] = '\0';
        }
    } else if (n_read == 0) {
        buffer[0] = '\0';
        return 0;
    } else {
        if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
        }
        return -1;
    }
    return n_read;
}


#endif // COMMON_H