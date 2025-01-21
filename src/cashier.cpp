#include "shared.h"
#include "ccol.h"
#include <errno.h>

// PID kasjera, numer kasy, semafor, struktura pamięci dzielonej
pid_t cashier_pid;
int checkout_number;
sem_t* semaphore = nullptr;
SharedState* state = nullptr;

// Obsługa sygnałów

// Sygnał pożaru
void handle_fire_signal(int signo) {
    sem_lock(semaphore);

    // Zamknij kasę, usuń klietnów z kolejki, usuń się z listy kasjerów
    state->checkout_statuses[checkout_number] = CLOSED;
    for (int i = 0; i < MAX_QUEUE; i++) 
        state->queues[checkout_number][i] = -1;

    state->cashiers[checkout_number] = -1;

    std::cout << info_important << "Cashier " << checkout_number + 1 << " (" << cashier_pid << "): Evacuating." << reset_color << std::endl;
    
    sem_unlock(semaphore);

    exit(0);
}

// Sygnał zamknięcia kasy
void handle_closing_signal(int signo) {
    sem_lock(semaphore);

    // Ustaw odpowiedni status kasy
    if (state->checkout_statuses[checkout_number] != CLOSING)
        state->checkout_statuses[checkout_number] = CLOSING;

    std::cout << info_alt << "Cashier " << checkout_number + 1 << " (" << cashier_pid << "): Closing after serving remaining clients." << reset_color << std::endl;
    
    sem_unlock(semaphore);
}

int main(int argc, char* argv[]) {
    // Walidacja argumentu uruchomienia
    if (argc != 2) {
        std::cerr << error << "Usage: cashier <checkout_number>" << reset_color << std::endl;
        return EXIT_FAILURE;
    }
    else {
        try {
            checkout_number = std::stoi(argv[1]);
        } catch (const std::exception& e) {
            std::cout << warning << "Cashier: Checkout number parsing error." << reset_color << std::endl << std::endl;
        }

        if (checkout_number < 0 || checkout_number >= MAX_CHECKOUTS) {
            std::cout << error << "Cashier: Checkout number must lie within range: (0, " << MAX_CHECKOUTS - 1 << ")." << reset_color << std::endl;
            return EXIT_FAILURE;
        }
    }

    // Inicjalizacja zmiennych i pamięci dzielonej
    cashier_pid = getpid();

    state = get_shared_memory();
    if (state == nullptr) {
        std::cout << fatal << "Cashier " << checkout_number + 1 << " (" << cashier_pid << "): The store is unavailable." << reset_color << std::endl;
        return EXIT_FAILURE;
    }
    
    semaphore = sem_open(SEM_NAME, 0);
    if (semaphore == SEM_FAILED) {
        std::cout << fatal << "Cashier " << checkout_number + 1 << " (" << cashier_pid << "): The store is unavailable." << reset_color << std::endl;
        perror("sem_open");
        std::cerr << "errno: " << errno << std::endl;
        return EXIT_FAILURE;
    }

    // Rejestracja funkcji obsługujących sygnały
    // Sygnał pożaru
    sighandler_t sig;
    sig = signal(SIGUSR1, handle_fire_signal);
    if (sig == SIG_ERR) {
        std::cout << fatal << "Cashier " << checkout_number + 1 << " (" << cashier_pid << "): Unable to set up properly." << reset_color << std::endl;
        perror("signal");
        std::cerr << "errno: " << errno << std::endl;
        return EXIT_FAILURE;
    }
    // Sygnał zamknięcia kasy
    sig = signal(SIGUSR2, handle_closing_signal);
    if (sig == SIG_ERR) {
        std::cout << fatal << "Cashier " << checkout_number + 1 << " (" << cashier_pid << "): Unable to set up properly." << reset_color << std::endl;
        perror("signal");
        std::cerr << "errno: " << errno << std::endl;
        return EXIT_FAILURE;
    }

    sem_lock(semaphore);
    
    state->checkout_statuses[checkout_number] = OPEN;

    std::cout << success << "Cashier " << checkout_number + 1 << " (" << cashier_pid << "): Opened checkout " << checkout_number + 1 << reset_color << std::endl;

    sem_unlock(semaphore);

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
            // Symulacja obsługi
            sleep(8); 

            bool client_served_properly = true;

            // Powiadomienie klienta o zakończeniu obsługi
            char client_fifo[32];
            sprintf(client_fifo, "/tmp/client_%d", client_pid);
            int client_fd = open(client_fifo, O_WRONLY);
            if (client_fd != -1) {
                int sys_f_res;
                sys_f_res = write(client_fd, "DONE", 4);
                if (sys_f_res == -1) {
                    perror("write");
                    std::cerr << "errno: " << errno << std::endl;
                    sys_f_res = kill(client_pid, SIGUSR2);
                    if (sys_f_res == -1) {
                        perror("kill");
                        std::cerr << "errno: " << errno << std::endl;
                    }            
                    client_served_properly = false;
                }
                sys_f_res = close(client_fd);
                if (sys_f_res == -1) {
                    perror("close");
                    std::cerr << "errno: " << errno << std::endl;
                }
            }
            else {
                perror("open");
                std::cerr << "errno: " << errno << std::endl;
                int sys_f_res = kill(client_pid, SIGUSR2);
                if (sys_f_res == -1) {
                    perror("kill");
                    std::cerr << "errno: " << errno << std::endl;
                }           
                client_served_properly = false;
            }

            // Przesunięcie kolejnych klientów w kolejce
            sem_lock(semaphore);

            for (int j = 0; j < MAX_QUEUE - 1; j++) {
                state->queues[checkout_number][j] = state->queues[checkout_number][j + 1];
            }
            state->queues[checkout_number][MAX_QUEUE - 1] = -1;
            
            std::cout << success << "\nCashier " << checkout_number + 1 << " (" << cashier_pid << "): ";
            if (client_served_properly) {
                std::cout << "Served client " << client_pid << ".";
            }
            else {
                std::cout << warning << "Wasn't able to serve client " << client_pid << ". Skipping to the next client.";
            }
            std::cout << reset_color << std::endl;
            
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
            // Zamknięcie kasy oczekującej na zamknięcie przy braku klientów w kolejce
            if (queue_empty) {
                state->checkout_statuses[checkout_number] = CLOSED;
                state->cashiers[checkout_number] = -1;

                std::cout << info_alt << "Cashier " << checkout_number + 1 << " (" << cashier_pid << "): Closed checkout " << checkout_number + 1 << reset_color << std::endl;

                sem_unlock(semaphore);
                break;
            }
        }

        sem_unlock(semaphore);
    }

    // Odłączenie segmentu pamięci dzielonej
    if (shmdt(state) == -1) {
        perror("shmdt");
        std::cerr << "shmdt: " << errno << std::endl;
    }

    return 0;
}
