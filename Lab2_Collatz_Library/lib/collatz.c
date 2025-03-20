#include "collatz.h"

int collatz_conjecture(int input) {
    if (input % 2 == 0) {
        return input/2;
    }
    else {
        return input*3 + 1;
    }
}

int test_collatz_convergence(int input, int max_iter, int *steps) {
    int counter = 0;
    while (input != 1 && counter < max_iter) {
        steps[counter] = input;
        input = collatz_conjecture(input);
        counter++;
    }
    if (input == 1 && counter < max_iter) {
        steps[counter] = input;
        counter++;
        return counter;
    }
    else {
      return 0;
    }
}