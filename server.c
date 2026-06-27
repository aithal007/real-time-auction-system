#include <arpa/inet.h>
#include <pthread.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "bid_processor.h"
#include "notification.h"
#include "common.h"

#define SERVER_PORT 8080
#define BACKLOG 5
#define BIDS_FILE "data/bids.txt"

static pthread_mutex_t auction_lock = PTHREAD_MUTEX_INITIALIZER;
static Auction auctions[MAX_AUCTIONS];
static int auction_count = 0;
static BidProcessor bid_processor;
static NotificationProcessor notification_processor;

typedef struct {
    int sock;
} ThreadArgs;

typedef struct {
    int auction_id;
    int duration;
} TimerArgs;

typedef struct {
    int auction_id;
    char bidder[MAX_USERNAME];
    float amount;
} BidEntry;

typedef struct {
    char username[MAX_USERNAME];
    char message[MAX_DATA];
} NotificationEntry;

static NotificationEntry notification_inbox[200];
static int notification_inbox_count = 0;
static pthread_mutex_t notification_lock = PTHREAD_MUTEX_INITIALIZER;

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

static void send_response_for_user(int client_sock, const char *username, const char *message) {
    char response[MAX_DATA];
    size_t offset = 0;
    int i;

    memset(response, 0, sizeof(response));

    pthread_mutex_lock(&notification_lock);
    for (i = 0; i < notification_inbox_count && offset < sizeof(response); i++) {
        if (strcmp(notification_inbox[i].username, username) == 0) {
            offset += (size_t)snprintf(response + offset, sizeof(response) - offset, "[NOTIF] %s\n", notification_inbox[i].message);
            notification_inbox[i] = notification_inbox[notification_inbox_count - 1];
            notification_inbox_count--;
            i--;
        }
    }
    pthread_mutex_unlock(&notification_lock);

    if (offset < sizeof(response)) {
        snprintf(response + offset, sizeof(response) - offset, "%s", message);
    }

    if (send_all(client_sock, response, sizeof(response)) < 0) {
        perror("send");
    }
}

static int notification_handler(const Notification *notif, void *ctx) {
    (void)ctx;

    pthread_mutex_lock(&notification_lock);
    if (notification_inbox_count < 200) {
        snprintf(notification_inbox[notification_inbox_count].username, sizeof(notification_inbox[notification_inbox_count].username), "%s", notif->username);
        snprintf(notification_inbox[notification_inbox_count].message, sizeof(notification_inbox[notification_inbox_count].message), "%s", notif->message);
        notification_inbox_count++;
    }
    pthread_mutex_unlock(&notification_lock);

    return 1;
}

static void enqueue_notification(const char *username, const char *message) {
    Notification notif;

    snprintf(notif.username, sizeof(notif.username), "%s", username);
    snprintf(notif.message, sizeof(notif.message), "%s", message);
    notification_processor_submit(&notification_processor, &notif);
}

static void log_bid(int auction_id, const char *bidder, float amount) {
    FILE *fp = fopen(BIDS_FILE, "a");

    if (!fp) {
        perror("fopen");
        return;
    }

    fprintf(fp, "%d %s %.2f\n", auction_id, bidder, amount);
    fclose(fp);
}

static int find_highest_bid_for_auction(int auction_id, BidEntry *best_bid) {
    FILE *fp = fopen(BIDS_FILE, "r");
    BidEntry current;
    int found = 0;

    if (!fp) {
        return 0;
    }

    while (fscanf(fp, "%d %49s %f", &current.auction_id, current.bidder, &current.amount) == 3) {
        if (current.auction_id == auction_id) {
            if (!found || current.amount > best_bid->amount) {
                *best_bid = current;
                found = 1;
            }
        }
    }

    fclose(fp);
    return found;
}

static int find_auction_index(int auction_id) {
    int i;

    for (i = 0; i < auction_count; i++) {
        if (auctions[i].id == auction_id) {
            return i;
        }
    }

    return -1;
}

static int process_bid(const Bid *bid, void *ctx) {
    (void)ctx;

    int i = find_auction_index(bid->auction_id);
    char previous_bidder[MAX_USERNAME];

    if (i < 0) {
        return -1;
    }

    if (!auctions[i].status) {
        return -2;
    }

    if (bid->amount > auctions[i].currentBid) {
        memset(previous_bidder, 0, sizeof(previous_bidder));
        if (auctions[i].highestBidder[0] != '\0') {
            snprintf(previous_bidder, sizeof(previous_bidder), "%s", auctions[i].highestBidder);
        }

        auctions[i].currentBid = bid->amount;
        snprintf(auctions[i].highestBidder, sizeof(auctions[i].highestBidder), "%s", bid->bidder);
        log_bid(bid->auction_id, bid->bidder, bid->amount);
        save_auctions(auctions, auction_count);

        if (previous_bidder[0] != '\0' && strcmp(previous_bidder, bid->bidder) != 0) {
            enqueue_notification(previous_bidder, "You have been outbid!");
        }

        return 1;
    }

    return 0;
}

static void close_auction(int auction_id) {
    int index = find_auction_index(auction_id);

    if (index >= 0) {
        BidEntry best_bid;

        if (find_highest_bid_for_auction(auction_id, &best_bid)) {
            auctions[index].currentBid = best_bid.amount;
            snprintf(auctions[index].highestBidder, sizeof(auctions[index].highestBidder), "%s", best_bid.bidder);
        }

        auctions[index].status = 0;
        auctions[index].timeLeft = 0;
        save_auctions(auctions, auction_count);
        if (auctions[index].highestBidder[0] != '\0') {
            enqueue_notification(auctions[index].highestBidder, "Congratulations! You won the auction!");
            printf("Auction %d closed. Winner: %s\n", auction_id, auctions[index].highestBidder);
        } else {
            printf("Auction %d closed with no bids\n", auction_id);
        }
    }
}

static void *auction_timer(void *arg) {
    TimerArgs *timer_args = (TimerArgs *)arg;
    int auction_id = timer_args->auction_id;

    free(timer_args);

    while (1) {
        sleep(1);

        pthread_mutex_lock(&auction_lock);

        if (auction_count == 0) {
            pthread_mutex_unlock(&auction_lock);
            break;
        }

        {
            int index = find_auction_index(auction_id);

            if (index < 0 || auctions[index].status == 0) {
                pthread_mutex_unlock(&auction_lock);
                break;
            }

            if (auctions[index].timeLeft > 0) {
                auctions[index].timeLeft--;
            }

            if (auctions[index].timeLeft <= 0) {
                close_auction(auction_id);
                pthread_mutex_unlock(&auction_lock);
                break;
            }
        }

        pthread_mutex_unlock(&auction_lock);
    }

    return NULL;
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
        if (auctions[i].status == 0 && auctions[i].highestBidder[0] != '\0') {
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
    Bid bid;

    bid.auction_id = auction_id;
    snprintf(bid.bidder, sizeof(bid.bidder), "%s", bidder);
    bid.amount = amount;

    return bid_processor_submit(&bid_processor, &bid) ? 2 : 0;
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

    if (!save_auctions(auctions, *auction_count)) {
        return 0;
    }

    if (duration > 0) {
        pthread_t timer_thread;
        TimerArgs *timer_args = malloc(sizeof(*timer_args));

        if (timer_args) {
            timer_args->auction_id = new_id;
            timer_args->duration = duration;
            if (pthread_create(&timer_thread, NULL, auction_timer, timer_args) == 0) {
                pthread_detach(timer_thread);
            } else {
                free(timer_args);
            }
        }
    }

    return 1;
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
                        send_response_for_user(client_sock, username, "OK");
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
                        send_response_for_user(client_sock, seller, "OK");
                        printf("Auction created: %s by %s\n", item, seller);
                    } else {
                        send_response_for_user(client_sock, seller, "FAIL");
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
                    int result = place_bid(auction_id, bidder, bid_amount);

                    if (result == 2) {
                        send_response_for_user(client_sock, bidder, "QUEUED");
                        printf("Bid queued: %s on auction %d\n", bidder, auction_id);
                    } else {
                        send_response(client_sock, "FAIL");
                        printf("Failed to queue bid for auction %d\n", auction_id);
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

    if (!bid_processor_init(&bid_processor, process_bid, NULL)) {
        fprintf(stderr, "Failed to initialize bid processor\n");
        close(server_fd);
        return 1;
    }

    if (!notification_processor_init(&notification_processor, notification_handler, NULL)) {
        fprintf(stderr, "Failed to initialize notification processor\n");
        bid_processor_destroy(&bid_processor);
        close(server_fd);
        return 1;
    }

    pthread_mutex_lock(&auction_lock);
    auction_count = load_auctions(auctions, MAX_AUCTIONS);
    pthread_mutex_unlock(&auction_lock);

    {
        int i;
        for (i = 0; i < auction_count; i++) {
            if (auctions[i].status == 1 && auctions[i].timeLeft > 0) {
                pthread_t timer_thread;
                TimerArgs *timer_args = malloc(sizeof(*timer_args));
                if (timer_args) {
                    timer_args->auction_id = auctions[i].id;
                    timer_args->duration = auctions[i].timeLeft;
                    if (pthread_create(&timer_thread, NULL, auction_timer, timer_args) == 0) {
                        pthread_detach(timer_thread);
                    } else {
                        free(timer_args);
                    }
                }
            }
        }
    }

    if (!bid_processor_start(&bid_processor)) {
        fprintf(stderr, "Failed to start bid processor\n");
        notification_processor_destroy(&notification_processor);
        bid_processor_destroy(&bid_processor);
        close(server_fd);
        return 1;
    }

    if (!notification_processor_start(&notification_processor)) {
        fprintf(stderr, "Failed to start notification processor\n");
        notification_processor_destroy(&notification_processor);
        bid_processor_destroy(&bid_processor);
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
    bid_processor_destroy(&bid_processor);
    return 0;
}
