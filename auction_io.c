#include <stdio.h>

#include "common.h"

#define AUCTIONS_FILE "data/auctions.txt"

int load_auctions(Auction auctions[], int max_auctions) {
    FILE *fp = fopen(AUCTIONS_FILE, "r");
    int count = 0;

    if (!fp) {
        return 0;
    }

    while (count < max_auctions) {
        Auction auction;
        int scanned = fscanf(fp, "%d %99s %49s %f %49s %d %d",
                             &auction.id,
                             auction.item,
                             auction.seller,
                             &auction.currentBid,
                             auction.highestBidder,
                             &auction.status,
                             &auction.timeLeft);

        if (scanned != 7) {
            break;
        }

        auctions[count++] = auction;
    }

    fclose(fp);
    return count;
}

int save_auctions(const Auction auctions[], int auction_count) {
    FILE *fp = fopen(AUCTIONS_FILE, "w");
    int i;

    if (!fp) {
        return 0;
    }

    for (i = 0; i < auction_count; i++) {
        fprintf(fp, "%d %s %s %.2f %s %d %d\n",
                auctions[i].id,
                auctions[i].item,
                auctions[i].seller,
                auctions[i].currentBid,
                auctions[i].highestBidder,
                auctions[i].status,
                auctions[i].timeLeft);
    }

    fclose(fp);
    return 1;
}
