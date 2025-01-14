#include "shared.h"

// Inicjalizacja semafora
sem_t* initialize_semaphore() {
    sem_t* sem = sem_open(SEM_NAME, O_CREAT | O_EXCL, 0644, 1);
    if (sem == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }
    return sem;
}

// Usuwanie semafora
void cleanup_semaphore(sem_t* semaphore) {
    if (sem_close(semaphore) != 0) {
        perror("sem_close");
    }
    if (sem_unlink(SEM_NAME) != 0) {
        perror("sem_unlink");
    }
}

// Funkcje semaforów
void sem_lock(sem_t* semaphore) {
    if (sem_wait(semaphore) != 0) {
        perror("sem_wait");
        exit(EXIT_FAILURE);
    }
}

void sem_unlock(sem_t* semaphore) {
    if (sem_post(semaphore) != 0) {
        perror("sem_post");
        exit(EXIT_FAILURE);
    }
}

// Inicjalizacja pamięci dzielonej
SharedState* initialize_shared_memory(key_t key, int& shmid) {
    shmid = shmget(key, sizeof(SharedState), IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    SharedState* state = (SharedState*)shmat(shmid, nullptr, 0);
    if (state == (void*)-1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    // Inicjalizacja stanu początkowego
    memset(state->checkoutStatuses, CLOSED, sizeof(state->checkoutStatuses));
    memset(state->clients, -1, sizeof(state->clients));
    state->evacuation = false;

    return state;
}

// Czyszczenie pamięci dzielonej
void cleanup_shared_memory(int shmid, SharedState* state) {
    if (shmdt(state) == -1) {
        perror("shmdt");
    }
    if (shmctl(shmid, IPC_RMID, nullptr) == -1) {
        perror("shmctl");
    }
}
