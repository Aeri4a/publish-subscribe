#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define QUEUE_EMPTY INT_MIN
#define MAX_SUBS 50

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
    pthread_mutex_t mutex;
    pthread_cond_t msgNew;
    pthread_cond_t msgRemove;

    int msgMax;
    int msgNumber;
    int subscribersNumber;
    Subscriber subscribers[MAX_SUBS];

    Message *tail;
    Message *head;
} TQueue;

void createQueueI(TQueue *queue, int size) {
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->msgNew, NULL);
    pthread_cond_init(&queue->msgRemove, NULL);

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
    pthread_mutex_lock(&queue->mutex);
    Message *tmp = queue->head;

    // Clear all Messages structures
    while (tmp != NULL) {
        queue->head = tmp->next;
        free(tmp);
        tmp = queue->head;        
    }

    pthread_mutex_unlock(&queue->mutex);
    pthread_mutex_destroy(&queue->mutex);

    // Clear rest of the TQueue structure
    free(queue);
}

bool subscribeI(TQueue *queue, pthread_t thread) {
    pthread_mutex_lock(&queue->mutex);
    for (int i = 0; i < MAX_SUBS; i++) {
        if (queue->subscribers[i].threadId == -1) {
            queue->subscribers[i].threadId = thread;
            queue->subscribersNumber += 1;
            pthread_mutex_unlock(&queue->mutex);
            // pthread_cond_broadcast(&queue->msgRemoved);
            // pthread_cond_broadcast(&queue->cond);
            return true;
        }
    }
    pthread_mutex_unlock(&queue->mutex);
    // pthread_cond_broadcast(&queue->msgRemoved);
    // pthread_cond_broadcast(&queue->cond);
    return false;
}

void unsubscribeI(TQueue *queue, pthread_t thread) {
    pthread_mutex_lock(&queue->mutex);
    Message *threadNextMsg;

    // TODO: Change it for something faster like hashmap
    for (int i = 0; i< MAX_SUBS; i++) {
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
                if (isFirstMsgFound)
                    tmp->receivers -= 1;
            }
            tmp = tmp->next;
        }
    }

    pthread_mutex_unlock(&queue->mutex);
}

void putI(TQueue *queue, void *msg) {
    pthread_mutex_lock(&queue->mutex);

    // Check if there is needed space in queue
    while (queue->msgNumber == queue->msgMax) {
        printf("Waiting\n");
        pthread_cond_wait(&queue->msgRemove, &queue->mutex);
    }

    // Create new Message
    Message *newMessage = malloc(sizeof(Message));
    if (newMessage == NULL) {
        pthread_mutex_unlock(&queue->mutex);
        pthread_cond_broadcast(&queue->msgNew);
        return;
    }

    if (queue->subscribersNumber == 0) {
        printf("No subs\n");
        free(newMessage);
        pthread_mutex_unlock(&queue->mutex);
        pthread_cond_broadcast(&queue->msgNew);
        return;
    }

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
    printf("Inserted message\n");
    pthread_mutex_unlock(&queue->mutex);
    pthread_cond_broadcast(&queue->msgNew);
}

void *getI(TQueue *queue, pthread_t thread) {
    pthread_mutex_lock(&queue->mutex);

    // TODO: Change it for something faster like hashmap
    // Find threadId in subscribers list
    int threadSubId;
    for (int i = 0; i < MAX_SUBS; i++) {
        if (queue->subscribers[i].threadId == thread) {
            threadSubId = i;
            break;
        }
    }

    // Get newest message pointer
    Message *nextThreadMsg = queue->subscribers[threadSubId].nextMsg;
    
    while (nextThreadMsg == NULL) {
        pthread_cond_wait(&queue->msgNew, &queue->mutex);
        printf("Seems to be new msg\n");
        nextThreadMsg = queue->subscribers[threadSubId].nextMsg;
    }
    printf("Getting message...\n");
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
                printf("Deleted message\n");
                queue->msgNumber -= 1;
                if (queue->head == queue->tail) {
                    queue->tail = tmp->next;
                }
                queue->head = tmp->next;
                tmp->msg = NULL;
                free(tmp);
             }
             pthread_mutex_unlock(&queue->mutex);
             pthread_cond_broadcast(&queue->msgRemove);
             return msg;
        }
        tmp = tmp->next;
    }
    pthread_mutex_unlock(&queue->mutex);
    pthread_cond_broadcast(&queue->msgRemove);
}

int getAvailableI(TQueue *queue, pthread_t thread) {
    pthread_mutex_lock(&queue->mutex);
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

    pthread_mutex_unlock(&queue->mutex);
    // pthread_cond_broadcast(&queue->msgNew);
    // pthread_cond_broadcast(&queue->msgRemove);
    return unreadMessages;
}

void removeI(TQueue *queue, void *msg) {
    pthread_mutex_lock(&queue->mutex);
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
    pthread_mutex_unlock(&queue->mutex);
}

// TODO: Complete function
void setSizeI(TQueue *queue, int size);

void *subscriber(void *q) {
    TQueue *queue = (TQueue*)q;
    pthread_t thread = ((pthread_t)pthread_self());
    subscribeI(queue, thread);
    while (true) {
        printf("[S] - Thread: %ld, got: %s\n", thread, (char*)getI(queue, thread));
        printf("Available: %d\n", getAvailableI(queue, thread));
        // sleep(1);
    }
}

void *publisher(void *q) {
    TQueue *queue = (TQueue*)q;
    pthread_t thread = ((pthread_t)pthread_self());
    char *yes[10] = { "Msg1", "Msg2", "Msg3", "Msg4", "Msg5", "Msg6", "Msg7", "Msg8", "Msg9", "Msg10" };
    int i = 0;
    while (true) {
        if (i==9) i=0;
        else i++;

        putI(queue, yes[i]);
        // sleep(1);
    }   
}

int main() {
    pthread_t pub1, pub2, pub3;
    pthread_t sub1, sub2, sub3, sub4, sub5, sub6;

    TQueue *queue = malloc(sizeof(TQueue));
    createQueueI(queue, 5);

    pthread_create(&pub1, NULL, publisher, queue);
    // pthread_create(&pub2, NULL, publisher, queue);
    // pthread_create(&pub3, NULL, publisher, queue);
    pthread_create(&sub1, NULL, subscriber, queue);
    pthread_create(&sub2, NULL, subscriber, queue);
    pthread_create(&sub3, NULL, subscriber, queue);
    pthread_create(&sub4, NULL, subscriber, queue);
    pthread_create(&sub5, NULL, subscriber, queue);
    pthread_create(&sub6, NULL, subscriber, queue);
    
    // sleep(4);
    // destroyQueueI(queue);

    pthread_join(pub1, NULL);
    // pthread_join(pub2, NULL);
    // pthread_join(pub3, NULL);
    pthread_join(sub1, NULL);
    pthread_join(sub2, NULL);
    pthread_join(sub3, NULL);
    pthread_join(sub4, NULL);
    pthread_join(sub5, NULL);
    pthread_join(sub6, NULL);


    return 0;
}
