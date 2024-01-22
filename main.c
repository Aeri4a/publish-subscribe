#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <string.h>
// #include <pthread.h>

#define QUEUE_EMPTY INT_MIN
#define MAX_SUBS 50

typedef struct Message {
    int receivers;
    void *msg;
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

void destroyQueue(TQueue *queue) {
    Message *tmp = queue->head;

    // Clear all Messages structures
    while (tmp != NULL) {
        queue->head = tmp->next;
        free(tmp);
        tmp = queue->head;        
    }

    // Clear rest of the TQueue structure
    free(queue);
}

// TODO: Change thread type of pthread_t
bool subscribe(TQueue *queue, int thread) {
    queue->subscribersNumber += 1;
    for (int i = 0; i < MAX_SUBS; i++) {
        if (queue->subscribers[i].threadId == -1) {
            queue->subscribers[i].threadId = thread;
            return true;
        }
    }
    return false;
}

// TODO: Change thread type of pthread_t
void unsubscribe(TQueue *queue, int thread) {
    Message *threadNextMsg;

    queue->subscribersNumber -= 1;
    // TODO: Change it for something faster like hashmap
    for (int i = 0; i< MAX_SUBS; i++) {
        if (queue->subscribers[i].threadId == thread) {
            // Get its last message
            threadNextMsg = queue->subscribers[i].nextMsg;

            // Remove from subscribers list
            queue->subscribers[i].threadId = -1;
            queue->subscribers[i].nextMsg = NULL;
            break;
        }
    }

    // If thread has unread messages then
    if (threadNextMsg != NULL) {
        // Traverse and decrement msg receivers number starting from newest unread
        Message *tmp = queue->head;
    
        bool isFirstMsgFound = false;
        while(tmp != NULL) {
            if (tmp == threadNextMsg) {
                isFirstMsgFound = true;
                tmp->receivers -= 1;
            } else {
                if (isFirstMsgFound)
                    tmp->receivers -= 1;
            }
            tmp = tmp->next;
        }
    }
}

void put(TQueue *queue, void *msg) {
    // Check if there is needed space in queue
    if (queue->msgNumber == queue->msgMax) {
        // Search for messages which can be deleted
        int deletedCount = 0;
        Message *tmp = queue->head;
        while (tmp->receivers == 0) {
            queue->head = tmp->next;
            free(tmp);
            deletedCount += 1;
            tmp = queue->head;
        }

        // No free space for new msg, waiting there -> check if thread read sth
        if (deletedCount == 0) return;
        // There is some space
        else queue->msgNumber -= deletedCount;
    }

    // Create new Message
    Message *newMessage = malloc(sizeof(Message));
    if (newMessage == NULL) return;

    newMessage->next = NULL;
    newMessage->receivers = queue->subscribersNumber;
    newMessage->msg = msg;

    // Enqueue new Message
    queue->msgNumber++;
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

// TODO: Change thread type of pthread_t
void *get(TQueue *queue, int thread) {
    int threadSubId;

    // TODO: Change it for something faster like hashmap
    // Find threadId in subscribers list
    for (int i = 0; i < MAX_SUBS; i++) {
        if (queue->subscribers[i].threadId == thread)
            threadSubId = i;
    }

    // Get newest message pointer
    Message *nextThreadMsg = queue->subscribers[threadSubId].nextMsg;

    
    if (nextThreadMsg == NULL) {
        // WAIT
    }

    // Get newest message by pointer
    Message *tmp = queue->head;
    while (tmp != NULL) {
        if (tmp == nextThreadMsg) {
             tmp->receivers -= 1;
             queue->subscribers[threadSubId].nextMsg = tmp->next; // save next message
             return tmp->msg;
        }
        tmp = tmp->next;
    }
}

// TODO: Change thread type of pthread_t
int getAvailable(TQueue *queue, int thread) {
    Message *nextThreadMsg;

    // TODO: Change it for something faster like hashmap
    // Find thread next unread message in subscribers list
    for (int i = 0; i < MAX_SUBS; i++) {
        if (queue->subscribers[i].threadId == thread) {
            nextThreadMsg = queue->subscribers[i].nextMsg;
        }
    }

    // Count unread messages starting from next unread
    int unreadMessages = 0;
    while (nextThreadMsg != NULL) {
        unreadMessages++;
        nextThreadMsg = nextThreadMsg->next;
    }

    return unreadMessages;
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
    // Temporary testing section
    int threadId1 = 5;
    int threadId2 = 6;
    TQueue *q = malloc(sizeof(TQueue));
    createQueue(q, 3);

    subscribe(q, threadId1);
    char msg1[] = "Message 1";
    put(q, msg1);
    subscribe(q, threadId2);
    char msg2[] = "Message 2";
    put(q, msg2);
    char msg3[] = "Message 3";
    put(q, msg3);

    printf("First message info: %d\n", q->head->receivers);
    printf("Second message info: %d\n", q->head->next->receivers);

    printf("Available messages: %d\n", getAvailable(q, threadId1));
    printf("Getting message: %s\n", (char*)get(q, threadId1));
    printf("Available messages: %d\n", getAvailable(q, threadId1));

    put(q, "Hello world!");

    printf("Msg num: %d\n", q->msgNumber);

    destroyQueue(q);

    return 0;
}
