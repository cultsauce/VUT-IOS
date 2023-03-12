/******** IOS PROJEKT 2
**** BUILDING H2O - proj2.h
********* xkubin27
********* 29.04.2022
**/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <fcntl.h>          
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

// macros to map/ unmap shared memory
#define MMAP(pointer) {(pointer) = mmap(NULL, sizeof(*(pointer)), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);}
#define UNMAP(pointer) {munmap((pointer), sizeof(pointer));}

// struct to store everything that needs to be shared
typedef struct shared
{
    int NH, NO, TB, TI, counter, oxycounter, hydrocounter, molcounter, barrier_count;
    sem_t oxyQueue, hydroQueue, mutex, writing, hydroMsg, oxyMsg, oxyBond, hydroBond;
    FILE *out;
} shared_t;

// global variable of type struct shared
shared_t *shared;

