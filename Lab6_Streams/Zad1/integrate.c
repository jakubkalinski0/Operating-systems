#include <printf.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>

double range_left = 0.0;
double range_right = 1.0; // przedzialy

double f(double x){
    return 4/(x*x + 1); // funkcja do calkowania
}

double calculate_integral(double interval_start, double interval_stop, double (*fun)(double), double interval_width, unsigned long long number_of_intervals){
    if(interval_stop - interval_start < interval_width)
        return fun((interval_start + interval_stop)/ 2.0)*(interval_stop - interval_start);

    double sum = 0.0;
    for(unsigned long long i = 0; i < number_of_intervals; i++){
        sum += fun((interval_start + interval_width/2.0));
        interval_start += interval_width;
    }

    return sum * interval_width;
}
// calkowanie metoda prostokatow

int main(int argc, char** argv) {
    if(argc < 3) { return -1;  } // zla liczba argumentow

    double interval_width = strtod(argv[1], NULL);
    //pobieramy dlugosc przedzialu

    long num_of_processes = strtol(argv[2], NULL, 10);
    //pobieramy liste procesow

    unsigned long long total_intervals_count = (unsigned long long)ceil((double)(range_right - range_left)/interval_width);
    // liczba podprzedzialow
    unsigned long long intervals_per_process = total_intervals_count/num_of_processes;
    // ile procesow zostanie przypisane do pojedynczego przedzialu

    double interval_start = range_left;
    double interval_stop = range_left;

    int pipes_fd[num_of_processes][2]; // przechowywanie deskryptorow do potokow, reading/writting

    for(int i = 0; i < num_of_processes; i++){
        // sprawdzenie poprawnosci konca zakresu
        if( range_right < interval_start + intervals_per_process*interval_width){
            interval_stop = range_right;
        } else {
            interval_stop = interval_start + intervals_per_process*interval_width;
        }

        pipe(pipes_fd[i]); // wywolanie potoku i umieszczenie deskyrptorow w tablicy
        pid_t pid = fork(); // nowy proices
        if(pid == 0){
            close(pipes_fd[i][0]); // zamyka deskryptor pliku końca do odczytu potoku
            double integral_result = calculate_integral(interval_start, interval_stop, f, interval_width, intervals_per_process);
            write(pipes_fd[i][1], &integral_result, sizeof(integral_result)); // proces potomny zapisuje zawartość zmiennej

            exit(0); // zakonczenie potomnego
        }
        close(pipes_fd[i][1]); // zamyka deskryptor pliku końca do zapisu potoku przez rodzic

        interval_start = interval_stop;
    }

    double sum = 0.0;
    for(int i = 0; i < num_of_processes; i++){
        double integral_result;
        read(pipes_fd[i][0], &integral_result, sizeof(integral_result)); // proces rodzicielski odczytuje dane z końca do odczytu potoku  utworzonego dla i-tego procesu potomnego
        sum += integral_result;
    }

    printf("total %lf\n", sum);

    return 0;
}