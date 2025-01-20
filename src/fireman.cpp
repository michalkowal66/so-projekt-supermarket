#include "shared.h"
#include "ccol.h"
#include <errno.h>

// Domyślna szansa na pożar
int fire_chance = 20;

int main(int argc, char* argv[]) {
    // Walidacja argumentu uruchomienia
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

    // Inicjalizacja pamięci współdzielonej i semaforów
    SharedState* state = get_shared_memory();
    if (state == nullptr) {
        std::cerr << fatal << "Fireman: The store is unavailable." << reset_color << std::endl;
        return EXIT_FAILURE;
    }

    sem_t* semaphore = sem_open(SEM_NAME, 0);
    if (semaphore == SEM_FAILED) {
        perror("sem_open");
        std::cerr << "errno: " << errno << std::endl;
        return EXIT_FAILURE;
    }

    while (true) {
        // Okresowe sprawdzanie
        std::cout << info << "Fireman: Periodic inspection, checking for fire." << reset_color << std::endl;
        // Losowa szansa na pojawienie się pożaru w momencie sprawdzenia
        if (rand() % 100 < fire_chance) { 
            sem_lock(semaphore);
            std::cout << warning << "Fireman: Fire detected, alarming supermarket users." << reset_color << std::endl;

            // Wysłanie sygnału pożaru do menadżera, kasjerów i klientów sklepu
            state->evacuation = true;

            int sys_f_res;

            sys_f_res = kill(state->manager, SIGUSR1); 
            if (sys_f_res == -1) {
                perror("kill");
                std::cerr << "errno: " << errno << std::endl;
            }

            for (int i = 0; i < MAX_CHECKOUTS; i++) {
                pid_t cashier_pid = state->cashiers[i];
                if (cashier_pid != -1) {
                    sys_f_res = kill(cashier_pid, SIGUSR1);
                    if (sys_f_res == -1) {
                        perror("kill");
                        std::cerr << "errno: " << errno << std::endl;
                    }
                }
            }

            for (int i = 0; i < MAX_CLIENTS; i++) {
                pid_t client_pid = state->clients[i];
                if (client_pid != -1) {
                    sys_f_res = kill(client_pid, SIGUSR1);
                    if (sys_f_res == -1) {
                        perror("kill");
                        std::cerr << "errno: " << errno << std::endl;
                    }
                }
            }
            
            std::cout << success_important << "Fireman: Supermarket users alarmed. Store is being evacuated." << reset_color << std::endl;

            sem_unlock(semaphore);
            break;
        }
        std::cout << success << "Fireman: No fire detected." << reset_color << std::endl << std::endl;
        sleep(10);
    }

    // Odłączenie segmentu pamięci dzielonej
    if (shmdt(state) == -1) {
        perror("shmdt");
        std::cerr << "shmdt: " << errno << std::endl;
    }

    return 0;
}
