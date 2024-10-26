#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>

//
// For calculating time span
//

struct timespec start_cpu, end_cpu;
struct timespec start_real, end_real;
double cpu_time_used, real_time_elapsed;

//
// Buffer settings
//

// number of buffer
// x >> x + 1 >> x * 2 >> printf x
// so it is 3
#define NUM_BUFFERS 3


// number of thread
// x + 1 and x * 2, each has ? threads
#define NUM_THREADS 3


// number of x
#define NUM_ITEMS 100
int num_items = 0;
pthread_spinlock_t num_lock;

//
// Struct to hold the Buffers and Semaphores
//

#define BUFFER_SIZE 5

#define STOP_SIGNAL - 1

struct Buffer {
    int buffer[BUFFER_SIZE];
    int in;
    int out;
    sem_t mutex;
    sem_t empty;
    sem_t full;
};

void initBuffer(struct Buffer *buffer) { // Function to initialize the buffer and semaphores
    buffer->in = 0;
    buffer->out = 0;
    sem_init(&buffer->mutex, 0, 1); // Initialize mutex to 1
    sem_init(&buffer->empty, 0, BUFFER_SIZE); // Initialize empty to BUFFER_SIZE (number of empty spaces)
    sem_init(&buffer->full, 0, 0); // Initialize full to 0 (number of spaces with value)
}

void destroyBuffer(struct Buffer *buffer) { // Function to destroy

    sem_destroy(&buffer->mutex);
    sem_destroy(&buffer->empty);
    sem_destroy(&buffer->full);
}

void produce(struct Buffer *buffer, int item) { // Function to add an item to the buffer
    sem_wait(&buffer->empty); // Wait for empty slot
    sem_wait(&buffer->mutex); // Lock the buffer
    buffer->buffer[buffer->in] = item; // Add item to buffer
    buffer->in = (buffer->in + 1) % BUFFER_SIZE; // Increment buffer index
    sem_post(&buffer->mutex); // Unlock the buffer
    sem_post(&buffer->full); // Increment full count
}

int consume(struct Buffer *buffer) { // Function to remove an item from the buffer
    sem_wait(&buffer->full); // Wait for a full slot
    sem_wait(&buffer->mutex); // Lock the buffer
    int item = buffer->buffer[buffer->out]; // Remove item from buffer
    buffer->out = (buffer->out + 1) % BUFFER_SIZE; // Increment buffer index
    sem_post(&buffer->mutex); // Unlock the buffer
    sem_post(&buffer->empty); // Increment empty count
    return item;
}

//
// Structure to store Buffers of two stages
//

struct TransferBuffer {
    struct Buffer *from;
    struct Buffer *to;
};

void initTransferBuffer(struct TransferBuffer *trans, struct Buffer *first, struct Buffer *second) {
    trans->from = first;
    trans->to = second;
}

//
// Functions
//

void* producer(void* arg) {
    // time used
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_cpu);
    clock_gettime(CLOCK_REALTIME, &start_real);

    struct Buffer *buffer = (struct Buffer*)arg;
    while (1) {
    pthread_spin_lock(&num_lock);
        int item;
        if (num_items == NUM_ITEMS) {
            item = STOP_SIGNAL; // send the stop signal
            num_items++; // stop in next loop;
        }
        else if (num_items > NUM_ITEMS) {
            break;
        }
        else {
            item = num_items++;//rand() % 100; // Generate a random item
        }
    pthread_spin_unlock(&num_lock);
        produce(buffer, item); // Produce item
        //printf("Produced item: %d\n", item);
    }
    pthread_exit(NULL);
}

void* add_one(void* arg) {
    struct TransferBuffer *trans = (struct TransferBuffer*)arg;
    struct Buffer *from = (struct Buffer*)trans->from;
    struct Buffer *to = (struct Buffer*)trans->to;
    while (1) {
        int x = consume(from);
        if (x == - 1) {
            produce(to, - 1);
            break;
        }
        int y = x + 1;
        sleep(0.1); // simulate complex caculation time
        produce(to, y);
        //printf(" Add 1: %d + 1 = %d\n", x, y);
    }
    pthread_exit(NULL);
}

void* muliple_two(void* arg) {
    struct TransferBuffer *trans = (struct TransferBuffer*)arg;
    struct Buffer *from = (struct Buffer*)trans->from;
    struct Buffer *to = (struct Buffer*)trans->to;
    while (1) {
        int x = consume(from);
        if (x == STOP_SIGNAL) {
            produce(to, STOP_SIGNAL);
            break;
        }
        int y = x * 2;
        sleep(0.1); // simulate complex caculation time
        produce(to, y);
        //printf(" Multiple 2: %d * 2 = %d\n", x, y);
    }
    pthread_exit(NULL);
}

void* consumer(void* arg) {
    struct Buffer *buffer = (struct Buffer*)arg;
    while (1) {
        int item = consume(buffer);
        if (item == STOP_SIGNAL) {
            // Time used
            clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_cpu);
            cpu_time_used = (double)(end_cpu.tv_sec - start_cpu.tv_sec) +
                    (double)(end_cpu.tv_nsec - start_cpu.tv_nsec) / 1e9;
            printf("CPU time used: %f seconds\n", cpu_time_used);
            clock_gettime(CLOCK_REALTIME, &end_real);
            real_time_elapsed = (double)(end_real.tv_sec - start_real.tv_sec) +
                                (double)(end_real.tv_nsec - start_real.tv_nsec) / 1e9;
            printf("Real-time elapsed: %f seconds\n", real_time_elapsed);
        }
        printf(" Consumed item: %d\n", item);
    }
    pthread_exit(NULL);
}

//
// timeout design
//

sem_t timeout;
// timeout handler: to signal timeout
void stop_signal(int signum) {
    sem_post(&timeout);
}

//
// Main
//

int main() {
    // Initialize buffer and semaphores
    struct Buffer buffer[NUM_BUFFERS];
    for (int i = 0; i < NUM_BUFFERS; i++) {
        initBuffer(&buffer[i]);
    }
    struct TransferBuffer transfer[NUM_BUFFERS - 1];
    for (int i = 0; i < NUM_BUFFERS - 1; i++) {
        initTransferBuffer(&transfer[i], &buffer[i], &buffer[i + 1]);
    }

    // Initialize producer_thread and consumer_thread
    pthread_spin_init(&num_lock, PTHREAD_PROCESS_SHARED);
    pthread_t producer_thread;
    pthread_t consumer_thread;
    // Initialize consumer_thread
    pthread_t add_thread[NUM_THREADS];
    pthread_t multiple_thread[NUM_THREADS];
    // Create producer and consumer threads
    pthread_create(&producer_thread, NULL, producer, (void*)&buffer[0]);
    pthread_create(&consumer_thread, NULL, consumer, (void*)&buffer[NUM_BUFFERS - 1]);
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&add_thread[i], NULL, add_one, (void*)&transfer[0]);
        pthread_create(&multiple_thread[i], NULL, muliple_two, (void*)&transfer[1]);
    }
    
    // Register the timeout handler for SIGALRM signal
    signal(SIGALRM, stop_signal);
    alarm(10); // Set a timeout of 10 seconds
    // Wait for time out signal
    sem_init(&timeout, 0, 0);
    sem_wait(&timeout);
    // Cancel threads
    pthread_cancel(producer_thread);
    pthread_cancel(consumer_thread);
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_cancel(add_thread[i]);
        pthread_cancel(multiple_thread[i]);
    }
    
    // Wait for threads to finish
    pthread_join(producer_thread, NULL);
    pthread_join(consumer_thread, NULL);
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(add_thread[i], NULL);
        pthread_join(multiple_thread[i], NULL);
    }
    
    // Destroy buffer and semaphores
    for (int i = 0; i < NUM_BUFFERS; i++) {
        destroyBuffer(&buffer[i]);
    }

    return 0;
}
