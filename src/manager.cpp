#include "shared.h"
#include <sys/wait.h>
#include <map>
#include <algorithm>
#include <cmath>

sem_t* semaphore = nullptr;
SharedState* state = nullptr;
int shmid;
bool store_empty = false;

// Obsługa sygnałów
void handle_fire_signal(int signo) {
    std::cout << "Manager: Evacuation triggered!" << std::endl;

    // TODO: Zachowanie w trakcie ewakuacji
    bool clients_evacuated = false;
    while (!clients_evacuated) {
        int total_clients = 0;
        sem_lock(semaphore);
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (state->clients[i] != -1) {
                break;
            }
            clients_evacuated = true;
        }
        sem_unlock(semaphore);
    }

    store_empty = true;
    std::cout << "Manager: All clients have left. Shutting down store." << std::endl;
}

void handle_child_signal(int signo) {
    // Weryfikacja zamknięcia kasjera
    int status;
    pid_t pid = waitpid(-1, &status, WNOHANG);
    if (pid > 0) {
        sem_lock(semaphore);

        for (int i = 0; i < MAX_CHECKOUTS; i++) {
            int cashier_pid = state->cashiers[i];
            if (pid == cashier_pid) {

                state->checkout_statuses[i] = CLOSED;
                for (int j = 0; j < MAX_QUEUE; ++j) {
                    state->queues[i][j] = -1;
                }
                state->cashiers[i] = -1;

                std::cout << "\nManager: Checkout " << i + 1 << " closed by cashier (PID: " << pid << ")." << std::endl;
                break;
            }
        }

        sem_unlock(semaphore);
    }
}

void open_checkout(int checkout_number) {
    pid_t pid = fork();
    if (pid == 0) {
        // Proces kasjera
        char checkout_arg[8];
        sprintf(checkout_arg, "%d", checkout_number);
        execl("./cashier", "./cashier", checkout_arg, nullptr);
        perror("execl");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        std::cout << "\nManager: Requested checkout #" << checkout_number + 1 << " to be opened (PID: " << pid << ")." << std::endl;
    } else {
        perror("fork");
    }
}

void close_checkout(int checkout_number) {
    sem_lock(semaphore);

    CheckoutStatus checkout_status = state->checkout_statuses[checkout_number];
    pid_t pid = state->cashiers[checkout_number];

    sem_unlock(semaphore);
    
    if (checkout_status == OPEN) {
        kill(pid, SIGUSR2); // Sygnał zamknięcia kasy
        std::cout << "\nManager: Requested checkout #" << checkout_number + 1 << " to be closed (PID: " << pid << ")." << std::endl;
    }    
}

int main() {
    signal(SIGUSR1, handle_fire_signal);
    signal(SIGCHLD, handle_child_signal);

    // Inicjalizacja pamięci współdzielonej i semaforów
    initialize_shared_key_file(SHARED_KEY_FILE);
    key_t key = ftok(SHARED_KEY_FILE, 65);
    state = initialize_shared_memory(key, shmid);
    semaphore = initialize_semaphore();

    // Startowe otwarcie minimalnej liczby kas
    for (int i = 0; i < MIN_CHECKOUTS; i++) {
        open_checkout(i);
    }

    while (true) {
        sleep(5); // Okresowa kontrola stanu sklepu

        sem_lock(semaphore);
        if (state->evacuation) {
            sem_unlock(semaphore);
            break;
        }

        // Liczba klientów w sklepie
        int total_clients = 0;
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (state->clients[i] != -1) {
                ++total_clients;
            }
        }
        int open_checkouts = 0;

        std::cout << "\nManager: Periodic inspection" << std::endl;
        std::cout << "  Clients in supermarket: " << total_clients << std::endl;
        for (int i = 0; i < MAX_CHECKOUTS; i++) {
            CheckoutStatus curr_checkout_status = (CheckoutStatus)(state->checkout_statuses[i]);
            std::cout << "    Checkout " << i + 1 << ": " << curr_checkout_status << std::endl;
            if (curr_checkout_status != CLOSED) {
                if (curr_checkout_status == OPEN) open_checkouts++;
                int queue_size = 0;
                for (int j = 0; j < MAX_QUEUE; j++) {
                    if (state->queues[i][j] == -1) break;
                    queue_size++;
                }
                std::cout << "      Clients in queue: " << queue_size << std::endl;
            }
        }

        // Logika otwierania kas
        if (open_checkouts < std::max((double)MIN_CHECKOUTS, ceil(total_clients * 1.0 / K_CLIENTS))) {
            for (int i = 0; i < MAX_CHECKOUTS; ++i) {
                if (state->checkout_statuses[i] == CLOSED) {
                    sem_unlock(semaphore);
                    open_checkout(i);
                    break;
                }
            }
        }
        // Logika zamykania kas
        else if (total_clients < K_CLIENTS * (open_checkouts - 1) && open_checkouts > 2) {
            int min_clients = MAX_QUEUE + 1;
            int checkout_to_close = -1;

            // Znajdź kasę z najmniejszą liczbą klientów
            for (int i = 0; i < MAX_CHECKOUTS; ++i) {
                if (state->checkout_statuses[i] == OPEN) {
                    int clients_in_queue = 0;
                    for (int j = 0; j < MAX_QUEUE; ++j) {
                        if (state->queues[i][j] != -1) {
                            clients_in_queue++;
                        }
                    }
                    if (clients_in_queue < min_clients) {
                        min_clients = clients_in_queue;
                        checkout_to_close = i;
                    }
                }
            }

            if (checkout_to_close != -1) {
                sem_unlock(semaphore);
                close_checkout(checkout_to_close);
            }
        }

        sem_unlock(semaphore);
    }

    while (!store_empty) sleep(1);

    // TODO: Podsumowanie ewakuacji
    sem_lock(semaphore);

    int total_clients = 0;
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (state->clients[i] != -1) {
            ++total_clients;
        }
    }
    int open_checkouts = 0;

    std::cout << "\nManager: Before closing summary" << std::endl;
    std::cout << "  Clients in supermarket: " << total_clients << std::endl;
    for (int i = 0; i < MAX_CHECKOUTS; i++) {
        CheckoutStatus curr_checkout_status = (CheckoutStatus)(state->checkout_statuses[i]);
        std::cout << "    Checkout " << i + 1 << ": " << curr_checkout_status << std::endl;
        if (curr_checkout_status != CLOSED) {
            if (curr_checkout_status == OPEN) open_checkouts++;
            int queue_size = 0;
            for (int j = 0; j < MAX_QUEUE; j++) {
                if (state->queues[i][j] == -1) break;
                queue_size++;
            }
            std::cout << "      Clients in queue: " << queue_size << std::endl;
        }
    }

    sem_unlock(semaphore);

    cleanup_shared_memory(shmid, state);
    cleanup_semaphore(semaphore);

    return 0;
}
