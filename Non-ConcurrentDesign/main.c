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
// Functions
//

#define NUM_ITEMS 100

void* producer_consumer() {
    // time used
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_cpu);
    clock_gettime(CLOCK_REALTIME, &start_real);

    for (int i = 0; i <= NUM_ITEMS; i++) {
        if (i == NUM_ITEMS) {
            // Time used
            clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_cpu);
            cpu_time_used = (double)(end_cpu.tv_sec - start_cpu.tv_sec) +
                    (double)(end_cpu.tv_nsec - start_cpu.tv_nsec) / 1e9;
            printf("CPU time used: %f seconds\n", cpu_time_used);
            clock_gettime(CLOCK_REALTIME, &end_real);
            real_time_elapsed = (double)(end_real.tv_sec - start_real.tv_sec) +
                                (double)(end_real.tv_nsec - start_real.tv_nsec) / 1e9;
            printf("Real-time elapsed: %f seconds\n", real_time_elapsed);
            break;
        }
        else {
	    // Generate item
            int item = i;
	    //int item = rand() % 100;

	    // x = x + 1
            item += 1;
            sleep(0.1); // simulate complex caculation time

	    // x = x * 2
            item *= 2;
            sleep(0.1); // simulate complex caculation time


            printf(" Consumed item: %d\n", item);
        }
    }
}

//
// Main
//

int main() {
    producer_consumer();
    return 0;
}
