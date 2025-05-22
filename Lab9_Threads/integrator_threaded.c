#define _USE_MATH_DEFINES // For MSVC - although likely not needed for GCC on Linux
#define _GNU_SOURCE       // Crucial for GCC/Clang on Linux to ensure M_PI is defined
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h> // For strerror
#include <errno.h>  // For errno
#include <math.h>   // M_PI should now be available

// Definition of the function to integrate: 4 / (x^2 + 1)
double func(double x) {
    return 4.0 / (1.0 + x * x);
}

// Structure to pass data to the thread
typedef struct {
    int thread_id;              // Thread identifier (for informational/debugging purposes)
    double dx;                  // Width of the rectangle
    long long start_rect_idx;   // Starting rectangle index for this thread
    long long num_rects;        // Number of rectangles to calculate by this thread
    double *partial_sum_ptr;    // Pointer to the location in the array to store the partial result
} thread_data_t;

// Function executed by each worker thread
void *calculate_partial_integral(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    double local_sum = 0.0;
    double x;

    // We use the left edge of the rectangle to calculate the height
    // x_i = i * dx
    for (long long i = 0; i < data->num_rects; ++i) {
        x = (data->start_rect_idx + i) * data->dx;
        local_sum += func(x) * data->dx;
    }

    *(data->partial_sum_ptr) = local_sum;
    // printf("Thread %d finished, partial sum: %f\n", data->thread_id, local_sum);
    return NULL; // Thread finishes, result written via pointer
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <rectangle_width> <num_threads>\n", argv[0]);
        fprintf(stderr, "Example: %s 0.0000001 4\n", argv[0]);
        return 1;
    }

    char *endptr_dx;
    double dx = strtod(argv[1], &endptr_dx);
    if (argv[1] == endptr_dx || *endptr_dx != '\0' || dx <= 0) {
        fprintf(stderr, "Error: Invalid rectangle width '%s'. Must be a positive number.\n", argv[1]);
        return 1;
    }

    char *endptr_k;
    long k_long = strtol(argv[2], &endptr_k, 10);
    if (argv[2] == endptr_k || *endptr_k != '\0' || k_long <= 0 || k_long > 1024 /* reasonable limit */) {
        fprintf(stderr, "Error: Invalid number of threads '%s'. Must be a positive integer.\n", argv[2]);
        return 1;
    }
    int num_threads = (int)k_long;

    // Calculate the total number of rectangles in the interval [0, 1]
    // Round down to avoid exceeding the interval
    long long total_rectangles = (long long)(1.0 / dx);
    if (total_rectangles == 0 && 1.0/dx > 0) { // If dx is very large, but 1/dx > 0
        total_rectangles = 1;
    } else if (total_rectangles <= 0) {
        fprintf(stderr, "Error: Rectangle width dx is too large, resulting in %lld rectangles. Please decrease dx.\n", total_rectangles);
        return 1;
    }

    printf("Rectangle width (dx): %g\n", dx);
    printf("Number of threads: %d\n", num_threads);
    printf("Total rectangles to calculate: %lld\n", total_rectangles);
    printf("Expected result (PI): %.15f\n", M_PI); // M_PI should now be recognized


    // Resource allocation
    pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
    if (!threads) {
        perror("Error allocating memory for thread identifiers");
        return 2;
    }

    thread_data_t *thread_args = malloc(num_threads * sizeof(thread_data_t));
    if (!thread_args) {
        perror("Error allocating memory for thread arguments");
        free(threads);
        return 2;
    }

    double *partial_sums = malloc(num_threads * sizeof(double));
    if (!partial_sums) {
        perror("Error allocating memory for partial sums");
        free(threads);
        free(thread_args);
        return 2;
    }

    // Distribute work among threads
    long long base_rects_per_thread = total_rectangles / num_threads;
    long long remainder_rects = total_rectangles % num_threads;
    long long current_start_rect_idx = 0;

    for (int i = 0; i < num_threads; ++i) {
        thread_args[i].thread_id = i;
        thread_args[i].dx = dx;
        thread_args[i].partial_sum_ptr = &partial_sums[i]; // Pointer to the array element

        thread_args[i].start_rect_idx = current_start_rect_idx;
        thread_args[i].num_rects = base_rects_per_thread;
        if (i < remainder_rects) { // The first 'remainder_rects' threads get one extra rectangle
            thread_args[i].num_rects++;
        }
        current_start_rect_idx += thread_args[i].num_rects;

        // printf("Thread %d: start_idx=%lld, num_rects=%lld\n", i, thread_args[i].start_rect_idx, thread_args[i].num_rects);

        int ret = pthread_create(&threads[i], NULL, calculate_partial_integral, &thread_args[i]);
        if (ret != 0) {
            fprintf(stderr, "Error creating thread %d: %s\n", i, strerror(ret));
            // Simple cleanup in case of error - one could try to cancel already created threads
            for (int j = 0; j < i; ++j) {
                pthread_cancel(threads[j]); // Request cancellation
                pthread_join(threads[j], NULL); // Wait for termination
            }
            free(threads);
            free(thread_args);
            free(partial_sums);
            return 3;
        }
    }

    // Wait for all threads to complete
    for (int i = 0; i < num_threads; ++i) {
        int ret = pthread_join(threads[i], NULL); // Second argument is NULL as the result is passed via pointer
        if (ret != 0) {
            fprintf(stderr, "Error joining thread %d: %s\n", i, strerror(ret));
            // Continue to try and collect results from other threads
        }
    }

    // Sum up the partial results
    double total_integral = 0.0;
    for (int i = 0; i < num_threads; ++i) {
        total_integral += partial_sums[i];
    }

    printf("Calculated integral value: %.15f\n", total_integral);
    printf("Difference from PI: %.15f\n", fabs(total_integral - M_PI)); // M_PI should now be recognized


    // Free allocated memory
    free(threads);
    free(thread_args);
    free(partial_sums);

    return 0;
}