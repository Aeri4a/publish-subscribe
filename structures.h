#pragma once

#define MAX_SUBS 50 // Max number of subscribers

typedef struct Message {
    int receivers;
    void *msg;
    struct Message *next;
} Message;

typedef struct {
    pthread_t threadId;
    Message *nextMsg;
} Subscriber;

typedef struct {
    // Pthread section
    pthread_mutex_t mutex;
    pthread_cond_t msgGetCall;
    pthread_cond_t msgPutCall;

    // Management section
    int msgMax;
    int msgNumber;
    bool exitFlag;
    int exitMode; // 0 - no exit, 1 - publishers quit, 2 - subscribers quit
    int activePublishers;
    int activeSubscribers;

    // Subscribers info
    int subscribersNumber;
    Subscriber subscribers[MAX_SUBS];

    // Queue base structure
    Message *tail;
    Message *head;
} TQueue;
