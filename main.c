#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "structures.h"


// -= Interfaces =-
void createQueueI(TQueue *queue, int size);
void destroyQueueI(TQueue *queue);
bool subscribeI(TQueue *queue, pthread_t thread);
void unsubscribeI(TQueue *queue, pthread_t thread);
int putI(TQueue *queue, void *msg);
void *getI(TQueue *queue, pthread_t thread);
int getAvailableI(TQueue *queue, pthread_t thread);
void removeI(TQueue *queue, void *msg);
void setSizeI(TQueue *queue, int size);


// -= Worker functions =-
void *subscriber(void *q) {
    TQueue *queue = (TQueue*)q;
    pthread_t thread = ((pthread_t)pthread_self());
    subscribeI(queue, thread);
    int available;
    int *newMsg;

    while (true) {
        available = getAvailableI(queue, thread);
        printf("[S] - Available messages for thread: %d\n", available);

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
