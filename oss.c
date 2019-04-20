/*
Programmer: Briton A. Powe          Program Homework Assignment #5
Date: 11/14/18                      Class: Operating Systems
File: oss.c
------------------------------------------------------------------------
Program Description:
Simulates oss resource allocation and deadlock avoidance. Randomly forks off 
children, reads resource request from a message queue, and calculates if system
is in a safe state. Creates a clock structure in shared memory to track time.
All events performed are recorded in a log file called log.txt by default or in 
a file designated by the -l option. To set the timer to termante the program, 
it uses a -t option.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <signal.h>
#include <sys/msg.h> 
#include "oss_struct.h"
#include <sys/stat.h>
#include <stdbool.h>

int flag = 0;
pid_t parent;
int processCount = 0, writtenTo, writeCounter; 
const int resourceCount = 20; 
bool verbose = false;

int is_pos_int(char test_string[]);

bool isSafe(int [], int [], int [][20], 
            int [][20], int );
void printAllocatMatrix(FILE *, queue *);
void alarmHandler();
void interruptHandler();

int main(int argc, char *argv[]){

    signal(SIGINT,interruptHandler);
    signal(SIGALRM, alarmHandler);
    signal(SIGQUIT, SIG_IGN);

    struct Oss_clock *clockptr;
    char filename[20] = "log.txt";
    int opt, t = 2, shmid, status = 0, sendmsgid, rcmsgid, clockIncrement,
        scheduleTimeSec = 0, scheduleTimeNSec = 0, bound = 5;
    double percentage = 0.0;
    long blockedTime;
    key_t shmkey = 3670401;
    key_t recievekey = 3670402;
    key_t sendkey = 3670403;
    message sendMsg;
    message recieveMsg;
	pid_t childpid = 0, wpid;
    FILE *logPtr;
    parent = getpid();
    srand(time(0));

    
    
    //Setting blocked queue and in system process list for oss.
    queue* processes = createQueue(18);

    //Parsing options.
    while((opt = getopt(argc, argv, "t:l:vhp")) != -1){
		switch(opt){
            
            //Option to enter t.
            case 't':
				if(is_pos_int(optarg) == 1){
					fprintf(stderr, "%s: Error: Entered illegal input for option -t\n",
							argv[0]);
					exit(-1);
				}
				else {
                    t = atoi(optarg);
                    if (t <= 0) {
                        fprintf(stderr, "%s: Error: Entered illegal input for option -t", argv[0]);
                        exit(-1);
                    }
				}
				break;
            
            //Option to enter l.
            case 'l':
				sprintf(filename, "%s", optarg);
                if (strcmp(filename, "") == 0){
                    fprintf(stderr, "%s: Error: Entered illegal input for option -l:"\
                                        " invalid filename\n", argv[0]);
                    exit(-1);
                }
				break;
            
            //Verbose option.
            case 'v':
                verbose = true;
				break;
            //Help option.
            case 'h':
                fprintf(stderr, "\nThis program simulates resource management with deadlock\n"\
                                "avoidance. The parent increments a clock in shared memory and\n"\
                                "schedules children to run based around the clock.\n"\
                                "Banker's algorithm is used for deadlock avoidance.\n"\
                                "OPTIONS:\n\n"\
                                "-t Set the number of seconds the program will run."\
                                "(i.e. -t 4 allows the program to run for 4 sec).\n"\
                                "-l set the name of the log file (default: log.txt).\n"\
                                "(i.e. -l logFile.txt sets the log file name to logFile.txt).\n"\
                                "-v to set verbose to true (will print allocation table).\n"\
                                "-h Bring up this help message.\n"\
                                "-p Bring up a test error message.\n\n");
                exit(0);
                break;
            
            //Option to print error message using perror.
            case 'p':
                perror(strcat(argv[0], ": Error: This is a test Error message"));
                exit(-1);
                break;
            case '?':
                fprintf(stderr, "%s: Error: Unrecognized option \'-%c\'\n", argv[0], optopt);
                exit(-1);
                break;
			default:
				fprintf(stderr, "%s: Error: Unrecognized option\n",
					    argv[0]);
				exit(-1);
		}
	}

    //Checking if m, s, and t have valid integer values.
    if (t <= 0){
        perror(strcat(argv[0], ": Error: Illegal parameter for -t option"));
        exit(-1);
    }
   
   //Creating or opening log file.
   if((logPtr = fopen(filename,"w+")) == NULL)
   {
      fprintf(stderr, "%s: Error: Failed to open/create log file\n",
					    argv[0]);
      exit(-1);             
   }

    alarm(t);
    
    //Creating shared memory segment.
    if ((shmid = shmget(shmkey, sizeof(struct Oss_clock), 0666|IPC_CREAT)) < 0) {
        perror(strcat(argv[0],": Error: Failed shmget allocation"));
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

    //Setting up system resources
    int count;
    for (count = 0; count < 20; count++){
        clockptr->systemResources[count] = (rand() % 10) +1;
        clockptr->availableResources[count] = clockptr->systemResources[count];
    }

    //Main Loop
    while (flag != 1){
        //Setting scheduling time.           
        if (scheduleTimeNSec == 0 && scheduleTimeSec == 0){
            scheduleTimeNSec = clockptr->nanoSec + ((rand() % (500000000 - 1000000 + 1)) + 1000000);
            if (scheduleTimeNSec > ((int)1e9)) {
                scheduleTimeSec += (scheduleTimeNSec / ((int)1e9));
                scheduleTimeNSec = (scheduleTimeNSec % ((int)1e9));
            }
        }

        //Incrementing clock.
        clockIncrement = (rand() % (15000000 - 5000000 + 1)) + 5000000;
        blockedTime += (long)clockIncrement;
        clockptr->nanoSec += clockIncrement;
        if (clockptr->nanoSec > ((int)1e9)) {
            clockptr->sec += (clockptr->nanoSec/((int)1e9));
            clockptr->nanoSec = (clockptr->nanoSec%((int)1e9));
        }
    
        
        //Checking if new process is ready to be created.
        if ((clockptr->sec > scheduleTimeSec) || 
            (clockptr->sec == scheduleTimeSec && clockptr->nanoSec >= scheduleTimeNSec)){
            scheduleTimeNSec = 0;
            scheduleTimeSec = 0;

            if (processCount < 18){
                //Create new process
                processResources newProcess = {.allocResources = {0}, .blocked = 0};
                
                //Initialize max claims for new process
                int index;
                for (index = 0; index < 20; index++){
                    if (bound <= clockptr->systemResources[index]){
                        newProcess.maxRequest[index] = (rand() % bound) + 1;
                    }
                    else {
                        newProcess.maxRequest[index] = (rand()% clockptr->systemResources[index]) + 1;
                    }
                }

                //Forking child.
                if ((childpid = fork()) < 0) {
                    perror(strcat(argv[0],": Error: Failed to create child"));
                }
                else if (childpid == 0) {
                    char limits[30];
                    char param[30];
                    
                    //Convert max claim into strings.
                    int count;
                    for (count = 0; count < 20; count++){
                        sprintf(param, "%d", newProcess.maxRequest[count]);
                        sprintf(limits, "%s", strcat(limits, param));
                    }

                    char *args[]={"./user", limits, NULL};

                    if ((execvp(args[0], args)) == -1) {
                        perror(strcat(argv[0],": Error: Failed to execvp child program\n"));
                        exit(-1);
                    }
                }

                //Adding process to oss processes queue.
                newProcess.pid = childpid;
                enqueue(processes, newProcess);
                clockptr->numChildren++;
                processCount++;
                
                //Adjusting for overhead.
                clockIncrement = (rand() % (50000 - 10000 + 1)) + 10000;
                blockedTime += (long) clockIncrement;
                clockptr->nanoSec += (rand() % (50000 - 10000 + 1)) + 10000;
                if (clockptr->nanoSec > ((int)1e9)) {
                    clockptr->sec += (clockptr->nanoSec / ((int)1e9));
                    clockptr->nanoSec = (clockptr->nanoSec % ((int)1e9));
                }

                //Writing to log file.
                if (writtenTo < 10000){
                    fprintf(logPtr, "OSS : %ld : Adding to system at %d.%d\n", 
                            (long) childpid, clockptr->sec, clockptr->nanoSec);
                    writtenTo++;
                    writeCounter++;
                }
            }
        }

        if (msgrcv(rcmsgid, &recieveMsg, sizeof(sendMsg), 0, IPC_NOWAIT) != -1){
            clockptr->numRequests++;

            //Adjusting for overhead.
            clockIncrement = (rand() % (5000 - 1000 + 1)) + 1000;
            blockedTime += (long)clockIncrement;
            clockptr->nanoSec += clockIncrement;
            if (clockptr->nanoSec > ((int)1e9)){
                clockptr->sec += (clockptr->nanoSec / ((int)1e9));
                clockptr->nanoSec = (clockptr->nanoSec % ((int)1e9));
            }
            
            //Writing to log file.
            if (writtenTo < 10000){
                fprintf(logPtr, "OSS : %s \n", recieveMsg.mesg_text);
                writtenTo++;
                writeCounter++;
            }
            
            sendMsg.mesg_type = recieveMsg.mesg_type;

            if (recieveMsg.choice == 1){
                //Setting up resources for banker's algorithm function to test.
                int processList[18];
                int available[20];
                int maxClaimMatrix[18][20];
                int allotedMatrix[18][20];

                int a,b;
                for (a = 0; a < processCount; a++){
                    processList[a] = 1;
                }

                for (a = 0; a < resourceCount; a++){
                    available[a] = clockptr->availableResources[a];
                    if (a == recieveMsg.choice_index){
                        available[a]--;
                    }
                }

                for (a = 0; a < processCount; a++){
                    for (b = 0; b < resourceCount; b++){
                        maxClaimMatrix[a][b] = processes->array[a].maxRequest[b];
                    }
                }

                for (a = 0; a < processCount; a++){
                    for (b = 0; b < resourceCount; b++){
                        allotedMatrix[a][b] = processes->array[a].allocResources[b];
                        if ((long)processes->array[a].pid == recieveMsg.mesg_type){
                            if (b == recieveMsg.choice_index){
                                allotedMatrix[a][b]++;
                            }
                        }
                    }
                }

                //Granting or denying resource allotment based on algorithm outcome.
                if (isSafe(processList, available, maxClaimMatrix, allotedMatrix, processCount)){
                    sprintf(sendMsg.mesg_text, "Granted");
                    
                    if (writtenTo < 10000){
                        fprintf(logPtr, "OSS : %ld : R%d granted at time %d.%d\n", recieveMsg.mesg_type,
                                recieveMsg.choice_index, clockptr->sec, clockptr->nanoSec);
                        writtenTo++;
                        writeCounter++;   
                    }
                    clockptr->numGranted++;
                    //Updating system resources.
                    clockptr->availableResources[recieveMsg.choice_index]--;
                    
                    //Updating process queue.
                    int a;
                    for(a = 0; a < processes->size; a++){
                        if((long) processes->array[a].pid == recieveMsg.mesg_type){
                            processes->array[a].allocResources[recieveMsg.choice_index]++;
                            processes->array[a].blocked = 0;
                            break;
                        }
                    }
                }
                else{
                    //Oss denies resource and blockes new request until orginal is fulfilled.
                    sprintf(sendMsg.mesg_text, "Denied");
                    if (writtenTo < 10000){
                        fprintf(logPtr, "OSS : %ld : R%d denied and blocked at time %d.%d\n", recieveMsg.mesg_type,
                                recieveMsg.choice_index, clockptr->sec, clockptr->nanoSec);
                        writtenTo++;
                        writeCounter++;
                    }
                    clockptr->numDenied++;

                    //Setting process status in queue to blocked.
                    int a;
                    for(a = 0; a < processes->size; a++){
                        if(((long) processes->array[a].pid) == recieveMsg.mesg_type){
                            processes->array[a].blocked = 1;
                            break;
                        }
                    }
                }

            }
            //If the request is to release a resource.
            else if (recieveMsg.choice == 2){
                sprintf(sendMsg.mesg_text, "Granted");
                
                //Updating system resources.
                clockptr->availableResources[recieveMsg.choice_index]++;
                if (writtenTo < 10000){
                    fprintf(logPtr, "OSS : %ld : Releasing R%d at time %d.%d\n", recieveMsg.mesg_type,
                                recieveMsg.choice_index, clockptr->sec, clockptr->nanoSec);
                    writtenTo++;
                    writeCounter++;
                }
                clockptr->numGranted++;

                //Updating allocated resources in processes queue.
                int a;
                for (a = 0; a < processes->size; a++){
                    if(((long) processes->array[a].pid) == recieveMsg.mesg_type){
                        processes->array[a].allocResources[recieveMsg.choice_index]--;
                        break;
                    }
                }       
            }
            //If process chooses to terminate.
            else if(recieveMsg.choice == 3){
                sprintf(sendMsg.mesg_text, "Granted");
                if (writtenTo < 10000){
                    fprintf(logPtr, "OSS : %ld : Terminating at time %d.%d\n", recieveMsg.mesg_type,
                            clockptr->sec, clockptr->nanoSec);
                    writtenTo++;
                    writeCounter++;
                }
                int a;
                for (a = 0; a < processes->size; a++){
                    if (((long) front(processes)->pid) == recieveMsg.mesg_type){
                        break;
                    }
                    enqueue(processes, *dequeue(processes));
                }

                //Adding resources back into system and removing from process queue.
                for (a = 0; a < 20; a++){
                    clockptr->availableResources[a] += front(processes)->allocResources[a];
                }
                clockptr->totalWait += front(processes)->waitTimeNSec;
                dequeue(processes);
                processCount--;
            }

            //Sending message to process.
            msgsnd(sendmsgid, &sendMsg, sizeof(sendMsg), IPC_NOWAIT);
        }

        //Adding blocked Time to Blocked processes.
        int x;
        for (x = 0; x < processes->size; x++){
            if (processes->array[x].blocked == 1){
                processes->array[x].waitTimeNSec += blockedTime;
            }
        }
        blockedTime = 0;

        if (writeCounter >= 20 && verbose == true){
            printAllocatMatrix(logPtr, processes);
            writeCounter = 0;
        }
        
    }
    
    //Sending signal to all children
    if (flag == 1) {
        if (kill(-parent, SIGQUIT) == -1) {
            perror(strcat(argv[0],": Error: Failed kill"));
            exit(-1);
        }
    }

    long waitAmount = clockptr->totalWait;

    int z;
    for (z = 0; z < processes->size; z++){
        if (processes->array[z].blocked == 1){
            waitAmount += processes->array[z].waitTimeNSec;
        }
    }
    

    fprintf(logPtr, "\nOSS :: System Ending ::\n\nOSS : System Statistics\n");
    fprintf(logPtr, "Total Requests: %d\n", clockptr->numRequests);
    fprintf(logPtr, "Total Granted: %d\n", clockptr->numGranted);
    fprintf(logPtr, "Total Denied: %d\n", clockptr->numDenied);
    fprintf(logPtr, "Total Wait time: %ld ns\n", waitAmount);
    fprintf(logPtr, "Average Request Granted: %f%%\n", 
            ((double)clockptr->numGranted/(double)clockptr->numRequests)*100);
    fprintf(logPtr, "Average Request Denied: %f%%\n", 
            ((double)clockptr->numDenied/(double)clockptr->numRequests)*100);
    fprintf(logPtr, "Average Wait time: %ld ns\n", waitAmount/(long)clockptr->numChildren);

    //Detaching from memory segment.
    if (shmdt(clockptr) == -1) {
        perror(strcat(argv[0],": Error: Failed shmdt detach"));
        clockptr = NULL;
        exit(-1);
    }

    //Removing memory segment.
    if (shmctl(shmid, IPC_RMID, 0) == -1) {
        perror(strcat(argv[0],": Error: Failed shmctl delete"));
        exit(-1);
    }

    //Removing message queues.
    if ((msgctl(sendmsgid, IPC_RMID, NULL)) < 0) {
      perror(strcat(argv[0],": Error: Failed send message queue removal"));
      exit(1);
    }

    if ((msgctl(rcmsgid, IPC_RMID, NULL)) < 0) {
      perror(strcat(argv[0],": Error: Failed recieve message queue removal"));
      exit(1);
    }

    //Closing log file stream.
    fclose(logPtr);
        
    return 0;
}

//Function to check whether string is a positive integer
int is_pos_int(char test_string[]){
	int is_num = 0, i;
	for(i = 0; i < strlen(test_string); i++)
	{
		if(!isdigit(test_string[i]))
			is_num = 1;
	}

	return is_num;
} 

//Function to find whether the system is in a safe state.
bool isSafe(int processList[], int available[], int maxClaim[][20], 
            int allot[][20], int numProcess) {
     int counter = numProcess, index, index2, exec, safe;
     while (counter != 0) {
            //Variable to indicate sequence is safe.
            safe = 0;
            for (index = 0; index < numProcess; index++) {
                    if (processList[index]) {
                        exec = 1;
                        for (index2 = 0; index2 < 20; index2++) {
                            if (maxClaim[index][index2] - allot[index][index2] > available[index2]) {
                                exec = 0;
                                break;
                            }
                        }
                        if (exec) {
                            processList[index] = 0;
                            counter--;
                            safe = 1;

                            for (index2 = 0; index2 < 20; index2++) {
                                available[index2] += allot[index][index2];
                            }
                            break;
                        }
                    }
            }
            if (!safe) {
                return false;
            } 
     }
     return true;
}

//Function to print currently allocated table.
void printAllocatMatrix(FILE *stream, queue *processes){
    int a,b;

    fprintf(stream, "\nCurrent Allocated Resources\n");
    fprintf(stream, "       ");
    for (a = 0; a < 20; a++){
        fprintf(stream, "R%d\t", a);
    }
    for (a = 0; a < processes->size; a++){
        fprintf(stream, "\n%ld : ", (long)processes->array[a].pid);
        for (b = 0; b < 20; b++){
            fprintf(stream, "%d\t", processes->array[a].allocResources[b]);
        }
    }
    fprintf(stream, "\n\n");

    return;
}

//Signal handler for 2 sec alarm
void alarmHandler() {
    flag = 1;
}

//Signal handler for Ctrl-C
void interruptHandler() {
    flag = 1;
}