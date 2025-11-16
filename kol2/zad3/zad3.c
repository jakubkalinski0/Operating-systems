#include<stdio.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<string.h>
#include<time.h>
#include<pthread.h>

/*

Zadanie 3 (2 warianty zadania:
		za 40% => 11 watkow oznaczone * (6 watkow increment[opisane w punkcie 2.1], 4 watki decrement[opisane w punkcie 2.2] i 1 watek t555 [opisany w punkcie 3])
		lub
	        za 30% => 10 watkow oznaczone ^ (6 watkow increment[opisane w punkcie 2.1], 4 watki decrement[opisane w punkcie 2.2] i tyle [bez punktu 3]):

Tresc zadania
Napisz program, który:

1) Stworzy 11* (10^) wątków.
2) Z tych 11* (10^) wątków 10 wykonuje 1000 iteracji petli:
	2.1) 6 wątków będzie incrementowało współdzielona zmienna globalną:
		2.1.1) Każdy z wątków incrementuje w pętli zmienną o 1 następnie odczekuje (zasypia) losową ilość czasu między (0.03s - 0.8 s) 
			ref (zobacz ponizej void* increment(void *arg))
	2.2) 4 wątki będą decrementowaly współdzielona zmienna globalną:
		2.2.1) Każdy z wątków decrementuje w pętli zmienną o 1 następnie odczekuje (zasypia) losową ilość czasu między (0.01s - 0.15 s)
			ref (zobacz ponizej void* decrement(void *arg))
3*) Ostatni (11) wątek oczekuje (spi) az warunek zostanie spelniony, taki ze 3 ostatnie cyfry zmiennej wspoldzielonej to 555 w wyniku tego wybudza sie i dodaje 100 do niej oraz wypisuje aktualna wartosc zmiennej wspodzielonej, oraz akcje dodania || ref (zobacz ponizej void* t555(void *arg))
4) Watek glowny czeka na wykonanie obliczen przez watki (increment oraz decrement) i zakonczenie pracy przez nie, nastepnie na koniec wypisuje wartosc koncowa zmiennej wspoldzielonej.


int shared_var = 0;

void* increment(void *arg)
{
  for (... < 1000){
     shared_var++;
     sleep, nanosleep, usleep <- (0.03s - 0.8 s)
  }

  return 0;
}

void* decrement(void *arg)
{
  for (... < 1000){
     shared_var--;
     sleep, nanosleep, usleep <- (0.01s - 0.15 s)
  }

  return 0;
}

void* t555(void *arg)
{
  oczekuje az shared_var % 1000 == 555
  wybudza sie i wypisuje 
  printf("T555 dodaje 100 do: %d\n", shared_var);
  shared_var += 100
  znowu oczekuje 
  return 0;
}

*/

int shared_var = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_555 = PTHREAD_COND_INITIALIZER;

int random_sleep_us(int min_ms, int max_ms) {
    return (rand() % (max_ms - min_ms + 1) + min_ms) * 1000;
}

void* increment(void *arg) {
    int thread_id = *(int*)arg;

    for (int i = 0; i < 1000; i++) {
        pthread_mutex_lock(&mutex);
        shared_var++;

        if (shared_var % 1000 == 555) {
            pthread_cond_signal(&cond_555);
        }

        pthread_mutex_unlock(&mutex);
        usleep(random_sleep_us(30, 800));
    }

    printf("Increment thread %d finished\n", thread_id);
    return NULL;
}

void* decrement(void *arg) {
    int thread_id = *(int*)arg;

    for (int i = 0; i < 1000; i++) {
        pthread_mutex_lock(&mutex);
        shared_var--;

        if (shared_var % 1000 == 555) {
            pthread_cond_signal(&cond_555);
        }

        pthread_mutex_unlock(&mutex);
        usleep(random_sleep_us(10, 150));
    }

    printf("Decrement thread %d finished\n", thread_id);
    return NULL;
}

void* t555(void *arg) {
    while (1) {
        pthread_mutex_lock(&mutex);

        while (shared_var % 1000 != 555) {
            pthread_cond_wait(&cond_555, &mutex);
        }

        printf("T555 adds 100 to: %d\n", shared_var);
        shared_var += 100;
        printf("T555 new value: %d\n", shared_var);

        pthread_mutex_unlock(&mutex);
        usleep(1000);
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    srand(time(NULL));

    pthread_t threads[11];
    int thread_ids[10];

    printf("Initial value of the variable: %d\n", shared_var);

    for (int i = 0; i < 6; i++) {
        thread_ids[i] = i + 1;
        if (pthread_create(&threads[i], NULL, increment, &thread_ids[i]) != 0) {
            perror("Failed to create increment thread");
            return 1;
        }
    }

    for (int i = 0; i < 4; i++) {
        thread_ids[i + 6] = i + 7;
        if (pthread_create(&threads[i + 6], NULL, decrement, &thread_ids[i + 6]) != 0) {
            perror("Failed to create decrement thread");
            return 1;
        }
    }

    if (pthread_create(&threads[10], NULL, t555, NULL) != 0) {
        perror("Failed to create t555 thread");
        return 1;
    }

    for (int i = 0; i < 10; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            perror("Failed to join thread");
            return 1;
        }
    }

    pthread_cancel(threads[10]);
    pthread_join(threads[10], NULL);
    printf("Final value of the variable: %d\n", shared_var);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond_555);

    return 0;
}