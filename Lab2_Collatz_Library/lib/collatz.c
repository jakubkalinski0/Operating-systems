int collatz_conjecture(int input) {
    if (input % 2 == 0) {
        return input/2;
    }
    else {
        return input*3 + 1;
    }
}