#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "common.h"

#define SERVER_PORT 8080

static void show_menu(void) {
    puts("\n--- Auction System ---");
    puts("1. Login");
    puts("2. Register");
    puts("3. Create Auction");
    puts("4. View Auctions");
    puts("5. Place Bid");
    puts("6. My Auctions");
    puts("7. View Winners");
    puts("8. Exit");
    printf("Enter choice: ");
}

int main(void) {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[256];
    Request req;
    int choice;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock);
        return 1;
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }

    memset(buffer, 0, sizeof(buffer));
    if (recv(sock, buffer, sizeof(buffer) - 1, 0) > 0) {
        printf("%s\n", buffer);
    }

    show_menu();
    if (scanf("%d", &choice) != 1) {
        close(sock);
        return 1;
    }

    memset(&req, 0, sizeof(req));
    req.type = choice;
    snprintf(req.data, sizeof(req.data), "choice=%d", choice);

    if (send(sock, &req, sizeof(req), 0) < 0) {
        perror("send");
        close(sock);
        return 1;
    }

    close(sock);
    return 0;
}
