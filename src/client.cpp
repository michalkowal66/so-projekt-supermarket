#include "shared.h"
#include <sys/stat.h>

sem_t* semaphore = nullptr;
SharedState* state = nullptr;
pid_t client_pid;

// Obsługa sygnału od strażaka
void handle_fire_signal(int signo) {
    sem_lock(semaphore);

    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (state->clients[i] == getpid()) {
            state->clients[i] = -1;
            break;
        }
    }

    sem_unlock(semaphore);

    std::cout << "Client " << client_pid << ": Evacuating supermarket!" << std::endl;

    exit(0);
}

int main() {
    client_pid = getpid();

    key_t key = ftok("manager.cpp", 65);
    int shmid = shmget(key, sizeof(SharedState), 0666);
    state = (SharedState*)shmat(shmid, nullptr, 0);
    semaphore = sem_open(SEM_NAME, 0);

    signal(SIGUSR1, handle_fire_signal);

    char fifoName[64];
    sprintf(fifoName, "/tmp/client_%d", getpid());
    mkfifo(fifoName, 0666);

    // Dodanie do listy klientów
    sem_lock(semaphore);

    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (state->clients[i] == -1) {
            state->clients[i] = getpid();
            break;
        }
    }

    // TODO: Sprawdź, czy udało się wejść do sklepu

    sem_unlock(semaphore);

    sleep(rand() % 10); // Losowy czas zakupów

    // TODO: Ustawienie się w kolejce, odczytanie FIFO, i zakonczenie zakupow

    unlink(fifoName);
    if (shmdt(state) == -1) {
        perror("shmdt");
    }

    return 0;
}
