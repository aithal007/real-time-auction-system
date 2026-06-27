#ifndef NOTIFICATION_H
#define NOTIFICATION_H

#include <pthread.h>
#include <semaphore.h>

#include "common.h"

#define NOTIF_QUEUE_SIZE 100

typedef struct Notification {
    char username[MAX_USERNAME];
    char message[MAX_DATA];
} Notification;

typedef struct NotificationQueue {
    Notification items[NOTIF_QUEUE_SIZE];
    int front;
    int rear;
    int count;
    pthread_mutex_t lock;
    sem_t items_sem;
} NotificationQueue;

typedef struct NotificationProcessor {
    NotificationQueue queue;
    pthread_t thread;
    int running;
    void *ctx;
    int (*handler)(const Notification *notif, void *ctx);
} NotificationProcessor;

int notification_queue_init(NotificationQueue *queue);
void notification_queue_destroy(NotificationQueue *queue);
int notification_queue_enqueue(NotificationQueue *queue, const Notification *notif);
int notification_queue_dequeue(NotificationQueue *queue, Notification *notif);

int notification_processor_init(NotificationProcessor *processor,
                                int (*handler)(const Notification *notif, void *ctx),
                                void *ctx);
int notification_processor_start(NotificationProcessor *processor);
int notification_processor_submit(NotificationProcessor *processor, const Notification *notif);
void notification_processor_destroy(NotificationProcessor *processor);

#endif
