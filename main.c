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
    if (!subscribeI(queue, thread)) return NULL;

    int available;
    int *newMsg;
    while (true) {
        available = getAvailableI(queue, thread);
        printf("[S] - Available messages for thread: %d\n", available);

        newMsg = getI(queue, thread);
        if (newMsg == NULL) {
            printf("[S] - Thread is not subscribed or queue has been destroyed\n");
            break;
        } else {
            printf("[S] - New message received: %d\n", *newMsg);
        }

        sleep(1);
    }
    printf("[S] - End of work\n");
    return NULL;
}

void *publisher(void *q) {
    TQueue *queue = (TQueue*)q;

    int i = 0;
    while (true) {
        if (i==9) i=0;
        else i++;

        if (putI(queue, &i) == -1)
            break;

        sleep(1);
    }
    printf("[P] - End of work\n");
    return NULL;
}

void *remover(void *q) {
    TQueue *queue = (TQueue*)q;
    int msg = 123;

    putI(queue, &msg);
    sleep(2);
    removeI(queue, &msg);
    return NULL;
}

int main() {
    // TEST SECTION
    // PLEASE UNCOMMENT CHOOSEN TEST

    // # CASE 1 --------------------------------------------------
    // 1 publisher, 4 subscribers
    // Starting with one subscriber
    // After 5 seconds, one subscriber (number 1) will be unsubscribed
    // After next 7 seconds, four subscribers will be added
    // After next 8 seconds, there will be called remover
    // Starting size 4, after next 10 sec increasing size to 6
    // Destroying queue after 60 seconds
    // UNCOMMENT >>
    printf("STARTING TEST CASE 1\n\n");
    pthread_t pub1;
    pthread_t rem1;
    pthread_t sub1, sub2, sub3, sub4, sub5;

    TQueue *queue = malloc(sizeof(TQueue));
    createQueueI(queue, 4);

    pthread_create(&pub1, NULL, publisher, queue);
    pthread_create(&sub1, NULL, subscriber, queue);
    
    sleep(5);
    unsubscribeI(queue, sub1);

    sleep(7);
    pthread_create(&sub2, NULL, subscriber, queue);
    pthread_create(&sub3, NULL, subscriber, queue);
    pthread_create(&sub4, NULL, subscriber, queue);
    pthread_create(&sub5, NULL, subscriber, queue);

    sleep(8);
    pthread_create(&rem1, NULL, remover, queue);

    sleep(10);
    setSizeI(queue, 6);
    
    sleep(60);
    destroyQueueI(queue);

    pthread_join(pub1, NULL);
    pthread_join(rem1, NULL);
    pthread_join(sub1, NULL);
    pthread_join(sub2, NULL);
    pthread_join(sub3, NULL);
    pthread_join(sub4, NULL);
    pthread_join(sub5, NULL);
    // UNCOMMENT >>
    // # -------------------------------------------------------------



    // # Case 2 --------------------------------------------------
    // 3 publishers, 6 subscribers
    // After 3 seconds, one subscriber (number 1) will be unsubscribed
    // Starting size 10, after next 5 sec reducing to size 5
    // Destroying queue after next 60 seconds
    // UNCOMMENT >>
        // printf("STARTING TEST CASE 2\n\n");
        // pthread_t pub1, pub2, pub3;
        // pthread_t sub1, sub2, sub3, sub4, sub5, sub6;

        // TQueue *queue = malloc(sizeof(TQueue));
        // createQueueI(queue, 10);

        // pthread_create(&pub1, NULL, publisher, queue);
        // pthread_create(&pub2, NULL, publisher, queue);
        // pthread_create(&pub3, NULL, publisher, queue);
        // pthread_create(&sub1, NULL, subscriber, queue);
        // pthread_create(&sub2, NULL, subscriber, queue);
        // pthread_create(&sub3, NULL, subscriber, queue);
        // pthread_create(&sub4, NULL, subscriber, queue);
        // pthread_create(&sub5, NULL, subscriber, queue);
        // pthread_create(&sub6, NULL, subscriber, queue);
        
        // sleep(3);
        // unsubscribeI(queue, sub1);

        // sleep(5);
        // setSizeI(queue, 5);
        
        // sleep(60);
        // destroyQueueI(queue);

        // pthread_join(pub1, NULL);
        // pthread_join(pub2, NULL);
        // pthread_join(pub3, NULL);
        // pthread_join(sub1, NULL);
        // pthread_join(sub2, NULL);
        // pthread_join(sub3, NULL);
        // pthread_join(sub4, NULL);
        // pthread_join(sub5, NULL);
        // pthread_join(sub6, NULL);
    // UNCOMMENT >>
    // # -------------------------------------------------------------

    return 0;
}
