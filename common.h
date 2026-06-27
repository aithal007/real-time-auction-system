#ifndef COMMON_H
#define COMMON_H

#define MAX_DATA 256
#define MAX_USERNAME 50
#define MAX_PASSWORD 50
#define MAX_ITEM 100

typedef struct Request {
    int type;
    char data[MAX_DATA];
} Request;

typedef struct Auction {
    int id;
    char item[MAX_ITEM];
    char seller[MAX_USERNAME];
    float currentBid;
    char highestBidder[MAX_USERNAME];
    int status;
    int timeLeft;
} Auction;

#define MAX_AUCTIONS 100

int user_exists(const char *username);
int login_user(const char *username, const char *password);
void register_user(const char *username, const char *password);
int load_auctions(Auction auctions[], int max_auctions);
int save_auctions(const Auction auctions[], int auction_count);
int create_auction(Auction auctions[], int *auction_count, const char *item, const char *seller, float start_bid, int duration);

#endif
