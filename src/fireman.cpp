#include "shared.h"

int main() {
    srand(time(nullptr) ^ getpid());

    SharedState* state = get_shared_memory();
    if (state == nullptr) {
        std::cerr << "The store is unavailable." << std::endl;
        exit(EXIT_FAILURE);
    }

    sem_t* semaphore = sem_open(SEM_NAME, 0);

    while (true) {
        sleep(10); // Okresowe sprawdzanie
        std::cout << "\nFireman: Periodic inspection, checking for fire." << std::endl;
        if (rand() % 100 > 80) { // Losowa szansa na pojawienie się pożaru w momencie sprawdzenia
            std::cout << "Fireman: Fire detected, alarming supermarket users." << std::endl;
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

            std::cout << "Fireman: Supermarket users alarmed. Store is being evacuated." << std::endl;
            break;
        }
        std::cout << "Fireman: No fire detected." << std::endl;
    }

    if (shmdt(state) == -1) {
        perror("shmdt");
    }

    return 0;
}
