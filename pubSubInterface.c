#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "structures.h"


// -= Interfaces =-
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

    printf("[Q] - Queue initialized\n");
}

void destroyQueueI(TQueue *queue) {
    pthread_mutex_lock(&queue->mutex);
    printf("[D] - Preparation for destroying queue\n");

    queue->exitFlag = true;
    // Wait for all "waiting on condition" threads
    // Using repetitive calls while normal putI & getI methods pass
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

    // Clear rest of the TQueue structure
    pthread_mutex_t *copy = &queue->mutex;
    free(queue);
    pthread_mutex_unlock(copy);
    pthread_mutex_destroy(copy);

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
    printf("[S] - Thread failed while subscribing to queue\n");
    return false;
}

void unsubscribeI(TQueue *queue, pthread_t thread) {
    pthread_mutex_lock(&queue->mutex);
    Message *threadNextMsg;

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
                printf("[U] - Found empty message | Deleting message\n");
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

// Normally returning 0, if error occurs returning -1 (error includes destroying queue)
int putI(TQueue *queue, void *msg) { 
    pthread_mutex_lock(&queue->mutex);
    // Notice queue destroy procedure and its status (mode)
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
        printf("[U] - Zero subscribers | Removing message\n");
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

// Returns pointer to message, if error occurs returning NULL (error includes destroying queue)
void *getI(TQueue *queue, pthread_t thread) {
    pthread_mutex_lock(&queue->mutex);
    // Notice queue destroy procedure and its status (mode)
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
                printf("[U] - Message was read by the last subscriber | Deleting message\n");
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
    pthread_mutex_lock(&queue->mutex);

    Message *nextThreadMsg;
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
        printf("[R] - Message for remove not found\n");
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
