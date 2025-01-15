#include "shared.h"

int main() {
    key_t key = ftok(SHARED_KEY_FILE, 65);
    int shmid = shmget(key, sizeof(SharedState), 0666);
    SharedState* state = (SharedState*)shmat(shmid, nullptr, 0);

    sem_t* semaphore = sem_open(SEM_NAME, 0);

    while (true) {
        sleep(10); // Okresowe sprawdzanie
        std::cout << "\nFireman: Periodic inspection, checking for fire." << std::endl;
        if (rand() % 100 > 80) { // Losowa szansa na pojawienie się pożaru w momencie sprawdzenia
            std::cout << "Fireman: Fire detected, alarming store users." << std::endl;
            sem_lock(semaphore);

            state->evacuation = true;

            kill(state->manager, SIGUSR1);
            for (int i = 0; i < MAX_CHECKOUTS; i++) {
                pid_t cashier_pid = state->cashiers[i];
                if (cashier_pid != -1) kill(cashier_pid, SIGUSR1);
            }

            for (int i = 0; i < MAX_CLIENTS; i++) {
                pid_t client_pid = state->clients[i];
                if (client_pid != -1) kill(client_pid, SIGUSR1);
            }
            
            sem_unlock(semaphore);

            break;
        }
    }

    if (shmdt(state) == -1) {
        perror("shmdt");
    }

    return 0;
}
