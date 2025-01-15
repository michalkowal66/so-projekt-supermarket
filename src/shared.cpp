#include "shared.h"

// Inicjalizacja semafora
sem_t* initialize_semaphore() {
    // Check if the semaphore exists
    sem_t* sem = sem_open(SEM_NAME, 0); // Open without O_CREAT to check existence
    if (sem != SEM_FAILED) {
        printf("Semaphore exists. Deleting it...\n");

        // Close and unlink the semaphore
        if (sem_close(sem) == -1) {
            perror("sem_close");
            exit(EXIT_FAILURE);
        }
        if (sem_unlink(SEM_NAME) == -1) {
            perror("sem_unlink");
            exit(EXIT_FAILURE);
        }

        printf("Semaphore deleted successfully.\n");
    } else if (errno != ENOENT) {
        perror("sem_open (check)");
        exit(EXIT_FAILURE);
    }

    // Create the semaphore
    printf("Creating semaphore...\n");
    sem = sem_open(SEM_NAME, O_CREAT, 0644, 1);
    if (sem == SEM_FAILED) {
        perror("sem_open (create)");
        exit(EXIT_FAILURE);
    }

    printf("Semaphore created successfully.\n");
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

int initialize_shared_key_file(const char* path) {
    int fd = open(path, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }
    close(fd);
    return 0;
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
    memset(state->checkout_statuses, CLOSED, sizeof(state->checkout_statuses));
    memset(state->cashiers, -1, sizeof(state->cashiers));
    state->manager = getpid();

    // Inicjalizacja kolejek kas
    for (int i = 0; i < MAX_CHECKOUTS; ++i) {
        for (int j = 0; j < MAX_QUEUE; ++j) {
            state->queues[i][j] = -1; // Puste miejsce w kolejce
        }
    }

    // Inicjalizacja listy klientów
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        state->clients[i] = -1; // Brak klientów w sklepie
    }
    
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
