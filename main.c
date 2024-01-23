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

void createQueueI(TQueue *queue, int size) {
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

void destroyQueueI(TQueue *queue) {
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
bool subscribeI(TQueue *queue, int thread) {
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
void unsubscribeI(TQueue *queue, int thread) {
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

void putI(TQueue *queue, void *msg) {
    // Check if there is needed space in queue
    if (queue->msgNumber == queue->msgMax) {
        // Search for messages which can be deleted
        int deletedCount = 0;
        Message *tmp = queue->head;
        while (tmp->receivers == 0) {
            if (queue->head == queue->tail) {
                queue->tail = NULL;
            }
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
void *getI(TQueue *queue, int thread) {
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
int getAvailableI(TQueue *queue, int thread) {
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

void removeI(TQueue *queue, void *msg) {
    // Look for message position in queue
    // Check if its (1) only one message in queue, (2) head, (3) tail or (4) between them

    Message *messageToRemove;
    Message *nextMessage;
    // (1)
    if (queue->head->msg == msg && queue->head == queue->tail) {
        messageToRemove = queue->head;
        nextMessage = NULL;

        queue->head = NULL;
        queue->tail = NULL;
    }
    // (2)
    else if (queue->head->msg == msg) {
        messageToRemove = queue->head;

        // Detach head
        queue->head = queue->head->next;
        nextMessage = queue->head;
    }
    // (3)
    else if (queue->tail->msg == msg) {
        messageToRemove = queue->tail;
        nextMessage = NULL;

        // Find predecessor of tail
        Message *tmp = queue->head;
        while (tmp->next != queue->tail) {
            tmp = tmp->next;
        }

        // Change tail
        tmp->next = NULL;
        queue->tail = tmp;
    }
    // (4)
    else {
        // Find in between
        Message *tmp = queue->head;
        while (tmp->next->msg != msg) {
            tmp = tmp->next;
        }

        messageToRemove = tmp->next;
        tmp->next = messageToRemove->next;
        nextMessage = tmp->next;
    }

    // Update subscribers next message which points to removed message
    for (int i = 0; i < MAX_SUBS; i++) {
        if (queue->subscribers[i].nextMsg == messageToRemove)
            queue->subscribers[i].nextMsg = nextMessage;
    }

    // Delete message
    free(messageToRemove);
}

// TODO: Complete function
void setSizeI(TQueue *queue, int size);

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
    createQueueI(q, 3);

    subscribeI(q, threadId1);
    char msg1[] = "Message 1";
    putI(q, msg1);
    subscribeI(q, threadId2);
    char msg2[] = "Message 2";
    putI(q, msg2);
    char msg3[] = "Message 3";
    putI(q, msg3);

    getI(q, threadId1);
    removeI(q, msg2);
    removeI(q, msg3);
    printf("Next msg for thread1: %s", (char*)getI(q, threadId1));

    destroyQueueI(q);

    return 0;
}
