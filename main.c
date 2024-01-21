#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
// #include <pthread.h>

#define QUEUE_EMPTY INT_MIN

typedef struct Message {
    int value;
    struct Message *next;
} Message;

typedef struct {
    Message *tail;
    Message *head;
} TQueue;

void initQueue(TQueue *q) {
    q->head = NULL;
    q->tail = NULL;
}

bool enqueue(TQueue *q, int num) {
    Message *newNode = malloc(sizeof(Message));
    if (newNode == NULL) return false;
    newNode->value = num;
    newNode->next = NULL;

    if (q->tail != NULL) {
        q->tail->next = newNode;
    }

    q->tail = newNode;
    if (q->head == NULL) {
        q->head = newNode;
    }

    return true;
}

int dequeue(TQueue *q) {
    if (q->head == NULL) return QUEUE_EMPTY;
    Message *tmpHead = q->head;    
    int result = tmpHead->value;

    q->head = q->head->next;
    if (q->head == NULL) {
        q->tail = NULL;
    }
    
    free(tmpHead);
    return result;
}

int main() {



    return 0;
}