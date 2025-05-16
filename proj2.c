#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <sys/sem.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <sys/ipc.h>
#include <time.h>
#include <limits.h>
#include <sys/shm.h>
#include <string.h>
#define FILE_NAME "proj2.out"

/*
    IOS 2.Project - Semaphors
    Jakub Fiľo - xfiloja00
    28.4.2024
    FIT
*/

// Global file pointer
FILE *file;

// Semaphores for synchronization
sem_t *mutex;
sem_t *bus_ReadyToBoard;
sem_t *bus_ReadyToUnBoard;
sem_t *Stop[11];
sem_t *boarded;
sem_t *gotOff;

// Shared data structure - Accessible by all processes
typedef struct
{
    int skiersWaiting[11];
    int currentStop;
    int skiersOnBus;
    int order;
    int left;
    int started;
} SharedData;

SharedData *shared_data;

// Checks command-line arguments and validates their values
int checkStartConditions(int *Skiers, int *idZ, int *Capacity, int *TL, int *TB, int argc, char *argv[]);

// Generic error handling and exit function
void exit_error(char *msg, int errcode);

// Initializes semaphores
void Init_Semaphores();

// Destroys (cleans up) semaphores
void Destroy_Semaphores();

// The bus process logic
void skibus_process(int idZ, int Capacity, int TB);

// The logic for a skier process
void skier_process(int idL, int idZ, int TL, int Capacity);

// Removes any pre-existing semaphores
void unlinkall();

// Returns the smaller number
int min(int a,int b);

// Returns the bigger number
int max(int a,int b);

// Returns random from between min-max, each process different number
int random_int(int min,int max);


int checkStartConditions(int *Skiers, int *idZ, int *Capacity, int *TL, int *TB, int argc, char *argv[])
{
    if (argc != 6)
    {
        fprintf(stderr, "Wrong number of arguments. Usage: ./proj2 Skiers idZ Capacity TL TB\n");
        return 1;
    }

    *Skiers = strtol(argv[1], NULL, 10);
    *idZ = strtol(argv[2], NULL, 10);
    *Capacity = strtol(argv[3], NULL, 10);
    *TL = strtol(argv[4], NULL, 10);
    *TB = strtol(argv[5], NULL, 10);

    if (*Skiers < 0 || *Skiers >= 20000)
    {
        fprintf(stderr, "Invalid skiers: %d (range: 0-19999)\n", *Skiers);
        return 1;
    }
    else if (!(0 < *idZ && *idZ <= 10))
    {
        fprintf(stderr, "Invalid bus stops: %d (range: 1-10)\n", *idZ);
        return 1;
    }
    else if (!(10 <= *Capacity && *Capacity <= 100))
    {
        fprintf(stderr, "Invalid capacity: %d (range: 10-100)\n", *Capacity);
        return 1;
    }
    else if (!(0 <= *TL && *TL <= 10000))
    {
        fprintf(stderr, "Invalid wait time: %d ms (range: 0-10000)\n", *TL);
        return 1;
    }
    else if (!(0 <= *TB && *TB <= 1000))
    {
        fprintf(stderr, "Invalid ride time: %d ms (range: 0-1000)\n", *TB);
        return 1;
    }

    return 0;
}

void exit_error(char *msg, int errcode)
{
    fprintf(stderr, "Error: %s Exit code: %d\n", msg, errcode);
    exit(errcode);
}

void Init_Semaphores()
{

    mutex = sem_open("/mutex_name", O_CREAT | O_EXCL, 0666, 1);
    if (mutex == SEM_FAILED)
    {
        fclose(file);
        exit_error("sem_open(mutex)", 1);
    }

    gotOff = sem_open("/gotOff_name", O_CREAT | O_EXCL, 0666, 0);
    if (gotOff == SEM_FAILED)
    {
        fclose(file);
        exit_error("sem_open(gotOff_name)", 1);
    }

    bus_ReadyToBoard = sem_open("/bus_ReadyToBoard_name", O_CREAT | O_EXCL, 0666, 0);
    if (bus_ReadyToBoard == SEM_FAILED)
    {
        fclose(file);
        exit_error("sem_open(bus_ReadyToBoard_name)", 1);
    }

    boarded = sem_open("/boarded_name", O_CREAT | O_EXCL, 0666, 0);
    if (boarded == SEM_FAILED)
    {
        fclose(file);
        exit_error("sem_open(boarded_name)", 1);
    }

    bus_ReadyToUnBoard = sem_open("/bus_ReadyToUnBoard_name", O_CREAT | O_EXCL, 0666, 0);
    if (bus_ReadyToUnBoard == SEM_FAILED)
    {
        fclose(file);
        exit_error("sem_open(bus_ReadyToUnBoard_name)", 1);
    }

    char stopName[20]; // Adjust size if needed
    for (int i = 0; i <= 10; i++)
    {
        sprintf(stopName, "/stop_%d", i);
        Stop[i] = sem_open(stopName, O_CREAT, 0666, 0);
        if (Stop[i] == SEM_FAILED)
        {
            fclose(file);
            exit_error("sem_open(Stop)", 1);
        }
    }
}

void Destroy_Semaphores()
{
    sem_close(mutex);
    sem_unlink("/mutex_name");
    sem_close(gotOff);
    sem_unlink("/gotOff_name");
    sem_close(bus_ReadyToBoard);
    sem_unlink("/bus_ReadyToBoard_name");
    sem_close(boarded);
    sem_unlink("/boarded_name");
    sem_close(bus_ReadyToUnBoard);
    sem_unlink("/bus_ReadyToUnBoard_name");

    char stopName[20];
    for (int i = 0; i <= 10; i++)
    {
        sprintf(stopName, "/stop_%d", i);
        sem_close(Stop[i]);
        sem_unlink(stopName);
    }
}

void unlinkall()
{
    sem_unlink("/mutex_name");
    sem_unlink("/gotOff_name");
    sem_unlink("/bus_ReadyToBoard_name");
    sem_unlink("/boarded_name");
    sem_unlink("/bus_ReadyToUnBoard_name");
    char stopName[20];
    for (int i = 0; i <= 10; i++)
    {
        sprintf(stopName, "/stop_%d", i);
        sem_unlink(stopName);
    }
}

int random_int(int min, int max)
{
    srand(time(NULL) * getpid());
    return rand() % (max - min + 1) + min;
}

int min(int a, int b)
{
    return (a < b ? a : b);
}

int max(int a, int b)
{
    return (a > b ? a : b);
}

void skibus_process(int idZ, int Capacity, int TB)
{
    sem_wait(mutex);
    fprintf(file, "%d: BUS: started\n", shared_data->order++);
    sem_post(mutex);

    shared_data->currentStop = 1; // Start at the first stop
    while (1)
    {
        // Travel to currentStop
        usleep(random_int(0, TB));

        sem_wait(mutex);
        fprintf(file, "%d: BUS: arrived to %d\n", shared_data->order++, shared_data->currentStop);
        
        // How many skiers can board
        int freeSeats = Capacity - shared_data->skiersOnBus;
        int n = min(shared_data->skiersWaiting[shared_data->currentStop], freeSeats);

        // Boarding logic
        if (!(shared_data->skiersOnBus == Capacity))
        {
            for (int i = 1; i <= n; i++)
            {
                sem_post(Stop[shared_data->currentStop]);
                sem_wait(boarded);
            }
        }
        sem_post(mutex);

        // Leaving the stop
        sem_wait(mutex);
        fprintf(file, "%d: BUS: leaving %d\n", shared_data->order++, shared_data->currentStop);
        sem_post(mutex);

        // Move to next stop (or handle reaching the final stop)
        if (shared_data->currentStop < idZ)
        {
            sem_wait(mutex);
            shared_data->currentStop++;
            sem_post(mutex);
        }
        else if (shared_data->currentStop == idZ)
        {
            sem_wait(mutex);
            fprintf(file, "%d: BUS: arrived to final\n", shared_data->order++);
            sem_post(mutex);

            // Unboarding at the final stop
            sem_wait(mutex);
            int exiting = shared_data->skiersOnBus;
            for (int i = 1; i <= exiting; i++)
            {
                sem_post(bus_ReadyToUnBoard);
                sem_wait(gotOff);
            }
            sem_post(mutex);

            // Leaving the final stop
            sem_wait(mutex);
            fprintf(file, "%d: BUS: leaving final\n", shared_data->order++);
            sem_post(mutex);

            // Check if all skiers have finished or if another round is needed
            int anyWaiting = 0;
            if (shared_data->left != shared_data->started)
            {
                anyWaiting = 1; // Skiers still left
            }
            else
            {
                for (int i = 1; i <= idZ; i++)
                {
                    if (shared_data->skiersWaiting[i] > 0)
                    {
                        anyWaiting = 1;
                        break;
                    }
                }
            }

            // Bus finish logic
            if (!anyWaiting)
            {
                sem_wait(mutex);
                fprintf(file, "%d: BUS: finish\n", shared_data->order);

                sem_post(mutex);
                break; // Exit the while loop
            }
            else // Start another round
            {
                shared_data->currentStop = 1;
            }
        }
    }
    fclose(file);
}

void skier_process(int idL, int assignedStop, int TL, int Capacity)
{
    sem_wait(mutex);
    fprintf(file, "%d: L %d: started\n", shared_data->order++, idL);
    sem_post(mutex);

    usleep(random_int(0, TL)); // Breakfast

    // Arrive at the assigned bus stop 
    sem_wait(mutex);
    fprintf(file, "%d: L %d: arrived to %d\n", shared_data->order++, idL, assignedStop);
    shared_data->skiersWaiting[assignedStop]++;
    sem_post(mutex);

    // Board the bus
    sem_wait(Stop[assignedStop]);
    if (shared_data->skiersOnBus == Capacity)
    {
        sem_post(boarded); // If the bus was full, just signal boarding
    }
    if (shared_data->skiersOnBus < Capacity)
    {
        fprintf(file, "%d: L %d: boarding\n", shared_data->order++, idL);
        shared_data->skiersOnBus++;
        shared_data->skiersWaiting[assignedStop]--;
        sem_post(boarded); // Signal to the bus that the skier has boarded
    }

    // Unboarding & Skiing
    sem_wait(bus_ReadyToUnBoard);
    shared_data->skiersOnBus--;
    fprintf(file, "%d: L %d: going to ski\n", shared_data->order++, idL);
    shared_data->left++;
    sem_post(gotOff);

    fclose(file);
}

int main(int argc, char *argv[])
{
    // Disable buffering for standard error
    setbuf(stderr, NULL);

    // Variables to hold the command-line arguments
    int Skiers = 0, idZ = 0, Capacity = 0, TL = 0, TB = 0;

    // Validate command-line arguments
    if (checkStartConditions(&Skiers, &idZ, &Capacity, &TL, &TB, argc, argv))
    {
        return 1;
    }

    // Open output log file
    if ((file = fopen(FILE_NAME, "w")) == NULL)
    {
        exit_error("Failed to open file.", 1);
    }

    // Disable buffering for the output file
    setbuf(file, NULL);

    // Clean up any pre-existing semaphores or shared memory
    unlinkall();

    // Initialize semaphores  for synchronization
    Init_Semaphores();

    // Allocate shared memory for communication between processes
    shared_data = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if (shared_data == (void *)-1)
    {
        Destroy_Semaphores();
        fclose(file);
        exit_error("Failed to attach shared memory", 1);
    }
    // Initialize values in the shared data structure
    shared_data->skiersOnBus = 0;
    shared_data->order = 1;
    shared_data->left = 0;
    shared_data->started = Skiers;

    // Create the bus process
    pid_t skibus_pid = fork();
    if (skibus_pid == -1)
    {
        Destroy_Semaphores();
        fclose(file);
        exit_error("Failed to create skibus process", 1);
    }
    else if (skibus_pid == 0)
    {
        // Bus process logic
        skibus_process(idZ, Capacity, TB);
        exit(0);
    }
    else
    {
         // Parent process: Create skier processes
        for (int idL = 1; idL <= Skiers; idL++)
        {
            pid_t skier_pid = fork();
            if (skier_pid == -1)
            {
                Destroy_Semaphores();
                fclose(file);
                exit_error("Failed to create skier process", 1);
            }
            else if (skier_pid == 0)
            {
                // Skier process logic
                skier_process(idL, random_int(1, idZ), TL, Capacity);
                exit(0);
            }
        }
    }
    // Parent process: Wait for all children to finish
    while (wait(NULL) > 0)
        ;
    fclose(file);
    Destroy_Semaphores();
    // Release shared memory 
    munmap(shared_data, sizeof(SharedData));
    return 0;
}
