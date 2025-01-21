#include "shared.h"

// Inicjalizacja semafora
sem_t* initialize_semaphore() {
    // Sprawdzenie, czy semafor istnieje i usunięcie go, jeśli tak
    // Otwarcie bez O_CREAT by sprawdzić istnienie semafora
    sem_t* sem = sem_open(SEM_NAME, 0); 
    if (sem != SEM_FAILED) {
        if (sem_close(sem) == -1) {
            perror("sem_close");
            std::cerr << "errno: " << errno << std::endl;
            return nullptr;
        }
        if (sem_unlink(SEM_NAME) == -1) {
            perror("sem_unlink");
            std::cerr << "errno: " << errno << std::endl;
            return nullptr;
        }
    } else if (errno != ENOENT) {
        perror("sem_open (check)");
        std::cerr << "errno: " << errno << std::endl;
        return nullptr;
    }

    // Utworzenie semafora
    sem = sem_open(SEM_NAME, O_CREAT, 0666, 1);
    if (sem == SEM_FAILED) {
        perror("sem_open (create)");
        std::cerr << "errno: " << errno << std::endl;
        return nullptr;
    }

    return sem;
}

// Usuwanie semafora
void cleanup_semaphore(sem_t* semaphore) {
    if (sem_close(semaphore) != 0) {
        perror("sem_close");
        std::cerr << "errno: " << errno << std::endl;
        exit(EXIT_FAILURE);
    }
    if (sem_unlink(SEM_NAME) != 0) {
        perror("sem_unlink");
        std::cerr << "errno: " << errno << std::endl;
        exit(EXIT_FAILURE);
    }
}

// Funkcje semaforów

// Wykonanie operacji WAIT na semaforze
void sem_lock(sem_t* semaphore) {
    if (sem_wait(semaphore) != 0) {
        perror("sem_wait");
        std::cerr << "errno: " << errno << std::endl;
        exit(EXIT_FAILURE);
    }
}

// Wykonanie operacji POST na semaforze
void sem_unlock(sem_t* semaphore) {
    if (sem_post(semaphore) != 0) {
        perror("sem_post");
        std::cerr << "errno: " << errno << std::endl;
        exit(EXIT_FAILURE);
    }
}

// Inicjalizacja pliku klucza dzielonego
int initialize_shared_key_file(const char* path) {
    int fd = open(path, O_CREAT | O_RDWR, 0644);
    if (fd == -1) {
        perror("open");
        std::cerr << "errno: " << errno << std::endl;
        exit(EXIT_FAILURE);
    }

    if (close(fd) == -1) {
        perror("close");
        std::cerr << "errno: " << errno << std::endl;
        exit(EXIT_FAILURE);
    }

    return 0;
}

// Inicjalizacja pamięci dzielonej
SharedState* initialize_shared_memory(key_t key, int& shmid) {
    shmid = shmget(key, sizeof(SharedState), IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget");
        std::cerr << "errno: " << errno << std::endl;
        return nullptr;
    }

    SharedState* state = (SharedState*)shmat(shmid, nullptr, 0);
    if (state == (void*)-1) {
        perror("shmat");
        std::cerr << "errno: " << errno << std::endl;
        return nullptr;
    }

    // Inicjalizacja stanu początkowego

    memset(state->checkout_statuses, CLOSED, sizeof(state->checkout_statuses));
    memset(state->cashiers, -1, sizeof(state->cashiers));
    memset(state->clients, -1, sizeof(state->clients));
    state->manager = getpid();

    // Inicjalizacja kolejek kas
    for (int i = 0; i < MAX_CHECKOUTS; ++i) {
        for (int j = 0; j < MAX_QUEUE; ++j) {
            state->queues[i][j] = -1; // Puste miejsce w kolejce
        }
    }
    
    state->evacuation = false;

    return state;
}

// Uzyskanie dostępu do pamięci dzielonej
SharedState* get_shared_memory() {
    key_t key = ftok(SHARED_KEY_FILE, 65);
    if (key == -1) {
        perror("ftok");
        std::cerr << "errno: " << errno << std::endl;
        return nullptr;
    }

    int shmid = shmget(key, sizeof(SharedState), 0666);
    if (shmid == -1) {
        perror("shmget");
        std::cerr << "errno: " << errno << std::endl;
        return nullptr;
    }

    SharedState *state = (SharedState*)shmat(shmid, nullptr, 0);
    if (state == (void*)-1) {
        perror("shmat");
        std::cerr << "errno: " << errno << std::endl;
        return nullptr;
    }

    return state;
}

// Czyszczenie pamięci dzielonej
void cleanup_shared_memory(int shmid, SharedState* state) {
    if (shmdt(state) == -1) {
        perror("shmdt");
        std::cerr << "errno: " << errno << std::endl;
    }
    if (shmctl(shmid, IPC_RMID, nullptr) == -1) {
        perror("shmctl");
        std::cerr << "errno: " << errno << std::endl;
    }
}

// Usuwanie pliku klucza dzielonego
int delete_file(const char* path) {
    // Sprawdzenie, czy plik istnieje
    if (access(path, F_OK) == -1) {
        perror("access");
        std::cerr << "errno: " << errno << std::endl;
        return -1;
    }

    // Próba usunięcia pliku
    if (unlink(path) == -1) {
        perror("unlink");
        std::cerr << "errno: " << errno << std::endl;
        return -1;
    }

    return 0;
}

// Funkcje pomocnicze

// Przekształcenie statusu kasy na łańcuch znaków
const char* checkout_status_to_string(CheckoutStatus status) {
    switch (status) {
        case 0:
            return "CLOSED";
        case 1:
            return "CLOSING";
        case 2:
            return "OPEN";
        case 3:
            return "OPENING";
        default:
            return "UNKNOWN";
    }
}
