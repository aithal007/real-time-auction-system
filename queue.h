#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include <semaphore.h>

#include "common.h"

#define BID_QUEUE_SIZE 100

typedef struct Bid {
    int auction_id;
    char bidder[MAX_USERNAME];
    float amount;
} Bid;

typedef struct BidQueue {
    Bid items[BID_QUEUE_SIZE];
    int front;
    int rear;
    int count;
    pthread_mutex_t lock;
    sem_t items_sem;
} BidQueue;

int bid_queue_init(BidQueue *queue);
void bid_queue_destroy(BidQueue *queue);
int bid_queue_enqueue(BidQueue *queue, const Bid *bid);
int bid_queue_dequeue(BidQueue *queue, Bid *bid);

#endif
