#include "notification.h"

#include <string.h>

static void *notification_processor_thread(void *arg) {
    NotificationProcessor *processor = (NotificationProcessor *)arg;

    while (processor->running) {
        Notification notif;

        sem_wait(&processor->queue.items_sem);

        if (!processor->running) {
            break;
        }

        if (notification_queue_dequeue(&processor->queue, &notif) && processor->handler) {
            processor->handler(&notif, processor->ctx);
        }
    }

    return NULL;
}

int notification_queue_init(NotificationQueue *queue) {
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

void notification_queue_destroy(NotificationQueue *queue) {
    if (!queue) {
        return;
    }

    sem_destroy(&queue->items_sem);
    pthread_mutex_destroy(&queue->lock);
}

int notification_queue_enqueue(NotificationQueue *queue, const Notification *notif) {
    if (!queue || !notif) {
        return 0;
    }

    pthread_mutex_lock(&queue->lock);

    if (queue->count >= NOTIF_QUEUE_SIZE) {
        pthread_mutex_unlock(&queue->lock);
        return 0;
    }

    queue->items[queue->rear] = *notif;
    queue->rear = (queue->rear + 1) % NOTIF_QUEUE_SIZE;
    queue->count++;
    sem_post(&queue->items_sem);
    pthread_mutex_unlock(&queue->lock);
    return 1;
}

int notification_queue_dequeue(NotificationQueue *queue, Notification *notif) {
    if (!queue || !notif) {
        return 0;
    }

    pthread_mutex_lock(&queue->lock);

    if (queue->count <= 0) {
        pthread_mutex_unlock(&queue->lock);
        return 0;
    }

    *notif = queue->items[queue->front];
    queue->front = (queue->front + 1) % NOTIF_QUEUE_SIZE;
    queue->count--;

    pthread_mutex_unlock(&queue->lock);
    return 1;
}

int notification_processor_init(NotificationProcessor *processor,
                                int (*handler)(const Notification *notif, void *ctx),
                                void *ctx) {
    if (!processor || !handler) {
        return 0;
    }

    if (!notification_queue_init(&processor->queue)) {
        return 0;
    }

    processor->handler = handler;
    processor->ctx = ctx;
    processor->running = 0;
    return 1;
}

int notification_processor_start(NotificationProcessor *processor) {
    if (!processor) {
        return 0;
    }

    processor->running = 1;
    if (pthread_create(&processor->thread, NULL, notification_processor_thread, processor) != 0) {
        processor->running = 0;
        return 0;
    }

    return 1;
}

int notification_processor_submit(NotificationProcessor *processor, const Notification *notif) {
    if (!processor || !notif) {
        return 0;
    }

    return notification_queue_enqueue(&processor->queue, notif);
}

void notification_processor_destroy(NotificationProcessor *processor) {
    if (!processor) {
        return;
    }

    processor->running = 0;
    sem_post(&processor->queue.items_sem);
    pthread_join(processor->thread, NULL);
    notification_queue_destroy(&processor->queue);
}
