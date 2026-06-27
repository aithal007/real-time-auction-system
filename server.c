#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "common.h"

#define SERVER_PORT 8080
#define BACKLOG 5

int main(void) {
    int server_fd;
    int client_sock;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    char welcome[] = "Welcome to Auction System";
    Request req;

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

        if (send(client_sock, welcome, sizeof(welcome), 0) < 0) {
            perror("send");
            close(client_sock);
            continue;
        }

        memset(&req, 0, sizeof(req));
        if (recv(client_sock, &req, sizeof(req), 0) > 0) {
            printf("Received request type: %d\n", req.type);
        }

        close(client_sock);
    }

    close(server_fd);
    return 0;
}
