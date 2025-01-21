#include "shared.h"
#include "ccol.h"
#include <sys/wait.h>
#include <map>
#include <algorithm>
#include <cmath>
#include <errno.h>

sem_t* semaphore = nullptr;
SharedState* state = nullptr;
int shmid;
bool store_empty = false;
bool shutting_down = false;

// Obsługa sygnałów

// Sygnał pożaru
void handle_fire_signal(int signo) {
    std::cout << info_important << "Manager: Evacuation triggered!" << reset_color << std::endl;

    shutting_down = true;

    // Zaczekaj aż kazdy klient opuści sklep
    bool clients_evacuated = false;
    while (!clients_evacuated) {
        clients_evacuated = true;
        sleep(1);

        sem_lock(semaphore);

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (state->clients[i] != -1) {
                clients_evacuated = false;
                break;
            }
        }

        sem_unlock(semaphore);
    }

    // Zaczekaj aż kasjerzy opuszczą kasy
    bool cashiers_evacuated = false;
    while (!cashiers_evacuated) {
        cashiers_evacuated = true;
        sleep(1);

        sem_lock(semaphore);

        for (int i = 0; i < MAX_CHECKOUTS; i++) {
            if (state->cashiers[i] != -1) {
                cashiers_evacuated = false;
                break;
            }
        }

        sem_unlock(semaphore);
    }

    store_empty = true;
    std::cout << info_important << "Manager: All clients and cashiers have left. Shutting down store." << reset_color << std::endl;
}

// Funkcje pomocnicze

// Otwieranie wybranej kasy
void open_checkout(int checkout_number) {
    sem_lock(semaphore);
    
    pid_t pid = fork();
    // Utworzenie procesu kasjera przekazując mu odpowiedni numer kasy
    if (pid == 0) {
        char checkout_arg[8];
        sprintf(checkout_arg, "%d", checkout_number);
        execl("./cashier", "./cashier", checkout_arg, nullptr);
        perror("execl");
        std::cerr << "errno: " << errno << std::endl;
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        // Ustawienie informacji o kasjerze i otwarcie kasy w pamięci dzielonej
        state->cashiers[checkout_number] = pid;
        state->checkout_statuses[checkout_number] = OPENING;
        std::cout << info_alt << "\nManager: Requested checkout #" << checkout_number + 1 << " to be opened (PID: " << pid << ")." << reset_color << std::endl;
    } else {
        perror("fork");
        std::cerr << "errno: " << errno << std::endl;
    }

    sem_unlock(semaphore);
}

// Zamykanie wybranej kasy
void close_checkout(int checkout_number) {
    sem_lock(semaphore);

    // Pobranie aktualnego statusu kasy i PID kasjera
    CheckoutStatus checkout_status = state->checkout_statuses[checkout_number];
    pid_t pid = state->cashiers[checkout_number];

    if (checkout_status == OPEN) {
        int sys_f_res;
        // Wysłanie sygnału zamknięcia kasy
        sys_f_res = kill(pid, SIGUSR2); 
        if (sys_f_res == -1) {
            perror("kill");
            std::cerr << "errno: " << errno << std::endl;
        }
        else {
            std::cout << info_alt << "\nManager: Requested checkout #" << checkout_number + 1 << " to be closed (PID: " << pid << ")." << reset_color << std::endl;
        } 
    }    

    sem_unlock(semaphore);
}

// Funkcja obsługująca oczyszczanie procesów potomnych
void* wait_children(void *arg) {
    while (!shutting_down) {
        int status;

        // Zastosowanie oczekiwania bez blokowania
        pid_t pid = waitpid(-1, &status, WNOHANG); 
        if (pid > 0) {
            std::cout << info_alt << "Manager: Cashier process (" << pid << ") finished." << reset_color << std::endl;
            // Sprawdzenie, czy kasjer, który zakończył proces z błędem nie był odpowiedzialny za kasę
            if (WIFEXITED(status)) {
                int ret_code = WEXITSTATUS(status);
                if (ret_code != 0) {
                    sem_lock(semaphore);
                    std::cout << info_alt_important << "Manager: Cashier process (" << pid << ") exited with error (" << ret_code << ")." << reset_color << std::endl;

                    bool active_cashier = false;
                    int checkout_number = -1;
                    for (int i = 0; i < MAX_CHECKOUTS; i++) {
                        if (state->cashiers[i] == pid) {
                            checkout_number = i;
                            active_cashier = true;
                            break;
                        }
                    }

                    // Oczyszczenie kasy, której nie udało się otworzyć
                    if (active_cashier && state->checkout_statuses[checkout_number] == OPENING) {
                        state->cashiers[checkout_number] = -1;
                        state->checkout_statuses[checkout_number] = CLOSED;
                        std::cout << info_alt_important << "Manager: Closing improperly opened checkout by cashier " << checkout_number + 1 << " (" << pid << ")." << reset_color << std::endl;
                    }

                    sem_unlock(semaphore);
                }
            }
        } else {
            sleep(1);
        }
    }

    // Upewnienie się, że po zatrzymaniu programu wszystkie procesy potomne są oczyszczone
    while (true) {
        int status;

        // Zastosowanie blokującego oczekiwania
        pid_t pid = waitpid(-1, &status, 0); 
        if (pid > 0) {
            std::cout << info_alt << "Manager: Cashier process (" << pid << ") finished after shutting down decision." << reset_color << std::endl;
            // Sprawdzenie, czy kasjer, który zakończył proces z błędem nie był odpowiedzialny za kasę
            if (WIFEXITED(status)) {
                int ret_code = WEXITSTATUS(status);
                if (ret_code != 0) {
                    sem_lock(semaphore);
                    std::cout << info_alt_important << "Manager: Cashier process (" << pid << ") exited with error (" << ret_code << ")." << reset_color << std::endl;

                    bool active_cashier = false;
                    int checkout_number = -1;
                    for (int i = 0; i < MAX_CHECKOUTS; i++) {
                        if (state->cashiers[i] == pid) {
                            checkout_number = i;
                            active_cashier = true;
                            break;
                        }
                    }

                    // Oczyszczenie kasy, której nie udało się otworzyć
                    if (active_cashier && state->checkout_statuses[checkout_number] == OPENING) {
                        state->cashiers[checkout_number] = -1;
                        state->checkout_statuses[checkout_number] = CLOSED;
                        std::cout << info_alt_important << "Manager: Closing improperly opened checkout by cashier " << checkout_number + 1 << " (" << pid << ")." << reset_color << std::endl;
                    }

                    sem_unlock(semaphore);
                }
            }
        } else {
            // Brak więcej procesów potomnych
            break; 
        }
    }

    return NULL;
}

int main() {
    // Rejestracja funkcji obsługujących sygnały
    // Sygnał pożaru
    sighandler_t sig;
    sig = signal(SIGUSR1, handle_fire_signal);
    if (sig == SIG_ERR) {
        perror("signal");
        std::cerr << "errno: " << errno << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << info << "Manager: Preparing the store to open." << reset_color << std::endl;

    // Inicjalizacja pamięci współdzielonej i semaforów
    initialize_shared_key_file(SHARED_KEY_FILE);
    key_t key = ftok(SHARED_KEY_FILE, 65);
    if (key == -1) {
        std::cout << fatal << "Manager: Unable to open the store." << reset_color << std::endl;
        perror("ftok");
        std::cerr << "errno: " << errno << std::endl;
        delete_file(SHARED_KEY_FILE);
        return EXIT_FAILURE;
    }

    state = initialize_shared_memory(key, shmid);
    if (state == nullptr) {
        std::cout << fatal << "Manager: Unable to open the store." << reset_color << std::endl;
        delete_file(SHARED_KEY_FILE);
        return EXIT_FAILURE;
    }

    semaphore = initialize_semaphore();
    if (semaphore == nullptr) {
        std::cout << fatal << "Manager: Unable to open the store." << reset_color << std::endl;
        cleanup_shared_memory(shmid, state);
        delete_file(SHARED_KEY_FILE);
        return EXIT_FAILURE;
    }

    std::cout << success_important << "Manager: Opening the store." << reset_color << std::endl;

    int sys_f_res;
    // Utworzenie wątku do oczyszczania procesów potomnych
    pthread_t wait_children_thread;
    sys_f_res = pthread_create(&wait_children_thread, NULL, wait_children, NULL);
    if (sys_f_res != 0) {
        perror("pthread_create");
        std::cerr << "errno: " << errno << std::endl;
        cleanup_shared_memory(shmid, state);
        cleanup_semaphore(semaphore);
        delete_file(SHARED_KEY_FILE);
        return EXIT_FAILURE;
    }

    // Startowe otwarcie minimalnej liczby kas
    for (int i = 0; i < MIN_CHECKOUTS; i++) {
        open_checkout(i);
    }

    while (true) {
        // Okresowa kontrola stanu sklepu
        sleep(5); 

        sem_lock(semaphore);

        if (state->evacuation) {
            // Przerwanie kontroli w przypadku ewakuacji
            sem_unlock(semaphore);
            break;
        }

        // Odczytanie liczby klientów w sklepie
        int total_clients = 0;
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (state->clients[i] != -1) {
                ++total_clients;
            }
        }

        int open_checkouts = 0;

        // Wydrukowanie podsumowania inspekcji: Ilość klientów, statusy kas i liczba klientów w kolejkach
        std::cout << info << "\nManager: Periodic inspection" << std::endl;
        std::cout << "  Clients in supermarket: " << total_clients << std::endl;
        for (int i = 0; i < MAX_CHECKOUTS; i++) {
            CheckoutStatus curr_checkout_status = (CheckoutStatus)(state->checkout_statuses[i]);
            std::cout << "    Checkout " << i + 1 << ": " << checkout_status_to_string(curr_checkout_status) << std::endl;
            if (curr_checkout_status != CLOSED) {
                if (curr_checkout_status == OPEN || curr_checkout_status == OPENING) open_checkouts++;
                int queue_size = 0;
                for (int j = 0; j < MAX_QUEUE; j++) {
                    if (state->queues[i][j] == -1) break;
                    queue_size++;
                }
                std::cout << "      Clients in queue: " << queue_size << std::endl;
            }
        }
        std::cout << reset_color;

        bool took_action = false;

        // Sprawdzenie konieczności otwarcia nowej kasy
        if (open_checkouts < std::max((double)MIN_CHECKOUTS, ceil(total_clients * 1.0 / K_CLIENTS))) {
            for (int i = 0; i < MAX_CHECKOUTS; ++i) {
                if (state->checkout_statuses[i] == CLOSED) {
                    sem_unlock(semaphore);

                    open_checkout(i);
                    took_action = true;
                    break;
                }
            }
        }
        // Sprawdzenie konieczności zamknięcia kasy
        else if (total_clients < K_CLIENTS * (open_checkouts - 1) && open_checkouts > MIN_CHECKOUTS) {
            int min_clients = MAX_QUEUE + 1;
            int checkout_to_close = -1;

            // Znajdowanie kasy z najmniejszą liczbą klientów
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
                took_action = true;
            }
        }

        // Zwolnienie dostępu do pamięci
        if (!took_action) {  
            sem_unlock(semaphore);
        }
    }

    while (!store_empty) {
        sleep(1);
    }
    
    // Dołączanie wątku odpowiedzialnego za czyszczenie
    sys_f_res = pthread_join(wait_children_thread, NULL);
    if (sys_f_res != 0) {
        perror("pthread_join");
        std::cerr << "errno: " << errno << std::endl;
    }

    // Podsumowanie ewakuacji - Wypisanie ilości klientów w sklepie i ilości czynnych kas
    sem_lock(semaphore);

    int total_clients = 0;
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (state->clients[i] != -1) {
            ++total_clients;
        }
    }
    int open_checkouts = 0;

    std::cout << warning << "\nManager: Before closing summary" << std::endl;
    std::cout << "  Clients in supermarket: " << total_clients << std::endl;
    for (int i = 0; i < MAX_CHECKOUTS; i++) {
        CheckoutStatus curr_checkout_status = (CheckoutStatus)(state->checkout_statuses[i]);
        std::cout << "    Checkout " << i + 1 << ": " << checkout_status_to_string(curr_checkout_status) << std::endl;
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
    std::cout << reset_color;

    sem_unlock(semaphore);

    // Usunięcie utworzonych struktur
    cleanup_shared_memory(shmid, state);
    cleanup_semaphore(semaphore);
    delete_file(SHARED_KEY_FILE);

    return 0;
}
