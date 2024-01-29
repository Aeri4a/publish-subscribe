# Publish-subscribe message system
Project for Concurrent and System Programming

Low-level based

## Files
- [**main.c**] - source file
- [**structures.h**] - struct models
- [**pubSubInterface.c**] - implementation of used functions
- [**main_sync.c**] - temporary, not important
- [**main_sync2.c**] - temporary, not important

## Compile & run
**Support only on Linux system, pthread library necessary**

Compile:
```bash
gcc -Wall -pthread -g main.c pubSubInterface.c -o outputFileName
```

Run:
```bash
./outputFileName
```

## Description of used structures

### Message
Object, which represents single added message.
It acts like node in linked list.
| Type | Name | Purpose |
| ------- | ------- | ------- |
| `int` | reveivers | number of subscribers who have not read the message |
| `void*` | msg | pointer to message |
| `Message*` | next | pointer to next Message object |
### Subscriber
Representation of subscribead thread.
| Type | Name | Purpose |
| ------- | ------- | ------- |
| `pthread_t` | threadId | id of subscribed thread |
| `Message*` | nextMsg | pointer to thread next unread message |
### TQueue
Queue structure is based on FIFO linked list.
| Type | Name | Purpose |
| ------- | ------- | ------- |
| `pthread_mutex_t` | mutex | main lock for queue |
| `pthread_cond_t` | msgGetCall | condition variable on which get waits |
| `pthread_cond_t` | msgPutCall | condition variable on which put waits |
| `int` | msgMax | max number of messages in queue |
| `int` | msgNumber | actual number of messages in queue |
| `bool` | exitFlag | if true, indicates that destroyQueue was called |
| `int`| exitMode | denotes the removal phase of waiting threads, if 0 then is unused, if 1 then publishers are being cancelled, if 2 then subscribers are being cancelled |
| `int` | activePublishers | number of active publishers |
| `int` | activeSubscribers | number of active subscribers |
| `int` | subscribersNumber | number of total subscribers in queue |
| `Subscriber` | subscribers | array of subscribers in queue |
| `Message*` | tail | tail of the queue |
| `Message*` | head | head of the queue |

## Other informations
### Subscribers array
Size of array is predefined with fixed value, which is in `structures.c` with name `MAX_SUBS`.

Default value is `50`.

### Supportive functions
In order to test the queue there are three supportive functions:
- [**publisher**] - which defines thread in role of publisher
- [**subscriber**] - which defines thread in role of subscriber
- [**remover**] - single thread call to put message and remove it

### Message prefixes
| Prefix | Description |
| --------- | --------- |
| [P] | Publisher |
| [S] | Subscriber |
| [U] | Utility |
| [R] | Remove operation (in remove interface) |
| [D] | Destroy queue operations |
| [Q] | Queue initialization |

### Included tests
File main.c includes two test cases.
In order to run them they need to be uncommented.

**Tests are not independent, there should be just one uncommented at the same time**

### Other informations
Based on task description, assumed that only get and put methods can react on destroyQueue.