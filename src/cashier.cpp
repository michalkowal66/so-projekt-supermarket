#include "shared.h"
#include "ccol.h"

pid_t cashier_pid;
int checkout_number;
sem_t* semaphore = nullptr;
SharedState* state = nullptr;

// Obsługa sygnałów
void handle_fire_signal(int signo) {
    sem_lock(semaphore);

    state->checkout_statuses[checkout_number] = CLOSED;
    for (int i = 0; i < MAX_QUEUE; i++) 
        state->queues[checkout_number][i] = -1;

    state->cashiers[checkout_number] = -1;

    std::cout << info_important << "Cashier " << checkout_number + 1 << " (" << cashier_pid << "): Evacuating." << reset_color << std::endl;
    
    sem_unlock(semaphore);

    exit(0);
}

void handle_closing_signal(int signo) {
    sem_lock(semaphore);

    if (state->checkout_statuses[checkout_number] != CLOSING)
        state->checkout_statuses[checkout_number] = CLOSING;

    sem_unlock(semaphore);
    
    std::cout << info_alt << "Cashier " << checkout_number + 1 << " (" << cashier_pid << "): Closing after serving remaining clients." << reset_color << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << error << "Usage: cashier <checkout_number>" << reset_color << std::endl;
        return EXIT_FAILURE;
    }

    cashier_pid = getpid();
    checkout_number = std::stoi(argv[1]);

    state = get_shared_memory();
    if (state == nullptr) {
        std::cerr << fatal << "Cashier " << checkout_number + 1 << " (" << cashier_pid << "): The store is unavailable." << reset_color << std::endl;
        return EXIT_FAILURE;
    }
    
    semaphore = sem_open(SEM_NAME, 0);

    signal(SIGUSR1, handle_fire_signal);
    signal(SIGUSR2, handle_closing_signal);

    std::cout << success << "Cashier " << checkout_number + 1 << " (" << cashier_pid << "): Opened checkout " << checkout_number + 1 << reset_color << std::endl;

    while (true) {
        sem_lock(semaphore);

        // Sprawdzenie statusu kasy
        if (state->checkout_statuses[checkout_number] == CLOSED) {
            sem_unlock(semaphore);
            break;
        }

        // Obsługa pierwszego klienta w kolejce
        int client_pid = state->queues[checkout_number][0];

        sem_unlock(semaphore);

        if (client_pid != -1) {
            sleep(8); // Symulacja obsługi

            // Powiadomienie klienta o zakończeniu obsługi
            char client_fifo[32];
            sprintf(client_fifo, "/tmp/client_%d", client_pid);
            int client_fd = open(client_fifo, O_WRONLY);
            if (client_fd != -1) {
                write(client_fd, "DONE", 4);
                close(client_fd);
            }

            // Przesunięcie kolejnych klientów w kolejce
            sem_lock(semaphore);

            for (int j = 0; j < MAX_QUEUE - 1; j++) {
                state->queues[checkout_number][j] = state->queues[checkout_number][j + 1];
            }
            state->queues[checkout_number][MAX_QUEUE - 1] = -1;
            
            std::cout << success << "\nCashier " << checkout_number + 1 << " (" << cashier_pid << "): Served client " << client_pid << "." << reset_color << std::endl;
            
            sem_unlock(semaphore);
        } else {
            // Brak klientów w kolejce
            sleep(1);
        }

        // Sprawdzenie statusu kasy
        sem_lock(semaphore);

        if (state->checkout_statuses[checkout_number] == CLOSING) {
            bool queue_empty = true;
            for (int j = 0; j < MAX_QUEUE; ++j) {
                if (state->queues[checkout_number][j] != -1) {
                    queue_empty = false;
                    break;
                }
            }
            if (queue_empty) {
                state->checkout_statuses[checkout_number] = CLOSED;
                state->cashiers[checkout_number] = -1;

                sem_unlock(semaphore);

                std::cout << info_alt << "Cashier " << checkout_number + 1 << " (" << cashier_pid << "): Closed checkout " << checkout_number + 1 << reset_color << std::endl;
                break;
            }
        }

        sem_unlock(semaphore);
    }

    if (shmdt(state) == -1) {
        perror("shmdt");
    }

    return 0;
}
