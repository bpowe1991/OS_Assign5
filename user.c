/*
Programmer: Briton A. Powe          Program Homework Assignment #5
Date: 11/14/18                      Class: Operating Systems
File: user.c
------------------------------------------------------------------------
Program Description:
This is the file for child processes for oss. User process run in a loop,
generating a choice to request, release or terminate. It reads from a message
queue to determine if it is allowed to allocate, release, or end.
If request is blocked, a user will not change a choice until it is granted.
Once it recieves a value for terminating, it will exit the loop and end.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/ipc.h> 
#include <sys/shm.h>
#include <sys/msg.h>  
#include <signal.h>
#include "oss_struct.h"
#include <sys/stat.h>

struct Oss_clock *clockptr;

void sigQuitHandler(int);

int main(int argc, char *argv[]){ 
    signal(SIGQUIT, sigQuitHandler);
    int shmid, choice, sendmsgid, rcmsgid, blocked = 0, running = 1;
    key_t shmkey = 3670401;
    key_t recievekey = 3670402;
    key_t sendkey = 3670403;
    message recieveMsg;
    message sendMsg;
    int maxClaims[20];
    int allocate[20];

    srand(time(0));
    
    //Getting max claims from agrv
    int index;
    for (index = 0; index < 20; index++){
        maxClaims[index] = argv[1][index]-'0';
    }

    //Finding shared memory segment.
    if ((shmid = shmget(shmkey, sizeof(struct Oss_clock), 0666|IPC_CREAT)) < 0) {
        perror(strcat(argv[0],": Error: Failed shmget find"));
        exit(-1);
    }

    //Attaching to memory segment.
    if ((clockptr = shmat(shmid, NULL, 0)) == (void *) -1) {
        perror(strcat(argv[0],": Error: Failed shmat attach"));
        exit(-1);
    }

    //Setting up send message queue.
    if ((sendmsgid = msgget(sendkey, 0666 | IPC_CREAT)) < 0) {
        perror(strcat(argv[0],": Error: Failed message queue creation/attach"));
        exit(1);
    }
    
    //Setting up recieve message queue.
    if ((rcmsgid = msgget(recievekey, 0666 | IPC_CREAT)) < 0) {
        perror(strcat(argv[0],": Error: Failed message queue creation/attach"));
        exit(1);
    }

    //Main loop.
    while (running == 1){
        
        //Adjust clock if oss hasn't yet.
        if (clockptr->nanoSec > ((int)1e9)) {
                clockptr->sec += (clockptr->nanoSec / ((int)1e9));
                clockptr->nanoSec = (clockptr->nanoSec % ((int)1e9));
        }

        //If the processes isn't blocked by oss, generate new request.
        if (blocked != 1){
            choice = (rand() % (100)) + 1;
            if (choice >= 1 && choice <= 60){
                sendMsg.choice_index = -1;
                sendMsg.choice = 1;
                
                //Checking if any allocated resources are less than max.
                int index;
                for(index = 0; index < 20; index++){
                    if(allocate[index] < maxClaims[index]){
                        sendMsg.choice_index = index;
                        break;
                    }
                }

                //If none of the resources are less than max.
                if(sendMsg.choice_index == -1){
                    sendMsg.choice = 2;
                    sendMsg.choice_index = (rand() % (20));
                    
                    //Writing message for request.
                    sprintf(sendMsg.mesg_text, "%ld : Releasing R%d at %d.%d", (long) getpid(), 
                            sendMsg.choice_index, clockptr->sec, clockptr->nanoSec);
                }

                //If at least one resource was less than max.
                if(sendMsg.choice_index != -1){
                    sendMsg.choice_index = -1;
                    while(sendMsg.choice_index == -1){
                        sendMsg.choice_index = (rand() % (20));

                        //If the resource at the index is at max, reset choice index.
                        if(allocate[sendMsg.choice_index] == maxClaims[sendMsg.choice_index]){
                            sendMsg.choice_index = -1;
                        }
                    }

                    //Writing message for request.
                    sprintf(sendMsg.mesg_text, "%ld : Requesting R%d at %d.%d", (long) getpid(), 
                            sendMsg.choice_index, clockptr->sec, clockptr->nanoSec);
                }
            }
            else if (choice > 60 && choice <= 90){
                sendMsg.choice_index = -1;
                sendMsg.choice = 2;

                //Checking if any allocated resources are more than 0.
                int index;
                for(index = 0; index < 20; index++){
                    if(allocate[index] > 0){
                        sendMsg.choice_index = index;
                        break;
                    }
                }

                //If none of the resources are more than 0.
                if(sendMsg.choice_index == -1){
                    sendMsg.choice = 1;
                    sendMsg.choice_index = (rand() % (20));
                    
                    //Writing message for request.
                    sprintf(sendMsg.mesg_text, "%ld : Requesting R%d at %d.%d", (long) getpid(), 
                            sendMsg.choice_index, clockptr->sec, clockptr->nanoSec);
                }

                //If at least one resource was more than 0.
                if(sendMsg.choice_index != -1){
                    sendMsg.choice_index = -1;
                    while(sendMsg.choice_index == -1){
                        sendMsg.choice_index = (rand() % (20));

                        //If the resource at the index is at 0, reset choice index.
                        if(allocate[sendMsg.choice_index] == 0){
                            sendMsg.choice_index = -1;
                        }
                    }

                    //Writing message for request.
                    sprintf(sendMsg.mesg_text, "%ld : Releasing R%d at %d.%d", (long) getpid(), 
                            sendMsg.choice_index, clockptr->sec, clockptr->nanoSec);
                }
            }
            else if (choice > 90 && choice <= 100){
                sendMsg.choice = 3;
                sprintf(sendMsg.mesg_text, "%ld : Requesting to terminate at %d.%d", (long) getpid(), 
                        clockptr->sec, clockptr->nanoSec);
            }
        }
        
        //Set message type to pid to differentiate.
        sendMsg.mesg_type = (long) getpid();
        msgsnd(rcmsgid, &sendMsg, sizeof(sendMsg), IPC_NOWAIT);

        //Keep searching for message with type equal to pid.
        msgrcv(sendmsgid, &recieveMsg, sizeof(recieveMsg), (long) getpid(), 0);
        
        //Handling response from oss.
        if (strcmp(recieveMsg.mesg_text, "Granted") == 0){
            if (choice == 1){
                allocate[sendMsg.choice_index]++;
                blocked = 0;
            }
            else if (choice == 2){
                allocate[sendMsg.choice_index]--;
                blocked = 0;
            }
            else if (choice == 3){
                running = 0;
            }
        }
        else{
            blocked = 1;
        }
    }

    //Detaching from memory segment.
    if (shmdt(clockptr) == -1) {
      perror(strcat(argv[0],": Error: Failed shmdt detach"));
      clockptr = NULL;
      exit(-1);
   }

    return 0;
}

//Handler for quit signal.
void sigQuitHandler(int sig) {
   abort();
}