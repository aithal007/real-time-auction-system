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

static pthread_mutex_t auction_lock = PTHREAD_MUTEX_INITIALIZER;
static Auction auctions[MAX_AUCTIONS];
static int auction_count = 0;

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

static void view_auctions(int client_sock) {
    char response[MAX_DATA];
    size_t offset = 0;
    int i;

    memset(response, 0, sizeof(response));

    if (auction_count == 0) {
        snprintf(response, sizeof(response), "No auctions available");
        send_response(client_sock, response);
        return;
    }

    offset += (size_t)snprintf(response + offset, sizeof(response) - offset, "ID | Item | Seller | Bid | Bidder | Status | TimeLeft\n");
    for (i = 0; i < auction_count && offset < sizeof(response); i++) {
        offset += (size_t)snprintf(
            response + offset,
            sizeof(response) - offset,
            "%d | %s | %s | %.2f | %s | %s | %d\n",
            auctions[i].id,
            auctions[i].item,
            auctions[i].seller,
            auctions[i].currentBid,
            auctions[i].highestBidder[0] ? auctions[i].highestBidder : "-",
            auctions[i].status ? "OPEN" : "ENDED",
            auctions[i].timeLeft
        );
    }

    send_response(client_sock, response);
}

static void view_my_auctions(int client_sock, const char *seller_name) {
    char response[MAX_DATA];
    size_t offset = 0;
    int i;
    int found = 0;

    memset(response, 0, sizeof(response));

    offset += (size_t)snprintf(response + offset, sizeof(response) - offset, "My Auctions for %s\n", seller_name);
    offset += (size_t)snprintf(response + offset, sizeof(response) - offset, "ID | Item | Bid | Bidder | Status | TimeLeft\n");

    for (i = 0; i < auction_count && offset < sizeof(response); i++) {
        if (strcmp(auctions[i].seller, seller_name) == 0) {
            found = 1;
            offset += (size_t)snprintf(
                response + offset,
                sizeof(response) - offset,
                "%d | %s | %.2f | %s | %s | %d\n",
                auctions[i].id,
                auctions[i].item,
                auctions[i].currentBid,
                auctions[i].highestBidder[0] ? auctions[i].highestBidder : "-",
                auctions[i].status ? "OPEN" : "ENDED",
                auctions[i].timeLeft
            );
        }
    }

    if (!found) {
        snprintf(response, sizeof(response), "No auctions found for %s", seller_name);
    }

    send_response(client_sock, response);
}

static void view_winners(int client_sock) {
    char response[MAX_DATA];
    size_t offset = 0;
    int i;
    int found = 0;

    memset(response, 0, sizeof(response));

    offset += (size_t)snprintf(response + offset, sizeof(response) - offset, "Winners\n");
    offset += (size_t)snprintf(response + offset, sizeof(response) - offset, "ID | Item | Winner | Bid\n");

    for (i = 0; i < auction_count && offset < sizeof(response); i++) {
        if (auctions[i].highestBidder[0] != '\0') {
            found = 1;
            offset += (size_t)snprintf(
                response + offset,
                sizeof(response) - offset,
                "%d | %s | %s | %.2f\n",
                auctions[i].id,
                auctions[i].item,
                auctions[i].highestBidder,
                auctions[i].currentBid
            );
        }
    }

    if (!found) {
        snprintf(response, sizeof(response), "No winners yet");
    }

    send_response(client_sock, response);
}

static int place_bid(int auction_id, const char *bidder, float amount) {
    int i;

    for (i = 0; i < auction_count; i++) {
        if (auctions[i].id == auction_id) {
            if (amount > auctions[i].currentBid) {
                auctions[i].currentBid = amount;
                snprintf(auctions[i].highestBidder, sizeof(auctions[i].highestBidder), "%s", bidder);
                save_auctions(auctions, auction_count);
                return 1;
            }
            return 0;
        }
    }

    return -1;
}

int create_auction(Auction auctions[], int *auction_count, const char *item, const char *seller, float start_bid, int duration) {
    int new_id;

    if (*auction_count >= MAX_AUCTIONS) {
        return 0;
    }

    new_id = (*auction_count == 0) ? 1 : auctions[*auction_count - 1].id + 1;

    auctions[*auction_count].id = new_id;
    snprintf(auctions[*auction_count].item, sizeof(auctions[*auction_count].item), "%s", item);
    snprintf(auctions[*auction_count].seller, sizeof(auctions[*auction_count].seller), "%s", seller);
    auctions[*auction_count].currentBid = start_bid;
    auctions[*auction_count].highestBidder[0] = '\0';
    auctions[*auction_count].status = 1;
    auctions[*auction_count].timeLeft = duration;

    (*auction_count)++;

    return save_auctions(auctions, *auction_count);
}

static void *handle_client(void *arg) {
    ThreadArgs *thread_args = (ThreadArgs *)arg;
    int client_sock = thread_args->sock;
    char welcome[MAX_DATA];
    Request req;
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
    char item[MAX_ITEM];
    char seller[MAX_USERNAME];
    float start_bid;
    int duration;
    int auction_id;
    char bidder[MAX_USERNAME];
    float bid_amount;
    char seller_filter[MAX_USERNAME];

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
                memset(item, 0, sizeof(item));
                memset(seller, 0, sizeof(seller));
                start_bid = 0.0f;
                duration = 0;

                if (sscanf(req.data, "%99s %49s %f %d", item, seller, &start_bid, &duration) == 4) {
                    pthread_mutex_lock(&auction_lock);
                    if (create_auction(auctions, &auction_count, item, seller, start_bid, duration)) {
                        send_response(client_sock, "OK");
                        printf("Auction created: %s by %s\n", item, seller);
                    } else {
                        send_response(client_sock, "FAIL");
                        printf("Failed to create auction for %s\n", item);
                    }
                    pthread_mutex_unlock(&auction_lock);
                } else {
                    send_response(client_sock, "BAD_FORMAT");
                    printf("Invalid create auction format\n");
                }
                break;
            case 4:
                pthread_mutex_lock(&auction_lock);
                view_auctions(client_sock);
                pthread_mutex_unlock(&auction_lock);
                break;
            case 5:
                auction_id = 0;
                memset(bidder, 0, sizeof(bidder));
                bid_amount = 0.0f;

                if (sscanf(req.data, "%d %49s %f", &auction_id, bidder, &bid_amount) == 3) {
                    pthread_mutex_lock(&auction_lock);
                    int result = place_bid(auction_id, bidder, bid_amount);
                    pthread_mutex_unlock(&auction_lock);

                    if (result == 1) {
                        send_response(client_sock, "OK");
                        printf("Bid accepted: %s on auction %d\n", bidder, auction_id);
                    } else if (result == 0) {
                        send_response(client_sock, "LOW_BID");
                        printf("Bid too low from %s on auction %d\n", bidder, auction_id);
                    } else {
                        send_response(client_sock, "NOT_FOUND");
                        printf("Auction %d not found for bid\n", auction_id);
                    }
                } else {
                    send_response(client_sock, "BAD_FORMAT");
                    printf("Invalid bid format\n");
                }
                break;
            case 6:
                memset(seller_filter, 0, sizeof(seller_filter));
                if (sscanf(req.data, "%49s", seller_filter) == 1) {
                    pthread_mutex_lock(&auction_lock);
                    view_my_auctions(client_sock, seller_filter);
                    pthread_mutex_unlock(&auction_lock);
                } else {
                    send_response(client_sock, "BAD_FORMAT");
                    printf("Invalid my auctions format\n");
                }
                break;
            case 7:
                pthread_mutex_lock(&auction_lock);
                view_winners(client_sock);
                pthread_mutex_unlock(&auction_lock);
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

    pthread_mutex_lock(&auction_lock);
    auction_count = load_auctions(auctions, MAX_AUCTIONS);
    pthread_mutex_unlock(&auction_lock);

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
