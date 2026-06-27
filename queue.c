#include "queue.h"

#include <string.h>

int bid_queue_init(BidQueue *queue) {
    if (!queue) {
        return 0;
    }

    memset(queue, 0, sizeof(*queue));

    if (pthread_mutex_init(&queue->lock, NULL) != 0) {
        return 0;
    }

    if (sem_init(&queue->items_sem, 0, 0) != 0) {
        pthread_mutex_destroy(&queue->lock);
        return 0;
    }

    return 1;
}

void bid_queue_destroy(BidQueue *queue) {
    if (!queue) {
        return;
    }

    sem_destroy(&queue->items_sem);
    pthread_mutex_destroy(&queue->lock);
}

int bid_queue_enqueue(BidQueue *queue, const Bid *bid) {
    if (!queue || !bid) {
        return 0;
    }

    pthread_mutex_lock(&queue->lock);

    if (queue->count >= BID_QUEUE_SIZE) {
        pthread_mutex_unlock(&queue->lock);
        return 0;
    }

    queue->items[queue->rear] = *bid;
    queue->rear = (queue->rear + 1) % BID_QUEUE_SIZE;
    queue->count++;
    sem_post(&queue->items_sem);
    pthread_mutex_unlock(&queue->lock);
    return 1;
}

int bid_queue_dequeue(BidQueue *queue, Bid *bid) {
    if (!queue || !bid) {
        return 0;
    }

    pthread_mutex_lock(&queue->lock);

    if (queue->count <= 0) {
        pthread_mutex_unlock(&queue->lock);
        return 0;
    }

    *bid = queue->items[queue->front];
    queue->front = (queue->front + 1) % BID_QUEUE_SIZE;
    queue->count--;

    pthread_mutex_unlock(&queue->lock);
    return 1;
}
