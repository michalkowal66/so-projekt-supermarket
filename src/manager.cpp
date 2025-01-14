#include "shared.h"
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <cstring>

// Globalne wskaźniki
sem_t* semaphore = nullptr;
SharedState* state = nullptr;
int shmid;

// Obsługa sygnałów
void handle_fire_signal(int signo) {
    sem_lock(semaphore);

    state->evacuation = true;
    
    sem_unlock(semaphore);
    std::cout << "Manager: Evacuation triggered! Closing all checkouts." << std::endl;

    // TODO: zaczekaj aż ewakuacja się zakończy
}

void handle_child_signal(int signo) {
    // TODO: upewnij się, że kasa, została zamknięta
}

int main() {
    signal(SIGUSR1, handle_fire_signal);
    signal(SIGCHLD, handle_child_signal);

    // Inicjalizacja pamięci współdzielonej i semaforów
    key_t key = ftok("manager.cpp", 65);
    state = initialize_shared_memory(key, shmid);
    semaphore = initialize_semaphore();

    // Startowe otwarcie kas
    sem_lock(semaphore);

    // TODO: Utwórz procesy kasjera

    sem_unlock(semaphore);

    while (true) {
        sleep(5); // Okresowa kontrola stanu sklepu

        sem_lock(semaphore);
        if (state->evacuation) {
            sem_unlock(semaphore);
            break;
        }

        // TODO: Logika otwierania kas

        sem_unlock(semaphore);
    }

    // TODO: Zaczekaj na flage zakonczenia ewakuacji (?)

    cleanup_shared_memory(shmid, state);
    cleanup_semaphore(semaphore);

    return 0;
}