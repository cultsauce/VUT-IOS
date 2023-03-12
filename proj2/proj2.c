/******** IOS PROJEKT 2
**** BUILDING H2O - proj2.c
********* xkubin27
********* 29.04.2022
**/

#include "proj2.h"

int parse_args(int argc, char **argv, int *TI, int *TB, int *NO, int *NH);
int conv_to_int(char *str, int *val);


int init(int argc, char **argv)
{ 
    // allocate shared memory
    MMAP(shared);

    // open file for output
    shared->out = fopen("proj2.out", "w");
    if (shared->out == NULL)
    {
        fprintf(stderr, "error opening file\n");
        return 0;
    }
    setbuf(shared->out, NULL);
    if (!parse_args(argc, argv, &shared->TI, &shared->TB, &shared->NO, &shared->NH)) return 0;

    // inistialize shared counters
    shared->hydrocounter = 1;
    shared->oxycounter = 1;
    shared->counter = 1;
    shared->molcounter = 1;
    shared->barrier_count = 0;

    // initialize semaphores
    if (sem_init(&shared->oxyQueue, 1, 1) == -1 ||  
        sem_init(&shared->hydroQueue, 1, 2) == -1 || 
        sem_init(&shared->mutex, 1, 1) == -1 || 
        sem_init(&shared->hydroMsg, 1, 0) == -1 ||
        sem_init(&shared->hydroBond, 1, 0) == -1 ||
        sem_init(&shared->oxyBond, 1, 0) == -1 || 
        sem_init(&shared->oxyMsg, 1, 0) == -1 ||
        sem_init(&shared->writing, 1, 1) == -1)
        return 0;

    return 1;
}

int destroy()
{
    // free all semaphores and close file
    if (fclose(shared->out) != 0 || 
        sem_destroy(&shared->mutex) == -1 || 
        sem_destroy(&shared->hydroQueue) == -1 || 
        sem_destroy(&shared->oxyQueue) == -1 || 
        sem_destroy(&shared->writing) == -1 || 
        sem_destroy(&shared->oxyMsg) == -1 ||
        sem_destroy(&shared->hydroMsg) == -1 ||
        sem_destroy(&shared->hydroBond) == -1 ||
        sem_destroy(&shared->oxyBond) == -1)
        return 0;

    // free shared memory
    UNMAP(shared);
    return 1;
}

void process_oxygen()
{
    sem_wait(&shared->mutex);
    int idO = shared->oxycounter++;
    sem_post(&shared->mutex);

    sem_wait(&shared->writing);
    fprintf(shared->out, "%d: O %d: started\n", shared->counter++, idO);
    sem_post(&shared->writing);

    usleep(rand() % shared->TI);

    sem_wait(&shared->writing);
    fprintf(shared->out, "%d: O %d: going to queue\n", shared->counter++, idO);
    sem_post(&shared->writing);

    sem_wait(&shared->oxyQueue);

    sem_wait(&shared->mutex);
    // not enough atoms left to make a molecule
    if (shared->NH - (shared->molcounter - 1) * 2 < 2 || shared->NO - (shared->molcounter - 1) < 1)
    {
        sem_wait(&shared->writing);
        fprintf(shared->out, "%d: O %d: not enough H\n", shared->counter++, idO);

        sem_post(&shared->writing);
        
        sem_post(&shared->oxyQueue);
        sem_post(&shared->mutex);

        fclose(shared->out);
        exit(0);
    }
    sem_post(&shared->mutex);

    // wait for two hydrogens to arrive
    sem_wait(&shared->oxyBond);
    sem_wait(&shared->oxyBond);
    sem_post(&shared->hydroBond);
    sem_post(&shared->hydroBond);

    int molId = shared->molcounter;
    sem_wait(&shared->writing);
    fprintf(shared->out, "%d: O %d: creating molecule %d\n", shared->counter++, idO, molId);
    sem_post(&shared->writing);


    usleep(rand() % shared->TB);

    // wait for two hydrogens to finish bonding
    sem_wait(&shared->oxyMsg);
    sem_wait(&shared->oxyMsg);

    sem_wait(&shared->writing);
    fprintf(shared->out, "%d: O %d: molecule %d created\n", shared->counter++, idO, molId);
    sem_post(&shared->writing);

    // signal two of the oxygens that the molecule is ready
    sem_post(&shared->hydroMsg);
    sem_post(&shared->hydroMsg);  

    sem_wait(&shared->mutex); 
    shared->barrier_count++;
    if (shared->barrier_count >= 3)
    {   
        shared->molcounter++;
        shared->barrier_count = 0;   
        sem_post(&shared->oxyQueue);
        sem_post(&shared->hydroQueue);
        sem_post(&shared->hydroQueue);
    }
    sem_post(&shared->mutex);

    fclose(shared->out);
    exit(0);
}

void process_hydrogen()
{
    sem_wait(&shared->mutex);
    int idH = shared->hydrocounter++;
    sem_post(&shared->mutex);

    sem_wait(&shared->writing);
    fprintf(shared->out, "%d: H %d: started\n", shared->counter++, idH);
    sem_post(&shared->writing);

    usleep(rand() % shared->TI);

    sem_wait(&shared->writing);
    fprintf(shared->out, "%d: H %d: going to queue\n", shared->counter++, idH);
    sem_post(&shared->writing);

    sem_wait(&shared->hydroQueue);

    sem_wait(&shared->mutex);
    if (shared->NH - (shared->molcounter - 1) * 2 < 2 || shared->NO - (shared->molcounter - 1) < 1)
    {
        sem_wait(&shared->writing);
        fprintf(shared->out, "%d: H %d: not enough O or H\n", shared->counter++, idH);
        sem_post(&shared->writing);
        
        sem_post(&shared->hydroQueue);
        sem_post(&shared->mutex);

        fclose(shared->out);
        exit(0);
    }
    sem_post(&shared->mutex);

    sem_post(&shared->oxyBond);
    sem_wait(&shared->hydroBond);


    int molId = shared->molcounter;
    sem_wait(&shared->writing);
    fprintf(shared->out, "%d: H %d: creating molecule %d\n", shared->counter++, idH, molId);
    sem_post(&shared->writing);

    // wait for message from oxygen and release
    sem_post(&shared->oxyMsg);
    sem_wait(&shared->hydroMsg);

    sem_wait(&shared->mutex);
    shared->barrier_count++;
    if (shared->barrier_count >= 3)
    {   
        shared->molcounter++;
        shared->barrier_count = 0;
        sem_post(&shared->oxyQueue);
        sem_post(&shared->hydroQueue);
        sem_post(&shared->hydroQueue);
    }
    sem_post(&shared->mutex);

    sem_wait(&shared->writing);
    fprintf(shared->out, "%d: H %d: molecule %d created\n", shared->counter++, idH, molId);
    sem_post(&shared->writing);

    fclose(shared->out);
    exit(0);
}

// generate num number of processes that execute the child_func
int generate_proc(int num, pid_t *children, void (*child_func)())
{
    for (int i = 0; i < num; i++)
    {
        pid_t child = fork();

        if (child == 0)
        {
            srand(child * time(0));
            child_func();
        }
        else if (child == -1)
        {
            fprintf(stderr, "failed fork\n");
            for (int j = 0; j < i; j++)
            {
                kill(children[j], SIGKILL);
            }
            return 0;
        }
        else
        {
            children[i] = child;
        }
    }
    return 1;
}

// convert string to integer and validate
int conv_to_int(char *str, int *val)
{
    char *tmp;
    long ans = strtoul(str, &tmp, 10);
    if (strlen(tmp) != 0 || ans < 0 || strlen(str) == 0)
    {
        return 0;
    }
    *val = ans;
    return 1;
}

// parse input arguments
int parse_args(int argc, char **argv, int *TI, int *TB, int *NO, int *NH)
{
    if (argc != 5)
    {
        fprintf(stderr, "nespravny pocet argumentu\n");
        return 0;
    }
    if (!(conv_to_int(argv[1], NO) && conv_to_int(argv[2], NH) && conv_to_int(argv[3], TI) && conv_to_int(argv[4], TB)))
    {
        fprintf(stderr, "invalid arguments supplied\n");
        return 0;
    }
    if (*TI < 0 || *TI > 1000 || *TB < 0 || *TB > 1000)
    {
        fprintf(stderr, "TI and TB has to be in the interval <0, 1000>");
        return 0;
    }
    return 1;
}

int main(int argc, char **argv)
{
    if (!init(argc, argv))
    {
        destroy();
        return 1;
    }

    // convert to microseconds
    shared->TB = (shared->TB + 1) * 1000;
    shared->TI = (shared->TI + 1) * 1000;
    
    pid_t oxygens[shared->NO], hydrogens[shared->NH];

    // generate processes for hydrogen and oxygen
    if (!generate_proc(shared->NH, hydrogens, process_hydrogen)) 
    {
        destroy();
        return 1;
    }
    if (!generate_proc(shared->NO, oxygens, process_oxygen))
    {
        // if process generating failed, kill all other processes
        for (int j = 0; j < shared->NH; j++)
        {
            kill(hydrogens[j], SIGKILL);
        }
        destroy();
        return 1;
    }

    int status;
    // wait for all child processes to finish
    for (int i = 0; i < shared->NH; i++)
    {
        waitpid(hydrogens[i], &status, 0);
    }
    for (int i = 0; i < shared->NO; i++)
    {
        waitpid(oxygens[i], &status, 0);
    }

    // cleanup
    if (!destroy()) return 1;

    return 0;
}