#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>

#define MAX_SIZE 10
#define SHM_KEY 123456 // Shared memory key

// Shared queue structure
typedef struct
{
    int queue[MAX_SIZE];
    int count; // Current number of elements in the queue
} SharedQueue;

// Utility function to print the queue status
void print_queue_status(SharedQueue *sq, const char *message)
{
    printf("%s\n", message);
    printf("Queue: [");
    for (int i = 0; i < MAX_SIZE; ++i)
    {
        printf("%d", sq->queue[i]);
        if (i < MAX_SIZE - 1)
        {
            printf(", ");
        }
    }
    printf("] (Count: %d)\n", sq->count);
}

// Producer: Add n values to the end of the queue
void producer_process(SharedQueue *sq)
{
    if (sq->count == MAX_SIZE)
    {
        printf("Producer: Queue is full. Cannot add more items.\n");
        return;
    }

    int n_add;
    printf("Producer: Enter number of items to add (space left: %d): ", MAX_SIZE - sq->count);
    if (scanf("%d", &n_add) != 1 || n_add < 0)
    {
        printf("Producer: Invalid input.\n");
        while (getchar() != '\n' && getchar() != EOF);
        return;
    }
    while (getchar() != '\n' && getchar() != EOF);

    if (n_add == 0)
    {
        printf("Producer: No items added.\n");
        return;
    }

    if (n_add > MAX_SIZE - sq->count)
    {
        printf("Producer: Input (%d) exceeds available space, will add up to %d items.\n", n_add, MAX_SIZE - sq->count);
        n_add = MAX_SIZE - sq->count;
    }

    printf("Producer: Enter %d values:\n", n_add);
    for (int i = 0; i < n_add; ++i)
    {
        if (sq->count >= MAX_SIZE)
        {
            printf("Producer: Queue became full during input.\n");
            break;
        }
        printf("  Enter value #%d: ", i + 1);
        int val;
        if (scanf("%d", &val) != 1)
        {
            printf("Producer: Invalid input for value. Skipping.\n");
            while (getchar() != '\n' && getchar() != EOF);
            i--;
            continue;
        }
        while (getchar() != '\n' && getchar() != EOF);

        // Add to the "left" of the current data block (right-aligned)
        sq->queue[MAX_SIZE - 1 - sq->count] = val;
        sq->count++;
    }
    printf("Producer: Finished adding items.\n");
}

// Consumer: Remove n values from the end of the queue and shift the array
void consumer_process(SharedQueue *sq)
{
    if (sq->count == 0)
    {
        printf("Consumer: Queue is empty. Nothing to consume.\n");
        return;
    }

    int n_consume;
    printf("Consumer: Enter number of items to consume (available: %d): ", sq->count);
    if (scanf("%d", &n_consume) != 1 || n_consume < 0)
    {
        printf("Consumer: Invalid input.\n");
        while (getchar() != '\n' && getchar() != EOF);
        return;
    }
    while (getchar() != '\n' && getchar() != EOF);

    if (n_consume == 0)
    {
        printf("Consumer: No items consumed.\n");
        return;
    }

    if (n_consume > sq->count)
    {
        printf("Consumer: Requested (%d) exceeds available (%d). Consuming all available items.\n", n_consume, sq->count);
        n_consume = sq->count;
    }

    printf("Consumer: Consuming %d items from the end of the queue...\n", n_consume);

    int old_count = sq->count;
    int new_count = old_count - n_consume;

    // 1. Preserve the remaining elements
    int temp_preserved[MAX_SIZE];
    for (int i = 0; i < new_count; ++i)
    {
        temp_preserved[i] = sq->queue[MAX_SIZE - old_count + i];
    }

    // 2. Clear the queue
    for (int i = 0; i < MAX_SIZE; ++i)
    {
        sq->queue[i] = 0;
    }

    // 3. Restore preserved elements, right-aligned
    for (int i = 0; i < new_count; ++i)
    {
        sq->queue[MAX_SIZE - new_count + i] = temp_preserved[i];
    }

    sq->count = new_count;
    printf("Consumer: Finished consuming.\n");
}

int main()
{
    int shmid;
    SharedQueue *shared_data_ptr;

    // 1. Create or get shared memory
    shmid = shmget(SHM_KEY, sizeof(SharedQueue), IPC_CREAT | 0666);
    if (shmid == -1)
    {
        perror("shmget error");
        exit(EXIT_FAILURE);
    }

    // 2. Attach shared memory to process address space
    shared_data_ptr = (SharedQueue *)shmat(shmid, NULL, 0);
    if (shared_data_ptr == (void *)-1)
    {
        perror("shmat error");
        shmctl(shmid, IPC_RMID, NULL);
        exit(EXIT_FAILURE);
    }

    // 3. Initialize queue (only once per run)
    static int initialized_in_this_run = 0;
    if (!initialized_in_this_run)
    {
        printf("Initializing queue for this run...\n");
        shared_data_ptr->count = 0;
        for (int i = 0; i < MAX_SIZE; ++i)
        {
            shared_data_ptr->queue[i] = 0;
        }
        initialized_in_this_run = 1;
        print_queue_status(shared_data_ptr, "Queue status after initialization:");
    }

    char continue_loop_char = 'y';
    do
    {
        printf("\n================ NEW LOOP ================\n");
        // --- Producer phase (Parent process) ---
        printf("--- Producer Phase (Parent Process) ---\n");
        producer_process(shared_data_ptr);
        print_queue_status(shared_data_ptr, "Queue status after Producer:");

        // --- Fork child process for Consumer ---
        pid_t pid = fork();

        if (pid == -1)
        {
            perror("fork error");
            shmdt(shared_data_ptr);
            shmctl(shmid, IPC_RMID, NULL);
            exit(EXIT_FAILURE);
        }

        if (pid == 0)
        { // Child process (Consumer)
            printf("\n--- Consumer Phase (Child Process) ---\n");
            consumer_process(shared_data_ptr);
            if (shmdt(shared_data_ptr) == -1)
            {
                perror("shmdt error in child");
            }
            exit(0);
        }
        else
        { // Parent process
            printf("\nParent (PID: %d) waiting for Child (PID: %d) (Consumer) to finish...\n", getpid(), pid);

            int child_status;
            wait(&child_status);

            if (WIFEXITED(child_status))
            {
                printf("Child (Consumer) finished with exit code %d.\n", WEXITSTATUS(child_status));
            }
            else
            {
                printf("Child (Consumer) terminated abnormally.\n");
            }

            print_queue_status(shared_data_ptr, "Queue status after Consumer (from Parent):");
            printf("\nContinue Producer-Consumer loop? (enter 'y' to continue, any other key to exit): ");
            scanf(" %c", &continue_loop_char);
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
        }
    } while (continue_loop_char == 'y' || continue_loop_char == 'Y');

    // --- Cleanup ---
    printf("\nProgram ending. Cleaning up shared memory...\n");
    if (shmdt(shared_data_ptr) == -1)
    {
        perror("shmdt error in parent (end of program)");
    }
    if (shmctl(shmid, IPC_RMID, NULL) == -1)
    {
        perror("shmctl IPC_RMID error");
    }

    printf("Cleanup done! Exit program\n");
    return 0;
}