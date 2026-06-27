#include "bid_processor.h"

static void *bid_processor_thread(void *arg) {
    BidProcessor *processor = (BidProcessor *)arg;

    while (processor->running) {
        Bid bid;

        sem_wait(&processor->queue.items_sem);

        if (!processor->running) {
            break;
        }

        if (bid_queue_dequeue(&processor->queue, &bid) && processor->handler) {
            processor->handler(&bid, processor->ctx);
        }
    }

    return NULL;
}

int bid_processor_init(BidProcessor *processor, BidHandler handler, void *ctx) {
    if (!processor || !handler) {
        return 0;
    }

    if (!bid_queue_init(&processor->queue)) {
        return 0;
    }

    processor->handler = handler;
    processor->ctx = ctx;
    processor->running = 0;
    return 1;
}

int bid_processor_start(BidProcessor *processor) {
    if (!processor) {
        return 0;
    }

    processor->running = 1;
    if (pthread_create(&processor->thread, NULL, bid_processor_thread, processor) != 0) {
        processor->running = 0;
        return 0;
    }

    return 1;
}

int bid_processor_submit(BidProcessor *processor, const Bid *bid) {
    if (!processor || !bid) {
        return 0;
    }

    return bid_queue_enqueue(&processor->queue, bid);
}

void bid_processor_destroy(BidProcessor *processor) {
    if (!processor) {
        return;
    }

    processor->running = 0;
    sem_post(&processor->queue.items_sem);
    pthread_join(processor->thread, NULL);
    bid_queue_destroy(&processor->queue);
}
