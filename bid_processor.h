#ifndef BID_PROCESSOR_H
#define BID_PROCESSOR_H

#include "queue.h"

typedef int (*BidHandler)(const Bid *bid, void *ctx);

typedef struct BidProcessor {
    BidQueue queue;
    pthread_t thread;
    BidHandler handler;
    void *ctx;
    int running;
} BidProcessor;

int bid_processor_init(BidProcessor *processor, BidHandler handler, void *ctx);
int bid_processor_start(BidProcessor *processor);
int bid_processor_submit(BidProcessor *processor, const Bid *bid);
void bid_processor_destroy(BidProcessor *processor);

#endif
