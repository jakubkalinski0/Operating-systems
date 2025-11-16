#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <string.h> // For memset

// --- Konfiguracja ---
#define MAX_PATIENTS_IN_WAITING_ROOM 3
#define PATIENTS_PER_CONSULTATION 3
#define MEDICINE_PER_CONSULTATION 3
#define MAX_MEDICINE_CAPACITY 6
#define MAX_ID_BUFFER_SIZE 1024 // Dla listy ID pacjentów w komunikacie lekarza

// --- Zmienne globalne (chronione mutexem) ---
pthread_mutex_t hospital_mutex;

pthread_cond_t doctor_cond;                   // Lekarz na niej czeka
pthread_cond_t patient_can_enter_cond;        // Pacjenci czekają na miejsce w poczekalni
pthread_cond_t pharmacist_can_deliver_cond;   // Farmaceuci czekają na miejsce w apteczce
pthread_cond_t consultation_slots_filled_cond; // Lekarz czeka aż 3 pacjentów się zbierze i zasygnalizuje gotowość
pthread_cond_t* patient_consultation_finished_cond; // Tablica CV, każdy pacjent czeka na swoją
pthread_cond_t pharmacist_delivery_accepted_cond; // Farmaceuta czeka aż lekarz przyjmie dostawę

int patients_in_waiting_room = 0;
int patient_ids_for_current_consultation[PATIENTS_PER_CONSULTATION];
int medicine_count = 0; // Apteczka startuje pusta

int total_patients_arg = 0;
int patients_served_count = 0;

bool pharmacist_wants_to_deliver = false; // Czy jakiś farmaceuta aktywnie chce dostarczyć
int delivering_pharmacist_id = -1;      // ID farmaceuty, który ma być obsłużony

// Funkcja pomocnicza do uzyskiwania czasu
char* get_time_str() {
    static char buffer[30];
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    return buffer;
}

// Funkcja pomocnicza do losowania czasu
int random_sleep_time(int min_sec, int max_sec) {
    return (rand() % (max_sec - min_sec + 1) + min_sec);
}

// --- Zachowania Lekarza ---
void* doctor_thread_func(void* arg) {
    (void)arg; // Informujemy kompilator, że argument jest celowo nieużywany
    printf("[%s] - Lekarz: Rozpoczynam dyżur.\n", get_time_str());

    pthread_mutex_lock(&hospital_mutex);
    while (patients_served_count < total_patients_arg) {
        // Warunek spania:
        // 1. Nie ma 3 pacjentów GOTOWYCH do konsultacji LUB nie ma wystarczająco leków
        // ORAZ
        // 2. Żaden farmaceuta nie chce dostarczyć LUB apteczka jest pełna LUB (jest >3 leków, więc nie jest krytycznie)
        while (!((patients_in_waiting_room == PATIENTS_PER_CONSULTATION && medicine_count >= MEDICINE_PER_CONSULTATION) ||
                 (pharmacist_wants_to_deliver && medicine_count < MAX_MEDICINE_CAPACITY && medicine_count < MEDICINE_PER_CONSULTATION))) {
            printf("[%s] - Lekarz: Zasypiam. Pacjentów w poczekalni: %d, Leków: %d.\n", get_time_str(), patients_in_waiting_room, medicine_count);
            pthread_cond_wait(&doctor_cond, &hospital_mutex);
            printf("[%s] - Lekarz: Zostałem obudzony sygnałem.\n", get_time_str());
        }
        printf("[%s] - Lekarz: Budzę się.\n", get_time_str());

        // Obsługa pacjentów
        if (patients_in_waiting_room == PATIENTS_PER_CONSULTATION && medicine_count >= MEDICINE_PER_CONSULTATION) {
            char patient_ids_str[MAX_ID_BUFFER_SIZE] = "";
            for (int i = 0; i < PATIENTS_PER_CONSULTATION; ++i) {
                char temp[10];
                sprintf(temp, "%d", patient_ids_for_current_consultation[i]);
                strcat(patient_ids_str, temp);
                if (i < PATIENTS_PER_CONSULTATION - 1) {
                    strcat(patient_ids_str, ", ");
                }
            }
            printf("[%s] - Lekarz: Konsultuję pacjentów %s.\n", get_time_str(), patient_ids_str);

            // Symulacja konsultacji (mutex odblokowany na czas sleep)
            pthread_mutex_unlock(&hospital_mutex);
            sleep(random_sleep_time(2, 4)); // Konsultacja trwa 2-4s
            pthread_mutex_lock(&hospital_mutex);

            medicine_count -= MEDICINE_PER_CONSULTATION;
            patients_served_count += PATIENTS_PER_CONSULTATION;

            printf("[%s] - Lekarz: Zakończono konsultację. Leków: %d. Obsłużonych: %d/%d\n", get_time_str(), medicine_count, patients_served_count, total_patients_arg);

            // Obudź skonsultowanych pacjentów
            for (int i = 0; i < PATIENTS_PER_CONSULTATION; ++i) {
                pthread_cond_signal(&patient_consultation_finished_cond[patient_ids_for_current_consultation[i]]);
            }
            // Resetuj listę pacjentów do konsultacji i liczbę w poczekalni
            memset(patient_ids_for_current_consultation, 0, sizeof(patient_ids_for_current_consultation));
            patients_in_waiting_room = 0;


            // Sygnalizuj, że zwolniły się miejsca w poczekalni
            pthread_cond_broadcast(&patient_can_enter_cond);

            // Jeśli mało leków, daj znać farmaceutom, że mogą chcieć dostarczyć
            if (medicine_count < MEDICINE_PER_CONSULTATION) {
                pthread_cond_broadcast(&pharmacist_can_deliver_cond); // Farmaceuci mogli czekać na zużycie
            }

        }
        // Obsługa farmaceuty
        else if (pharmacist_wants_to_deliver && medicine_count < MAX_MEDICINE_CAPACITY) {
            // int id_pharmacist_to_serve = delivering_pharmacist_id; // Można zapamiętać, ale nie jest to krytyczne z nową logiką farmaceuty
            printf("[%s] - Lekarz: Przyjmuję dostawę leków od Farmaceuty(%d).\n", get_time_str(), delivering_pharmacist_id);

            pthread_mutex_unlock(&hospital_mutex);
            sleep(random_sleep_time(1, 3));
            pthread_mutex_lock(&hospital_mutex);

            medicine_count = MAX_MEDICINE_CAPACITY;
            pharmacist_wants_to_deliver = false;
            delivering_pharmacist_id = -1;

            printf("[%s] - Lekarz: Dostawa zakończona. Leków: %d.\n", get_time_str(), medicine_count);
            pthread_cond_signal(&pharmacist_delivery_accepted_cond);
            pthread_cond_broadcast(&pharmacist_can_deliver_cond); // << --- DODANA LINIA: Obudź innych farmaceutów
        }
    }
    pthread_mutex_unlock(&hospital_mutex);
    printf("[%s] - Lekarz: Kończę dyżur, wszyscy pacjenci obsłużeni.\n", get_time_str());
    return NULL;
}

// --- Zachowania Pacjentów ---
void* patient_thread_func(void* arg) {
    int patient_id = *(int*)arg;
    free(arg); // Pamięć zaalokowana w main

    int arrival_time = random_sleep_time(2, 5);
    printf("[%s] - Pacjent(%d): Idę do szpitala, będę za %d s.\n", get_time_str(), patient_id, arrival_time);
    sleep(arrival_time);

    pthread_mutex_lock(&hospital_mutex);
    while (true) { // Pętla na wypadek, gdyby musiał poczekać na miejsce i spróbować ponownie
        if (patients_in_waiting_room >= MAX_PATIENTS_IN_WAITING_ROOM) {
            int walk_time = random_sleep_time(1, 3); // Krótki spacer
            printf("[%s] - Pacjent(%d): Za dużo pacjentów (%d), wracam później za %d s.\n", get_time_str(), patient_id, patients_in_waiting_room, walk_time);
            pthread_cond_wait(&patient_can_enter_cond, &hospital_mutex);
            // Po obudzeniu, pętla zacznie się od nowa, sprawdzając warunek
            continue;
        } else {
            // Jest miejsce, pacjent wchodzi
            patient_ids_for_current_consultation[patients_in_waiting_room] = patient_id;
            patients_in_waiting_room++;
            printf("[%s] - Pacjent(%d): Wszedłem. Czeka %d pacjentów na lekarza.\n", get_time_str(), patient_id, patients_in_waiting_room);

            if (patients_in_waiting_room == PATIENTS_PER_CONSULTATION) {
                printf("[%s] - Pacjent(%d): Jestem trzeci, budzę lekarza.\n", get_time_str(), patient_id);
                pthread_cond_signal(&doctor_cond); // Obudź lekarza
            }

            // Pacjent czeka na zakończenie swojej konsultacji
            pthread_cond_wait(&patient_consultation_finished_cond[patient_id], &hospital_mutex);
            printf("[%s] - Pacjent(%d): Kończę wizytę.\n", get_time_str(), patient_id);
            break; // Pacjent obsłużony, wychodzi z pętli i kończy wątek
        }
    }
    pthread_mutex_unlock(&hospital_mutex);
    return NULL;
}


// --- Zachowania Farmaceutów ---
void* pharmacist_thread_func(void* arg) {
    int pharmacist_id = *(int*)arg;
    free(arg);

    int arrival_time = random_sleep_time(5, 15);
    printf("[%s] - Farmaceuta(%d): Idę do szpitala z dostawą, będę za %d s.\n", get_time_str(), pharmacist_id, arrival_time);
    sleep(arrival_time);

    pthread_mutex_lock(&hospital_mutex);

    // Czekaj, jeśli apteczka jest pełna LUB inny farmaceuta już się zgłosił
    while (medicine_count == MAX_MEDICINE_CAPACITY || pharmacist_wants_to_deliver) {
        if (medicine_count == MAX_MEDICINE_CAPACITY) {
            printf("[%s] - Farmaceuta(%d): Apteczka pełna (%d/%d). Czekam.\n", get_time_str(), pharmacist_id, medicine_count, MAX_MEDICINE_CAPACITY);
        } else { // pharmacist_wants_to_deliver jest true, więc inny farmaceuta jest zgłoszony
            printf("[%s] - Farmaceuta(%d): Inny farmaceuta (%d) jest już zgłoszony. Czekam na swoją kolej.\n", get_time_str(), pharmacist_id, delivering_pharmacist_id);
        }
        pthread_cond_wait(&pharmacist_can_deliver_cond, &hospital_mutex);
    }

    // Teraz: medicine_count < MAX_MEDICINE_CAPACITY ORAZ pharmacist_wants_to_deliver == false
    // Możemy się zgłosić.
    delivering_pharmacist_id = pharmacist_id;
    pharmacist_wants_to_deliver = true;

    printf("[%s] - Farmaceuta(%d): Chcę dostarczyć leki. Stan apteczki: %d/%d.\n", get_time_str(), pharmacist_id, medicine_count, MAX_MEDICINE_CAPACITY);

    if (medicine_count < MEDICINE_PER_CONSULTATION) {
        printf("[%s] - Farmaceuta(%d): Budzę lekarza (mało leków).\n", get_time_str(), pharmacist_id);
        pthread_cond_signal(&doctor_cond);
    } else if (medicine_count < MAX_MEDICINE_CAPACITY &&
               !(patients_in_waiting_room == PATIENTS_PER_CONSULTATION && medicine_count >= MEDICINE_PER_CONSULTATION)) {
        // Dodatkowe budzenie: jeśli lekarz nie ma co robić z pacjentami, ale jest miejsce w apteczce
        printf("[%s] - Farmaceuta(%d): Budzę lekarza (może przyjąć dostawę, a nie pacjentów).\n", get_time_str(), pharmacist_id);
        pthread_cond_signal(&doctor_cond);
    }


    // Czekaj, aż lekarz przyjmie dostawę (konkretnie naszą, bo poprzednia pętla serializuje zgłoszenia)
    pthread_cond_wait(&pharmacist_delivery_accepted_cond, &hospital_mutex);
    // Po tym wait, lekarz już zresetował pharmacist_wants_to_deliver i delivering_pharmacist_id
    // i zasygnalizował pharmacist_delivery_accepted_cond
    printf("[%s] - Farmaceuta(%d): Zakończyłem dostawę.\n", get_time_str(), pharmacist_id);

    pthread_mutex_unlock(&hospital_mutex);
    return NULL;
}


int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Sposób użycia: %s <liczba_pacjentow> <liczba_farmaceutow>\n", argv[0]);
        return 1;
    }

    total_patients_arg = atoi(argv[1]);
    int num_pharmacists = atoi(argv[2]);

    if (total_patients_arg <= 0 || num_pharmacists <= 0) {
        fprintf(stderr, "Liczba pacjentów i farmaceutów musi być dodatnia.\n");
        return 1;
    }
     if (total_patients_arg % PATIENTS_PER_CONSULTATION != 0) {
        fprintf(stderr, "Ostrzeżenie: Liczba pacjentów (%d) nie jest wielokrotnością %d. Ostatni niepełna grupa może nie zostać obsłużona.\n", total_patients_arg, PATIENTS_PER_CONSULTATION);
    }


    srand(time(NULL));

    // Inicjalizacja mutexu i zmiennych warunkowych
    pthread_mutex_init(&hospital_mutex, NULL);
    pthread_cond_init(&doctor_cond, NULL);
    pthread_cond_init(&patient_can_enter_cond, NULL);
    pthread_cond_init(&pharmacist_can_deliver_cond, NULL);
    pthread_cond_init(&consultation_slots_filled_cond, NULL);
    pthread_cond_init(&pharmacist_delivery_accepted_cond, NULL);

    patient_consultation_finished_cond = malloc(total_patients_arg * sizeof(pthread_cond_t));
    if (patient_consultation_finished_cond == NULL) {
        perror("malloc dla patient_consultation_finished_cond");
        return 1;
    }
    for (int i = 0; i < total_patients_arg; ++i) {
        pthread_cond_init(&patient_consultation_finished_cond[i], NULL);
    }


    pthread_t doctor_tid;
    pthread_t patient_tids[total_patients_arg];
    pthread_t pharmacist_tids[num_pharmacists];

    // Tworzenie wątku lekarza
    pthread_create(&doctor_tid, NULL, doctor_thread_func, NULL);

    // Tworzenie wątków pacjentów
    for (int i = 0; i < total_patients_arg; ++i) {
        int* patient_id = malloc(sizeof(int));
        *patient_id = i; // Unikalne ID pacjenta
        pthread_create(&patient_tids[i], NULL, patient_thread_func, patient_id);
    }

    // Tworzenie wątków farmaceutów
    for (int i = 0; i < num_pharmacists; ++i) {
        int* pharmacist_id = malloc(sizeof(int));
        *pharmacist_id = i; // Unikalne ID farmaceuty
        pthread_create(&pharmacist_tids[i], NULL, pharmacist_thread_func, pharmacist_id);
    }

    // Oczekiwanie na zakończenie wątków
    pthread_join(doctor_tid, NULL);
    printf("[%s] - Główny: Lekarz zakończył pracę.\n", get_time_str());

    pthread_mutex_lock(&hospital_mutex);
    for(int i=0; i < total_patients_arg; ++i) {
        pthread_cond_signal(&patient_consultation_finished_cond[i]);
    }
    pthread_mutex_unlock(&hospital_mutex);


    for (int i = 0; i < total_patients_arg; ++i) {
        pthread_join(patient_tids[i], NULL);
    }
    printf("[%s] - Główny: Wszyscy pacjenci zakończyli.\n", get_time_str());

    pthread_mutex_lock(&hospital_mutex);
    pthread_cond_broadcast(&pharmacist_can_deliver_cond);
    pthread_cond_signal(&pharmacist_delivery_accepted_cond);
    pthread_mutex_unlock(&hospital_mutex);


    for (int i = 0; i < num_pharmacists; ++i) {
        pthread_join(pharmacist_tids[i], NULL);
    }
    printf("[%s] - Główny: Wszyscy farmaceuci zakończyli.\n", get_time_str());


    // Niszczenie mutexu i zmiennych warunkowych
    pthread_mutex_destroy(&hospital_mutex);
    pthread_cond_destroy(&doctor_cond);
    pthread_cond_destroy(&patient_can_enter_cond);
    pthread_cond_destroy(&pharmacist_can_deliver_cond);
    pthread_cond_destroy(&consultation_slots_filled_cond);
    pthread_cond_destroy(&pharmacist_delivery_accepted_cond);

    for (int i = 0; i < total_patients_arg; ++i) {
        pthread_cond_destroy(&patient_consultation_finished_cond[i]);
    }
    free(patient_consultation_finished_cond);

    printf("[%s] - Główny: Sprzątanie zakończone. Koniec programu.\n", get_time_str());
    return 0;
}