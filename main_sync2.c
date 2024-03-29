#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

// #define QUEUE_EMPTY INT_MIN
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


// -= Interfaces (I) =-

void createQueueI(TQueue *queue, int size) {
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->msgGetCall, NULL);
    pthread_cond_init(&queue->msgPutCall, NULL);

    queue->head = NULL;
    queue->tail = NULL;
    queue->msgMax = size;
    queue->msgNumber = 0;
    queue->exitFlag = false;
    queue->exitMode = 0;
    queue->activePublishers = 0;
    queue->activeSubscribers = 0;

    // Empty subscribers list
    queue->subscribersNumber = 0;
    for (int i = 0; i < MAX_SUBS; i++) {
        queue->subscribers[i].threadId = -1;
        queue->subscribers[i].nextMsg = NULL;
    }
}

void destroyQueueI(TQueue *queue) {
    pthread_mutex_lock(&queue->mutex);
    printf("[D] - Preparation for destroying queue\n");

    queue->exitFlag = true;
    // Wait for all "waiting on condition" threads
    // 1. publishers
    queue->exitMode = 1;
    while (queue->activePublishers != 0) {
        pthread_cond_broadcast(&queue->msgPutCall);
        pthread_cond_wait(&queue->msgPutCall, &queue->mutex);
    }
    printf("[D] - Removed all waiting publishers\n");

    // 2. subscribers
    queue->exitMode = 2;
    while (queue->activeSubscribers != 0) {
        pthread_cond_broadcast(&queue->msgGetCall);
        pthread_cond_wait(&queue->msgGetCall, &queue->mutex);
    }
    printf("[D] - Removed all waiting subscribers\n");

    // Clear all Messages structures
    Message *tmp = queue->head;
    while (tmp != NULL) {
        queue->head = tmp->next;
        free(tmp);
        tmp = queue->head;        
    }

    pthread_mutex_t *copy = &queue->mutex;
    free(queue);
    // pthread_mutex_unlock(&queue->mutex);
    // pthread_mutex_destroy(&queue->mutex);
    pthread_mutex_unlock(copy);
    pthread_mutex_destroy(copy);

    // Clear rest of the TQueue structure
    printf("[D] - Queue destroyed\n");
}

bool subscribeI(TQueue *queue, pthread_t thread) {
    pthread_mutex_lock(&queue->mutex);
    for (int i = 0; i < MAX_SUBS; i++) {
        if (queue->subscribers[i].threadId == -1) {
            queue->subscribers[i].threadId = thread;
            queue->subscribersNumber += 1;
            pthread_mutex_unlock(&queue->mutex);
            pthread_cond_broadcast(&queue->msgPutCall);
            printf("[S] - Thread subscribed to queue\n");
            return true;
        }
    }
    pthread_mutex_unlock(&queue->mutex);
    return false;
}

void unsubscribeI(TQueue *queue, pthread_t thread) {
    pthread_mutex_lock(&queue->mutex);
    Message *threadNextMsg;

    // TODO: Change it for something faster like hashmap
    for (int i = 0; i < MAX_SUBS; i++) {
        if (queue->subscribers[i].threadId == thread) {
            queue->subscribersNumber -= 1;

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
                if (isFirstMsgFound) {
                    tmp->receivers -= 1;
                }
            }

            // Check if there is Message with no receivers
            if (tmp->receivers == 0) {
                printf("[S] - Found empty message | Deleting message\n");
                queue->msgNumber -= 1;
                if (queue->head == queue->tail) {
                    queue->tail = tmp->next;
                }
                queue->head = tmp->next;
                tmp->msg = NULL;
                free(tmp);
             }

            tmp = tmp->next;
        }
    }

    pthread_cond_broadcast(&queue->msgPutCall);
    pthread_mutex_unlock(&queue->mutex);
}

int putI(TQueue *queue, void *msg) {  
    pthread_mutex_lock(&queue->mutex);
    if (queue->exitFlag) {
        int exitMode = queue->exitMode;
        pthread_mutex_unlock(&queue->mutex);

        if (exitMode == 1)
            pthread_cond_broadcast(&queue->msgPutCall);
        else if (exitMode == 2)
            pthread_cond_broadcast(&queue->msgGetCall);

        return -1;
    }

    queue->activePublishers += 1;

    // Check if there is needed space in queue
    while (queue->msgNumber == queue->msgMax) {
        printf("[P] - Queue full | Waiting for free space\n");
        pthread_cond_wait(&queue->msgPutCall, &queue->mutex);

        if (queue->exitFlag) {
            queue->activePublishers -= 1;
            pthread_mutex_unlock(&queue->mutex);
            pthread_cond_broadcast(&queue->msgPutCall);
            return -1;
        }
    }

    // Create new Message
    Message *newMessage = malloc(sizeof(Message));
    if (newMessage == NULL) {
        printf("[P] - Failed to allocate memory for new message | Message not added \n");
        queue->activePublishers -= 1;
        pthread_mutex_unlock(&queue->mutex);
        pthread_cond_broadcast(&queue->msgGetCall);
        return 0;
    }

    if (queue->subscribersNumber == 0) {
        printf("[P] - Zero subscribers | Removing message\n");
        free(newMessage);
        queue->activePublishers -= 1;
        pthread_mutex_unlock(&queue->mutex);
        pthread_cond_broadcast(&queue->msgGetCall);
        return 0;
    }

    // Preparing new Message 'object'
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

    printf("[P] - Added new message\n");
    queue->activePublishers -= 1;
    pthread_mutex_unlock(&queue->mutex);
    pthread_cond_broadcast(&queue->msgGetCall);
}

void *getI(TQueue *queue, pthread_t thread) {
    pthread_mutex_lock(&queue->mutex);
    if (queue->exitFlag) {
        int exitMode = queue->exitMode;
        pthread_mutex_unlock(&queue->mutex);
        if (exitMode == 1)
            pthread_cond_broadcast(&queue->msgPutCall);
        else if (exitMode == 2)
            pthread_cond_broadcast(&queue->msgGetCall);
        
        return NULL;
    }

    queue->activeSubscribers += 1;

    // TODO: Change it for something faster like hashmap
    // Find threadId in subscribers list
    int threadSubId = -1; // -1 marks that thread is not in subscribers list
    for (int i = 0; i < MAX_SUBS; i++) {
        if (queue->subscribers[i].threadId == thread) {
            threadSubId = i;
            break;
        }
    }
    
    if (threadSubId == -1) {
        queue->activeSubscribers -= 1;
        pthread_mutex_unlock(&queue->mutex);
        printf("[S] - Thread is no longer subscribed | Returning NULL\n");
        return NULL;
    }

    // Get newest unread message pointer
    Message *nextThreadMsg = queue->subscribers[threadSubId].nextMsg;
    
    while (nextThreadMsg == NULL) {
        printf("[S] - All messages readed | Waiting for new one\n");
        pthread_cond_wait(&queue->msgGetCall, &queue->mutex);
        
        if (queue->exitFlag) {
            queue->activeSubscribers -= 1;
            pthread_mutex_unlock(&queue->mutex);
            pthread_cond_broadcast(&queue->msgGetCall);
            return NULL;
        }

        // Check if its still subscribed
        if (queue->subscribers[threadSubId].threadId != thread) {
            queue->activeSubscribers -= 1;
            pthread_mutex_unlock(&queue->mutex);
            printf("[S] - Thread is no longer subscribed | Returning NULL\n");
            return NULL;
        }
        nextThreadMsg = queue->subscribers[threadSubId].nextMsg;
    }

    // Get newest message by pointer
    void *msg;
    Message *tmp = queue->head;
    while (tmp != NULL) {
        if (tmp == nextThreadMsg) {
            // Save next message
             queue->subscribers[threadSubId].nextMsg = tmp->next;
             // Save message
             msg = tmp->msg;
             tmp->receivers -= 1;

             // If its last read of message - delete it
             if (tmp->receivers == 0) {
                printf("[S] - Message was read by the last subscriber | Deleting message\n");
                queue->msgNumber -= 1;

                if (queue->head == queue->tail) {
                    queue->tail = tmp->next;
                }
                queue->head = tmp->next;
                // tmp->msg = NULL;
                free(tmp);
             }
             queue->activeSubscribers -= 1;
             pthread_mutex_unlock(&queue->mutex);
             pthread_cond_broadcast(&queue->msgPutCall);
             return msg;
        }
        tmp = tmp->next;
    }

    // Secure exit - shouldn't be necessary
    queue->activeSubscribers -= 1;
    pthread_mutex_unlock(&queue->mutex);
    pthread_cond_broadcast(&queue->msgPutCall);
}

int getAvailableI(TQueue *queue, pthread_t thread) {
    if (queue == NULL) {
        return 0;
    }

    pthread_mutex_lock(&queue->mutex);
    Message *nextThreadMsg;

    // TODO: Change it for something faster like hashmap
    // Find thread next unread message in subscribers list
    bool isSubscriberFound = false;
    for (int i = 0; i < MAX_SUBS; i++) {
        if (queue->subscribers[i].threadId == thread) {
            nextThreadMsg = queue->subscribers[i].nextMsg;
            isSubscriberFound = true;
            break;
        }
    }

    if (!isSubscriberFound) {
        pthread_mutex_unlock(&queue->mutex);
        return 0;
    }

    // Count unread messages starting from next unread
    int unreadMessages = 0;
    while (nextThreadMsg != NULL) {
        unreadMessages++;
        nextThreadMsg = nextThreadMsg->next;
    }

    pthread_mutex_unlock(&queue->mutex);
    return unreadMessages;
}

void removeI(TQueue *queue, void *msg) {
    pthread_mutex_lock(&queue->mutex);

    // Look for message position in queue
    // Check if its (0) no messages, (1) only one message in queue, (2) head, (3) tail or (4) between them

    Message *messageToRemove;
    Message *nextMessage;
    // (0)
    if (queue->head == NULL || queue->tail == NULL) {
        pthread_mutex_unlock(&queue->mutex);
        printf("[U] - Message for remove not found\n");
        return;
    }

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
        Message *previous;
        while (tmp != NULL) {
            if (tmp->msg == msg) {
                break;
            }

            previous = tmp;
            tmp = tmp->next;
        }

        // If not found
        if (tmp == NULL) {
            pthread_mutex_unlock(&queue->mutex);
            printf("[R] - Message for remove not found\n");
            return;
        }

        messageToRemove = tmp;
        nextMessage = messageToRemove->next;
        previous->next = nextMessage;
    }

    // Update subscribers next message which points to removed message
    for (int i = 0; i < MAX_SUBS; i++) {
        if (queue->subscribers[i].nextMsg == messageToRemove)
            queue->subscribers[i].nextMsg = nextMessage;
    }

    // Decrement queue message counter
    queue->msgNumber -= 1;

    // Delete message
    free(messageToRemove);
    pthread_mutex_unlock(&queue->mutex);
    printf("[R] - Removed message\n");
}

void setSizeI(TQueue *queue, int size) {
    if (size < 1) return;
    pthread_mutex_lock(&queue->mutex);

    int sizeDifference = 0;
    if (size > queue->msgMax || size >= queue->msgNumber) {
        queue->msgMax = size;
    } else {
        sizeDifference = queue->msgMax - size;

        // Remove oldest messages
        Message *tmp = queue->head;
        for (int i = 0; i < sizeDifference; i++) {
            // Check which subscriber is pointing on it
            for (int j = 0 ; j < MAX_SUBS; j++) {
                if (queue->subscribers[j].nextMsg == tmp) {
                    queue->subscribers[j].nextMsg = tmp->next;
                }
            }

            queue->msgNumber -=1;
            queue->head = tmp->next;
            free(tmp);
            tmp = queue->head;
        }
        queue->msgMax = size;
    }

    printf("[U] - Changed size of queue\n");
    pthread_mutex_unlock(&queue->mutex);
}


// -= Worker functions =-

void *subscriber(void *q) {
    TQueue *queue = (TQueue*)q;
    pthread_t thread = ((pthread_t)pthread_self());
    subscribeI(queue, thread);
    int available;
    int *newMsg;

    while (true) {
        // available = getAvailableI(queue, thread);
        // printf("[S] - Available messages for thread: %d\n", available);

        newMsg = getI(queue, thread);
        if (newMsg == NULL) {
            printf("[S] - Thread is not subscribed\n");
            break;
        } else {
            printf("[S] - New message received: %d\n", *newMsg);
        }
        // sleep(1);
    }
    printf("[S] - End of work\n");
}

void *publisher(void *q) {
    TQueue *queue = (TQueue*)q;
    // pthread_t thread = ((pthread_t)pthread_self());
    int i = 0;
    while (true) {
        if (i==9) i=0;
        else i++;

        if (putI(queue, &i) == -1) {
            break;
        }
        // sleep(1);
    }
    printf("[P] - End of work\n");
}

void *remover(void *q) {
    TQueue *queue = (TQueue*)q;
    // pthread_t thread = ((pthread_t)pthread_self());
    int i = 0;
    while (true) {
        if (i==9) i=0;
        else i++;

        putI(queue, &i);
        // sleep(1);

        if (i == 4 || i == 7)
            removeI(queue, &i);
    }
}

int main() {
    pthread_t pub1, pub2, pub3;
    pthread_t sub1, sub2, sub3, sub4, sub5, sub6;
    pthread_t rem1;

    TQueue *queue = malloc(sizeof(TQueue));
    createQueueI(queue, 5);

    pthread_create(&pub1, NULL, publisher, queue);
    pthread_create(&pub2, NULL, publisher, queue);
    pthread_create(&pub3, NULL, publisher, queue);
    pthread_create(&sub1, NULL, subscriber, queue);
    pthread_create(&sub2, NULL, subscriber, queue);
    pthread_create(&sub3, NULL, subscriber, queue);
    pthread_create(&sub4, NULL, subscriber, queue);
    pthread_create(&sub5, NULL, subscriber, queue);
    pthread_create(&sub6, NULL, subscriber, queue);
    // pthread_create(&rem1, NULL, remover, queue);
    

    // # Testing unsubscribing
    // sleep(2);
    // unsubscribeI(queue, sub1);
    // sleep(2);
    // unsubscribeI(queue, sub2);
    // sleep(2);
    // unsubscribeI(queue, sub3);

    // sleep(5);
    // pthread_create(&sub4, NULL, subscriber, queue);

    // sleep(30);
    // setSizeI(queue, 5);
    
    sleep(10);
    destroyQueueI(queue);

    pthread_join(pub1, NULL);
    pthread_join(pub2, NULL);
    pthread_join(pub3, NULL);
    pthread_join(sub1, NULL);
    pthread_join(sub2, NULL);
    pthread_join(sub3, NULL);
    pthread_join(sub4, NULL);
    pthread_join(sub5, NULL);
    pthread_join(sub6, NULL);
    // pthread_join(rem1, NULL);

    return 0;
}
