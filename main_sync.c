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
    int threadId;
    Message *nextMsg;
} Subscriber;

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;

    int msgMax;
    int msgNumber;
    int subscribersNumber;
    Subscriber subscribers[MAX_SUBS];

    Message *tail;
    Message *head;
} TQueue;

void createQueueI(TQueue *queue, int size) {
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->cond, NULL);

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

    pthread_mutex_destroy(&queue->mutex);

    // Clear rest of the TQueue structure
    free(queue);
}

// TODO: Change thread type of pthread_t
bool subscribeI(TQueue *queue, int thread) {
    // pthread_mutex_lock(&queue->mutex);
    queue->subscribersNumber += 1;
    for (int i = 0; i < MAX_SUBS; i++) {
        if (queue->subscribers[i].threadId == -1) {
            queue->subscribers[i].threadId = thread;
            // pthread_mutex_unlock(&queue->mutex);
            // pthread_cond_signal(&queue->cond);
            return true;
        }
    }
    // pthread_mutex_unlock(&queue->mutex);
    // pthread_cond_signal(&queue->cond);
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
    pthread_mutex_lock(&queue->mutex);

    // Check if there is needed space in queue
    while (queue->msgNumber == queue->msgMax) {
        // Search for messages which can be deleted
        int deletedCount = 0;
        Message *tmp = queue->head;
        while (tmp->receivers == 0) { //tmp != NULL && 
            printf("Message to delete: %s\n", (char*)tmp->msg);
            if (queue->head == queue->tail) {
                queue->tail = NULL;
            }
            queue->head = tmp->next;
            free(tmp);
            deletedCount += 1;
            tmp = queue->head;
        }

        // No free space for new msg, waiting there -> check if thread read sth
        if (deletedCount == 0) {
            printf("Waiting for read\n");
            pthread_cond_wait(&queue->cond, &queue->mutex);
            printf("Stopped waiting for read\n");
        }
        // There is some space
        else queue->msgNumber -= deletedCount;
    }

    // Create new Message
    if (queue->subscribersNumber == 0) {
        pthread_mutex_unlock(&queue->mutex);
        pthread_cond_broadcast(&queue->cond);
        return;
    }
    Message *newMessage = malloc(sizeof(Message));
    if (newMessage == NULL) {
        pthread_mutex_unlock(&queue->mutex);
        pthread_cond_broadcast(&queue->cond);
        return;
    }

    printf("Sub num: %d\n", queue->subscribersNumber);
    newMessage->next = NULL;
    newMessage->receivers = queue->subscribersNumber;
    newMessage->msg = msg;

    // Update next message for subscribed threads with any unread messages
    for (int i = 0; i < MAX_SUBS; i++) {
        if (
            queue->subscribers[i].threadId != -1 &&
            queue->subscribers[i].nextMsg == NULL
        )
            queue->subscribers[i].nextMsg = newMessage;
    }

    // Enqueue new Message
    queue->msgNumber++;
    if (queue->tail != NULL) {
        queue->tail->next = newMessage;
    }

    queue->tail = newMessage;
    if (queue->head == NULL) {
        queue->head = newMessage;
    }
    printf("\n\nAdded message\n\n");
    pthread_mutex_unlock(&queue->mutex);
    pthread_cond_broadcast(&queue->cond);
}

// TODO: Change thread type of pthread_t
void *getI(TQueue *queue, int thread) {
    pthread_mutex_lock(&queue->mutex);
    int threadSubId;

    // TODO: Change it for something faster like hashmap
    // Find threadId in subscribers list
    for (int i = 0; i < MAX_SUBS; i++) {
        if (queue->subscribers[i].threadId == thread)
            threadSubId = i;
    }

    // Get newest message pointer
    Message *nextThreadMsg = queue->subscribers[threadSubId].nextMsg;
    while (nextThreadMsg == NULL) {
        printf("Waiting for new message\n");
        pthread_cond_wait(&queue->cond, &queue->mutex);
        nextThreadMsg = queue->subscribers[threadSubId].nextMsg;
        printf("No waiting for new message\n");
    }

    // Get newest message by pointer
    Message *tmp = queue->head;
    while (tmp != NULL) {
        if (tmp == nextThreadMsg) {
             tmp->receivers -= 1;
             queue->subscribers[threadSubId].nextMsg = tmp->next; // save next message
             pthread_mutex_unlock(&queue->mutex);
             pthread_cond_broadcast(&queue->cond);
             return tmp->msg;
        }
        tmp = tmp->next;
    }

    pthread_mutex_unlock(&queue->mutex);
    pthread_cond_broadcast(&queue->cond);
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
//
//     if (q->tail != NULL) {
//         q->tail->next = newNode;
//     }
//
//     q->tail = newNode;
//     if (q->head == NULL) {
//         q->head = newNode;
//     }
//
//     return true;
// }
//
// int dequeue(TQueue *q) {
//     if (q->head == NULL) return QUEUE_EMPTY;
//     Message *tmpHead = q->head;    
//     int result = tmpHead->value;
//
//     q->head = q->head->next;
//     if (q->head == NULL) {
//         q->tail = NULL;
//     }
//
//     free(tmpHead);
//     return result;
// }

void *subscriber(void *q) {
    TQueue *queue = (TQueue*)q;
    int thread = ((int)pthread_self());
    // subscribeI(q, thread);
    while (true) {
        printf("[S] - Thread: %d, got: %s\n", thread, (char*)getI(q, thread));
        // sleep(rand() % 5);
    }
}

void *publisher(void *q) {
    TQueue *queue = (TQueue*)q;
    int thread = ((int)pthread_self());
    char *yes[10] = { "Msg1", "Msg2", "Msg3", "Msg4", "Msg5", "Msg6", "Msg7", "Msg8", "Msg9", "Msg10" };
    int i = 0;
    while (true) {
        putI(q, yes[i]);
        // printf("[P] - Thread: %d, added message\n", thread);
        if (i>9) i=0;
        else i++;
        // sleep(4);
    }
}

int main() {
    pthread_t pub1, pub2, pub3;
    pthread_t sub1, sub2, sub3, sub4, sub5, sub6;

    TQueue *queue = malloc(sizeof(TQueue));
    createQueueI(queue, 5);
    subscribeI(queue, (int)sub1);
    subscribeI(queue, (int)sub2);
    subscribeI(queue, (int)sub3);
    subscribeI(queue, (int)sub4);
    subscribeI(queue, (int)sub5);
    subscribeI(queue, (int)sub6);

    pthread_create(&pub1, NULL, publisher, queue);
    pthread_create(&pub2, NULL, publisher, queue);
    pthread_create(&pub3, NULL, publisher, queue);
    pthread_create(&sub1, NULL, subscriber, queue);
    pthread_create(&sub2, NULL, subscriber, queue);
    pthread_create(&sub3, NULL, subscriber, queue);
    pthread_create(&sub4, NULL, subscriber, queue);
    pthread_create(&sub5, NULL, subscriber, queue);
    pthread_create(&sub6, NULL, subscriber, queue);

    pthread_join(pub1, NULL);
    pthread_join(sub4, NULL);
    pthread_join(sub1, NULL);
    pthread_join(sub2, NULL);
    pthread_join(sub3, NULL);
    pthread_join(pub3, NULL);
    pthread_join(sub5, NULL);
    pthread_join(pub2, NULL);
    pthread_join(sub6, NULL);

    destroyQueueI(queue);

    // Temporary testing section
    // int threadId1 = 5;
    // int threadId2 = 6;
    // TQueue *q = malloc(sizeof(TQueue));
    // createQueueI(q, 3);

    // subscribeI(q, threadId1);
    // char msg1[] = "Message 1";
    // putI(q, msg1);
    // subscribeI(q, threadId2);
    // char msg2[] = "Message 2";
    // putI(q, msg2);
    // char msg3[] = "Message 3";
    // putI(q, msg3);

    // getI(q, threadId1);
    // removeI(q, msg2);
    // removeI(q, msg3);
    // printf("Next msg for thread1: %s", (char*)getI(q, threadId1));

    // destroyQueueI(q);



    return 0;
}
