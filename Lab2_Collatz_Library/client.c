#include <stdio.h>
#include <stdlib.h>

// For dynamically loaded library
#ifdef DYNAMIC_LOAD
#include <dlfcn.h>
#else
// For static and shared libraries
#include "lib/collatz.h"
#endif

int main() {
    int input, max_iter;

    // User input
    printf("Enter a starting number: ");
    if (scanf("%d", &input) != 1 || input <= 0) {
        fprintf(stderr, "Invalid input. Please enter a positive integer.\n");
        return 1;
    }

    printf("Enter the maximum number of iterations: ");
    if (scanf("%d", &max_iter) != 1 || max_iter <= 0) {
        fprintf(stderr, "Invalid input. Please enter a positive integer.\n");
        return 1;
    }

    int *steps = malloc(max_iter * sizeof(int));

    if (steps == NULL) {
        fprintf(stderr, "Memory allocation error\n");
        return 1;
    }

#ifdef DYNAMIC_LOAD
    // Code for dynamically loaded library
    int (*collatz_conjecture)(int input);
    int (*test_collatz_convergence)(int input, int max_iter, int *steps);

    void* dll_handle = dlopen("./lib/libcollatz_shared.so", RTLD_LAZY);
    if (!dll_handle) {
        fprintf(stderr, "Error: %s\n", dlerror());
        free(steps);
        return 1;
    }

    collatz_conjecture = dlsym(dll_handle, "collatz_conjecture");
    if (!collatz_conjecture) {
        fprintf(stderr, "Error loading collatz_conjecture: %s\n", dlerror());
        dlclose(dll_handle);
        free(steps);
        return 1;
    }

    test_collatz_convergence = dlsym(dll_handle, "test_collatz_convergence");
    if (!test_collatz_convergence) {
        fprintf(stderr, "Error loading test_collatz_convergence: %s\n", dlerror());
        dlclose(dll_handle);
        free(steps);
        return 1;
    }
#endif

    int count;
#ifdef DYNAMIC_LOAD
    count = test_collatz_convergence(input, max_iter, steps);
#else
    // Code for static and shared libraries
    count = test_collatz_convergence(input, max_iter, steps);
#endif

    printf("Value after one step for %d:\n%d\n", input, collatz_conjecture(input));
    if (count == 0) {
        printf("Failed to reach 1 within the given number of iterations.\n");
    } else {
        printf("Sequence for %d:\n", input);
        for (int i = 0; i < count; i++) {
            printf("%d ", steps[i]);
        }
        printf("\n");
    }

    free(steps);

#ifdef DYNAMIC_LOAD
    dlclose(dll_handle);
#endif

    return 0;
}