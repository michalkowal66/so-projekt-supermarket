#include "shared.h"
#include "ccol.h"

int fire_chance = 20;

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << warning << "Fireman: Fire chance not provided, using default value: " << fire_chance << reset_color << std::endl << std::endl;
    }
    else {
        try {
            fire_chance = std::stoi(argv[1]);
        } catch (const std::exception& e) {
            std::cout << warning << "Fireman: Fire chance parsing error, using default value: " << fire_chance << reset_color << std::endl << std::endl;
        }
    }

    srand(time(nullptr) ^ getpid());

    SharedState* state = get_shared_memory();
    if (state == nullptr) {
        std::cerr << fatal << "Fireman: The store is unavailable." << reset_color << std::endl;
        return EXIT_FAILURE;
    }

    sem_t* semaphore = sem_open(SEM_NAME, 0);

    while (true) {
        // Okresowe sprawdzanie
        std::cout << info << "Fireman: Periodic inspection, checking for fire." << reset_color << std::endl;
        if (rand() % 100 < fire_chance) { // Losowa szansa na pojawienie się pożaru w momencie sprawdzenia
            std::cout << warning << "Fireman: Fire detected, alarming supermarket users." << reset_color << std::endl;
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

            std::cout << success_important << "Fireman: Supermarket users alarmed. Store is being evacuated." << reset_color << std::endl;
            break;
        }
        std::cout << success << "Fireman: No fire detected." << reset_color << std::endl << std::endl;
        sleep(10);
    }

    if (shmdt(state) == -1) {
        perror("shmdt");
    }

    return 0;
}
