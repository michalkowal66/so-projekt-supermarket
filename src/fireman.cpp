#include "shared.h"

int main() {
    key_t key = ftok("manager.cpp", 65);
    int shmid = shmget(key, sizeof(SharedState), 0666);
    SharedState* state = (SharedState*)shmat(shmid, nullptr, 0);

    sem_t* semaphore = sem_open(SEM_NAME, 0);

    while (true) {
        sleep(10); // Okresowe sprawdzanie
        if (rand() % 100 > 80) { // Losowa szansa na pojawienie się pożaru w momencie sprawdzenia
            sem_lock(semaphore);

            state->evacuation = true;

            sem_unlock(semaphore);

            // TODO: Przesłanie sygnału o pożarze

            break;
        }
    }

    if (shmdt(state) == -1) {
        perror("shmdt");
    }

    return 0;
}
