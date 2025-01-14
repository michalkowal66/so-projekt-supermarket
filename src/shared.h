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

#define SEM_NAME "/supermarket_sem"
#define MAX_CHECKOUTS 10
#define MAX_QUEUE 20
#define MAX_CLIENTS 400

enum CheckoutStatus { CLOSED = 0, CLOSING = 1, OPEN = 2 };

// Struktura stanu współdzielonego
struct SharedState {
    char checkoutStatuses[MAX_CHECKOUTS]; // CLOSED, CLOSING, OPEN
    std::queue<int> queues[MAX_CHECKOUTS]; // Kolejki kas (10 kas, po 20 miejsc)
    int clients[MAX_CLIENTS]; // Lista klientów w sklepie (-1 oznacza brak klienta)
    bool evacuation; // Flaga ewakuacji
};

// Funkcje pomocnicze
sem_t* initialize_semaphore();
void cleanup_semaphore(sem_t* semaphore);
void sem_lock(sem_t* semaphore);
void sem_unlock(sem_t* semaphore);
SharedState* initialize_shared_memory(key_t key, int& shmid);
void cleanup_shared_memory(int shmid, SharedState* state);

#endif