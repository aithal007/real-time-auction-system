#include <arpa/inet.h>
#include <pthread.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "common.h"

#define SERVER_PORT 8080
#define BACKLOG 5

typedef struct {
    int sock;
} ThreadArgs;

static int send_all(int sock, const void *buffer, size_t length) {
    const char *ptr = (const char *)buffer;
    size_t sent = 0;

    while (sent < length) {
        ssize_t n = send(sock, ptr + sent, length - sent, 0);
        if (n <= 0) {
            return -1;
        }
        sent += (size_t)n;
    }

    return 0;
}

static int recv_all(int sock, void *buffer, size_t length) {
    char *ptr = (char *)buffer;
    size_t received = 0;

    while (received < length) {
        ssize_t n = recv(sock, ptr + received, length - received, 0);
        if (n <= 0) {
            return -1;
        }
        received += (size_t)n;
    }

    return 0;
}

static void send_response(int client_sock, const char *message) {
    char response[MAX_DATA];

    memset(response, 0, sizeof(response));
    snprintf(response, sizeof(response), "%s", message);
    if (send_all(client_sock, response, sizeof(response)) < 0) {
        perror("send");
    }
}

static void *handle_client(void *arg) {
    ThreadArgs *thread_args = (ThreadArgs *)arg;
    int client_sock = thread_args->sock;
    char welcome[MAX_DATA];
    Request req;
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];

    free(thread_args);

    memset(welcome, 0, sizeof(welcome));
    snprintf(welcome, sizeof(welcome), "Welcome to Auction System");
    if (send_all(client_sock, welcome, sizeof(welcome)) < 0) {
        perror("send");
        close(client_sock);
        return NULL;
    }

    while (1) {
        memset(&req, 0, sizeof(req));
        if (recv_all(client_sock, &req, sizeof(req)) < 0) {
            break;
        }

        printf("Received request type: %d\n", req.type);

        switch (req.type) {
            case 1:
                memset(username, 0, sizeof(username));
                memset(password, 0, sizeof(password));
                if (sscanf(req.data, "%49s %49s", username, password) == 2) {
                    if (login_user(username, password)) {
                        send_response(client_sock, "OK");
                        printf("Login success for %s\n", username);
                    } else {
                        send_response(client_sock, "FAIL");
                        printf("Login failed for %s\n", username);
                    }
                } else {
                    send_response(client_sock, "BAD_FORMAT");
                    printf("Invalid login data format\n");
                }
                break;
            case 2:
                memset(username, 0, sizeof(username));
                memset(password, 0, sizeof(password));
                if (sscanf(req.data, "%49s %49s", username, password) == 2) {
                    if (user_exists(username)) {
                        send_response(client_sock, "USER_EXISTS");
                        printf("Register rejected for existing user %s\n", username);
                    } else {
                        register_user(username, password);
                        send_response(client_sock, "OK");
                        printf("Registered new user %s\n", username);
                    }
                } else {
                    send_response(client_sock, "BAD_FORMAT");
                    printf("Invalid register data format\n");
                }
                break;
            case 3:
                break;
            case 4:
                break;
            case 5:
                break;
            case 6:
                break;
            case 7:
                break;
            case 8:
                break;
            default:
                printf("Unknown request type: %d\n", req.type);
                break;
        }
    }

    close(client_sock);
    return NULL;
}

int main(void) {
    int server_fd;
    int client_sock;
    pthread_t thread_id;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, BACKLOG) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("Server listening on port %d\n", SERVER_PORT);

    while (1) {
        client_sock = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock < 0) {
            perror("accept");
            continue;
        }

        printf("Client connected\n");

        ThreadArgs *thread_args = malloc(sizeof(*thread_args));
        if (!thread_args) {
            perror("malloc");
            close(client_sock);
            continue;
        }

        thread_args->sock = client_sock;

        if (pthread_create(&thread_id, NULL, handle_client, thread_args) != 0) {
            perror("pthread_create");
            free(thread_args);
            close(client_sock);
            continue;
        }

        pthread_detach(thread_id);
    }

    close(server_fd);
    return 0;
}
