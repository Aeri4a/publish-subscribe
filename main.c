#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <string.h>
// #include <pthread.h>

#define QUEUE_EMPTY INT_MIN
#define MAX_SUBS 50

typedef struct Message {
    int recievers;
    char detail[50];
    struct Message *next;
} Message;

typedef struct {
    int threadId;
    Message *nextMsg;
} Subscriber;

typedef struct {
    int msgMax;
    int msgNumber;
    int subscribersNumber;
    Subscriber subscribers[MAX_SUBS];

    Message *tail;
    Message *head;
} TQueue;

void createQueue(TQueue *queue, int size) {
    queue->head = NULL;
    queue->tail = NULL;
    queue->msgMax = size;
    queue->msgNumber = 0;

    // Empty subscribers list
    queue->subscribersNumber = 0;
    for (int i = 0; i < MAX_SUBS; i++) {
        queue->subscribers[i].threadId = -1;
        queue->subscribers[i].nextMsg = NULL;
    }
}

// TODO
void destroyQueue();

// change thread type of pthread_t
bool subscribe(TQueue *queue, int thread) {
    queue->subscribersNumber += 1;
    for (int i = 0; i < MAX_SUBS; i++) {
        if (queue->subscribers[i].threadId == -1) {
            queue->subscribers[i].threadId = thread;
            return true;
        }
    }
    return false;

    // TODO
    // IT SHOULD RETURN I-POSITION FOR THREAD -> threadSubId
}

// change thread type of pthread_t
void unsubscribe(TQueue *queue, int thread) {
    Message *threadNextMsg;

    queue->subscribersNumber -= 1;
    for (int i = 0; i< MAX_SUBS; i++) {
        if (queue->subscribers[i].threadId == thread) {
            // Get its last message
            threadNextMsg = queue->subscribers[i].nextMsg;

            // Remove from subscribers list
            queue->subscribers[i].threadId = -1;
            queue->subscribers[i].nextMsg = NULL;
        }
    }

    // If has unread messages
    if (threadNextMsg != NULL) {
        // Traverse and decrement msg recievers starting from newest unread
        Message *tmp = queue->head;
    
        bool isFirstMsgFound = false;
        while(tmp != NULL) {
            if (tmp == threadNextMsg) {
                isFirstMsgFound = true;
                tmp->recievers -= 1;
            } else {
                if (isFirstMsgFound)
                    tmp->recievers -= 1;
            }
            tmp = tmp->next;
        }

        free(tmp);
    }
}

void put(TQueue *queue, char *message) {
    // Check if there is needed space in queue
    if (queue->msgNumber == queue->msgMax) {
        // Search for messages which can be deleted
        int deletedCount = 0;
        Message *tmp = queue->head;
        while (tmp->recievers != 0) {
            queue->head = tmp->next;
            deletedCount +=1;
            tmp = tmp->next;
        }

        // No free space for new msg, waiting there -> check if thread read sth
        if (deletedCount == 0) return;
        // There is some space
        else queue->msgNumber -= deletedCount;

        free(tmp);
    }

    // Create new Message from string msg
    Message *newMessage = malloc(sizeof(Message));
    if (newMessage == NULL) return false;

    newMessage->next = NULL;
    newMessage->recievers = queue->subscribersNumber;
    strcpy(newMessage->detail, message);

    // Enqueue new Message
    if (queue->tail != NULL) {
        queue->tail->next = newMessage;
    }

    queue->tail = newMessage;
    if (queue->head == NULL) {
        queue->head = newMessage;
    }

    // Update next message for subscribed threads with any unread messages
    for (int i = 0; i < MAX_SUBS; i++) {
        if (
            queue->subscribers[i].threadId != -1 &&
            queue->subscribers[i].nextMsg == NULL
        )
            queue->subscribers[i].nextMsg = newMessage;
    }
}

char *get(TQueue *queue, int threadSubId) {
    // Get newest message pointer
    Message *nextThreadMsg = queue->subscribers[threadSubId].nextMsg;
    if (nextThreadMsg == NULL) {
        // WAIT
    }

    // Get newest message by pointer
    Message *tmp = queue->head;
    while (tmp != NULL) {
        if (tmp == nextThreadMsg) {
             tmp->recievers -= 1;
             queue->subscribers[threadSubId].nextMsg = tmp->next; // save next message
             return tmp->detail;
        }
        tmp = tmp->next;
    }
}

// bool enqueue(TQueue *q, int num) {
//     Message *newNode = malloc(sizeof(Message));
//     if (newNode == NULL) return false;
//     newNode->value = num;
//     newNode->next = NULL;

//     if (q->tail != NULL) {
//         q->tail->next = newNode;
//     }

//     q->tail = newNode;
//     if (q->head == NULL) {
//         q->head = newNode;
//     }

//     return true;
// }

// int dequeue(TQueue *q) {
//     if (q->head == NULL) return QUEUE_EMPTY;
//     Message *tmpHead = q->head;    
//     int result = tmpHead->value;

//     q->head = q->head->next;
//     if (q->head == NULL) {
//         q->tail = NULL;
//     }

//     free(tmpHead);
//     return result;
// }

int main() {
    return 0;
}