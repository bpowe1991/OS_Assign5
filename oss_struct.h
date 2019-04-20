/*
Programmer: Briton A. Powe          Program Homework Assignment #4
Date: 10/25/18                      Class: Operating Systems
File: oss_struct.h
------------------------------------------------------------------------
Program Description:
Header file to declare process block, oss shared memory clock, and queue
structures. The function declarations are also stated here.
*/

#ifndef OSS_CLOCK
#define OSS_CLOCK
#include <sys/types.h>
#include <stdio.h>

//Structure of message for message queue.
typedef struct processMessage {
    long mesg_type; 
    char mesg_text[100];
    int choice_index;
    int choice; 
} message;

//Stucture of process control block.
typedef struct processResources {
    pid_t pid;
    long waitTimeNSec;
    int allocResources[20];
    int maxRequest[20];
    int blocked;
} processResources;

//Structure to be used in shared memory.
typedef struct Oss_clock {
    int sec;
    int nanoSec;
    int numChildren;
    int numRequests;
    int numGranted;
    int numDenied;
    int systemResources[20];
    int availableResources[20];
    long totalWait;
} oss_clock;

//Queue structure.
typedef struct queue 
{ 
    int front, rear, size; 
    unsigned capacity; 
    processResources* array; 
} queue; 

//Queue function declarations.
queue* createQueue(unsigned capacity);
int isFull(struct queue* queue);
int isEmpty(struct queue* queue);
void enqueue(struct queue* queue, struct processResources item);
processResources* dequeue(struct queue* queue);
processResources* front(struct queue* queue);

#endif