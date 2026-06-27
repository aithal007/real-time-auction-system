#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "common.h"

#define SERVER_PORT 8080

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

static void read_line(char *buffer, size_t size) {
    if (fgets(buffer, size, stdin) == NULL) {
        buffer[0] = '\0';
        return;
    }

    buffer[strcspn(buffer, "\n")] = '\0';
}

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
    char input[256];
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
    int running = 1;

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
    if (recv_all(sock, buffer, sizeof(buffer)) == 0) {
        printf("%s\n", buffer);
    }

    while (running) {
        show_menu();
        read_line(input, sizeof(input));
        if (sscanf(input, "%d", &choice) != 1) {
            printf("Invalid choice\n");
            continue;
        }

        if (choice == 8) {
            running = 0;
            break;
        }

        memset(&req, 0, sizeof(req));
        req.type = choice;

        if (choice == 1 || choice == 2) {
            printf("Username: ");
            read_line(username, sizeof(username));

            printf("Password: ");
            read_line(password, sizeof(password));

            snprintf(req.data, sizeof(req.data), "%s %s", username, password);
        } else {
            snprintf(req.data, sizeof(req.data), "choice=%d", choice);
        }

        if (send_all(sock, &req, sizeof(req)) < 0) {
            perror("send");
            close(sock);
            return 1;
        }

        memset(buffer, 0, sizeof(buffer));
        if (recv_all(sock, buffer, sizeof(buffer)) == 0) {
            printf("Server response: %s\n", buffer);
        }
    }

    close(sock);
    return 0;
}
