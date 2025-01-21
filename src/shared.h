#ifndef SHARED_H
#define SHARED_H

#include <signal.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <queue>
#include <errno.h>

#define SHARED_KEY_FILE "/tmp/supermarket_key"
#define SEM_NAME "/supermarket_sem"
#define K_CLIENTS 5
#define MAX_CHECKOUTS 10
#define MIN_CHECKOUTS 2
#define MAX_QUEUE 10
#define MAX_CLIENTS 300

enum CheckoutStatus { CLOSED = 0, CLOSING = 1, OPEN = 2, OPENING = 3 };

// Struktura stanu współdzielonego
struct SharedState {
    int manager;
    int cashiers[MAX_CHECKOUTS];
    CheckoutStatus checkout_statuses[MAX_CHECKOUTS]; // CLOSED, CLOSING, OPEN
    int queues[MAX_CHECKOUTS][MAX_QUEUE]; // Kolejki kas
    int clients[MAX_CLIENTS]; // Lista klientów w sklepie (-1 oznacza brak klienta)
    bool evacuation; // Flaga ewakuacji
};

// Funkcje pomocnicze
sem_t* initialize_semaphore();
void cleanup_semaphore(sem_t* semaphore);
void sem_lock(sem_t* semaphore);
void sem_unlock(sem_t* semaphore);
int initialize_shared_key_file(const char* path);
SharedState* initialize_shared_memory(key_t key, int& shmid);
void cleanup_shared_memory(int shmid, SharedState* state);
int delete_file(const char* path);
SharedState* get_shared_memory();
const char* checkout_status_to_string(CheckoutStatus status);

#endif