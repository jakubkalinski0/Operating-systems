#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <errno.h>

#define MAX_CLIENTS 10
#define MAX_NAME_LEN 32
#define MAX_MSG_LEN 256
#define BUFFER_SIZE 512

// Struktura reprezentująca klienta
typedef struct {
    int socket;
    char name[MAX_NAME_LEN];
    struct sockaddr_in address;
    time_t last_ping;
    int active;
} client_t;

// Globalne zmienne dla serwera
client_t clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
int server_socket;
int server_running = 1;

// Funkcje pomocnicze
void log_activity(const char* client_name, const char* activity) {
    time_t now = time(NULL);
    struct tm* timeinfo = localtime(&now);
    printf("[%02d:%02d:%02d] %s: %s\n",
           timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec,
           client_name, activity);
}

void send_to_client(int socket, const char* message) {
    if (send(socket, message, strlen(message), MSG_NOSIGNAL) < 0) {
        // Błąd wysyłania - prawdopodobnie klient się rozłączył
        // Nie robimy nic tutaj, błąd zostanie obsłużony wyżej
    }
}

void broadcast_message(const char* sender, const char* message, int sender_socket) {
    char full_message[BUFFER_SIZE];
    time_t now = time(NULL);
    struct tm* timeinfo = localtime(&now);

    snprintf(full_message, sizeof(full_message),
             "[%02d:%02d:%02d] %s: %s\n",
             timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec,
             sender, message);

    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && clients[i].socket != sender_socket) {
            send_to_client(clients[i].socket, full_message);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void send_private_message(const char* sender, const char* recipient, const char* message) {
    char full_message[BUFFER_SIZE];
    time_t now = time(NULL);
    struct tm* timeinfo = localtime(&now);

    snprintf(full_message, sizeof(full_message),
             "[%02d:%02d:%02d] (private) %s: %s\n",
             timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec,
             sender, message);

    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && strcmp(clients[i].name, recipient) == 0) {
            send_to_client(clients[i].socket, full_message);
            pthread_mutex_unlock(&clients_mutex);
            return;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void send_client_list(int socket) {
    char list_message[BUFFER_SIZE] = "Active clients:\n";

    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active) {
            strcat(list_message, "- ");
            strcat(list_message, clients[i].name);
            strcat(list_message, "\n");
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    send_to_client(socket, list_message);
}

int add_client(int socket, const char* name) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].active) {
            clients[i].socket = socket;
            strncpy(clients[i].name, name, MAX_NAME_LEN - 1);
            clients[i].name[MAX_NAME_LEN - 1] = '\0';
            clients[i].last_ping = time(NULL);
            clients[i].active = 1;
            pthread_mutex_unlock(&clients_mutex);
            log_activity(name, "CONNECTED to server");
            return 1;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    return 0;
}

void remove_client(int socket) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && clients[i].socket == socket) {
            log_activity(clients[i].name, "DISCONNECTED from server");
            clients[i].active = 0;
            close(clients[i].socket);
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

char* get_client_name(int socket) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && clients[i].socket == socket) {
            pthread_mutex_unlock(&clients_mutex);
            return clients[i].name;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    return NULL;
}

void* handle_client(void* arg) {
    int client_socket = *(int*)arg;
    char buffer[BUFFER_SIZE];
    char name[MAX_NAME_LEN];
    char log_message[BUFFER_SIZE];

    // Otrzymaj nazwę klienta
    if (recv(client_socket, name, sizeof(name), 0) <= 0) {
        close(client_socket);
        return NULL;
    }
    name[MAX_NAME_LEN - 1] = '\0';

    if (!add_client(client_socket, name)) {
        send_to_client(client_socket, "Server full. Connection rejected.\n");
        log_activity("UNKNOWN", "CONNECTION REJECTED - server full");
        close(client_socket);
        return NULL;
    }

    send_to_client(client_socket, "Welcome to the chat! Type 'LIST', '2ALL message', '2ONE name message', or 'STOP'\n");

    while (server_running) {
        int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            // Klient się rozłączył
            if (bytes_received == 0) {
                log_activity(name, "CONNECTION CLOSED normally");
            } else {
                log_activity(name, "CONNECTION ERROR - unexpected disconnect");
            }
            break;
        }

        buffer[bytes_received] = '\0';

        // Usuń znak nowej linii
        char* newline = strchr(buffer, '\n');
        if (newline) *newline = '\0';

        if (strcmp(buffer, "LIST") == 0) {
            log_activity(name, "LIST - requested client list");
            send_client_list(client_socket);
        }
        else if (strcmp(buffer, "STOP") == 0) {
            // Wyślij potwierdzenie rozłączenia
            send_to_client(client_socket, "Goodbye! You have been disconnected.\n");
            log_activity(name, "STOP - requested disconnect");
            break;
        }
        else if (strncmp(buffer, "2ALL ", 5) == 0) {
            char* message = buffer + 5;
            snprintf(log_message, sizeof(log_message), "2ALL - broadcast message: \"%.100s\"", message);
            log_activity(name, log_message);
            broadcast_message(name, message, client_socket);
        }
        else if (strncmp(buffer, "2ONE ", 5) == 0) {
            char* space = strchr(buffer + 5, ' ');
            if (space) {
                *space = '\0';
                char* recipient = buffer + 5;
                char* message = space + 1;
                snprintf(log_message, sizeof(log_message), "2ONE - private message to %s: \"%.80s\"", recipient, message);
                log_activity(name, log_message);
                send_private_message(name, recipient, message);
            } else {
                log_activity(name, "2ONE - invalid format (missing message)");
            }
        }
        else if (strcmp(buffer, "PONG") == 0) {
            // Odpowiedź na ping - aktualizuj czas ostatniej odpowiedzi
            pthread_mutex_lock(&clients_mutex);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].active && clients[i].socket == client_socket) {
                    clients[i].last_ping = time(NULL);
                    break;
                }
            }
            pthread_mutex_unlock(&clients_mutex);
            log_activity(name, "PONG - responded to ping");
        }
        else {
            snprintf(log_message, sizeof(log_message), "UNKNOWN COMMAND: \"%.100s\"", buffer);
            log_activity(name, log_message);
        }
    }

    // Spróbuj wysłać wiadomość o rozłączeniu (jeśli socket nadal działa)
    if (send(client_socket, "You have been disconnected from the server.\n", 44, MSG_NOSIGNAL) < 0) {
        // Socket już nie działa - nic nie robimy
    }

    remove_client(client_socket);
    return NULL;
}

void* ping_clients(void* arg) {
    while (server_running) {
        sleep(60); // Ping co 60 sekund

        pthread_mutex_lock(&clients_mutex);
        time_t now = time(NULL);

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].active) {
                if (now - clients[i].last_ping > 180) { // Timeout 3 minuty
                    log_activity(clients[i].name, "TIMEOUT - removing due to no response");
                    close(clients[i].socket);
                    clients[i].active = 0;
                } else {
                    // Wyślij ping do klienta (w tle, niewidoczny dla użytkowników)
                    if (send(clients[i].socket, "PING\n", 5, MSG_NOSIGNAL) < 0) {
                        log_activity(clients[i].name, "PING FAILED - connection lost, removing client");
                        close(clients[i].socket);
                        clients[i].active = 0;
                    } else {
                        log_activity(clients[i].name, "PING - sent keepalive");
                    }
                }
            }
        }
        pthread_mutex_unlock(&clients_mutex);
    }
    return NULL;
}

void signal_handler(int sig) {
    printf("\n");
    log_activity("SERVER", "SHUTDOWN - server shutting down");

    // Wyślij wiadomość o zamknięciu do wszystkich klientów
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active) {
            send_to_client(clients[i].socket, "Server is shutting down. You have been disconnected.\n");
            log_activity(clients[i].name, "FORCED DISCONNECT - server shutdown");
            close(clients[i].socket);
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    server_running = 0;
    close(server_socket);
    exit(0);
}

// SERWER
int main_server(int port) {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    pthread_t tid, ping_tid;

    signal(SIGINT, signal_handler);

    // Inicjalizacja tablicy klientów
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].active = 0;
    }

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket creation failed");
        return 1;
    }

    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        return 1;
    }

    if (listen(server_socket, 5) < 0) {
        perror("Listen failed");
        return 1;
    }

    printf("Chat server started on port %d\n", port);

    // Uruchom wątek pingowania
    pthread_create(&ping_tid, NULL, ping_clients, NULL);

    while (server_running) {
        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) {
            if (server_running) perror("Accept failed");
            continue;
        }

        pthread_create(&tid, NULL, handle_client, &client_socket);
        pthread_detach(tid);
    }

    close(server_socket);
    return 0;
}

// KLIENT
int client_socket;
int client_running = 1;

void client_signal_handler(int sig) {
    printf("\nDisconnecting from server...\n");
    send_to_client(client_socket, "STOP");
    client_running = 0;
    close(client_socket);
    exit(0);
}

void* receive_messages(void* arg) {
    char buffer[BUFFER_SIZE];

    while (client_running) {
        int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            if (client_running) {
                printf("\nConnection lost to server\n");
                client_running = 0;
            }
            break;
        }

        buffer[bytes_received] = '\0';

        if (strncmp(buffer, "PING", 4) == 0) {
            // Odpowiedz na ping od serwera (w tle, niewidoczne dla użytkownika)
            send(client_socket, "PONG\n", 5, MSG_NOSIGNAL);
            // Opcjonalnie: odkomentuj poniższą linię jeśli chcesz widzieć pingi w kliencie
            // printf("(Ping received from server)\n");
        } else {
            printf("%s", buffer);
            fflush(stdout);
            if (client_running) {
                printf("> ");
                fflush(stdout);
            }
        }
    }
    return NULL;
}

int main_client(const char* name, const char* server_ip, int server_port) {
    struct sockaddr_in server_addr;
    pthread_t receive_tid;
    char input[BUFFER_SIZE];

    signal(SIGINT, client_signal_handler);

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("Socket creation failed");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        return 1;
    }

    // Wyślij nazwę klienta
    send_to_client(client_socket, name);

    printf("Connected to chat server. You are: %s\n", name);
    printf("Commands: LIST, 2ALL <message>, 2ONE <name> <message>, STOP\n");

    // Uruchom wątek odbierający wiadomości
    pthread_create(&receive_tid, NULL, receive_messages, NULL);

    while (client_running) {
        printf("> ");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }

        if (strncmp(input, "STOP", 4) == 0) {
            send_to_client(client_socket, "STOP");
            break;
        }

        send_to_client(client_socket, input);
    }

    client_running = 0;
    close(client_socket);
    return 0;
}

// MAIN - określa czy uruchomić serwer czy klienta na podstawie argumentów
int main(int argc, char* argv[]) {
    if (argc == 2) {
        // Tryb serwera: ./chat <port>
        int port = atoi(argv[1]);
        if (port <= 0) {
            printf("Usage (server): %s <port>\n", argv[0]);
            return 1;
        }
        return main_server(port);
    }
    else if (argc == 4) {
        // Tryb klienta: ./chat <name> <server_ip> <port>
        const char* name = argv[1];
        const char* server_ip = argv[2];
        int server_port = atoi(argv[3]);

        if (strlen(name) >= MAX_NAME_LEN || server_port <= 0) {
            printf("Usage (client): %s <name> <server_ip> <port>\n", argv[0]);
            printf("Note: name must be less than %d characters\n", MAX_NAME_LEN);
            return 1;
        }

        return main_client(name, server_ip, server_port);
    }
    else {
        printf("Usage:\n");
        printf("  Server: %s <port>\n", argv[0]);
        printf("  Client: %s <name> <server_ip> <port>\n", argv[0]);
        return 1;
    }
}