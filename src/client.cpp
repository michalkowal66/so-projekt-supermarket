#include "shared.h"
#include "ccol.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdlib>
#include <ctime>
#include <errno.h>

// PID klienta, semafor, struktura pamięci dzielonej
pid_t client_pid;
sem_t* semaphore = nullptr;
SharedState* state = nullptr;

// Deskrpytor FIFO klienta, ścieżka FIFO klienta, flaga wskazująca, czy FIFO jest utworzone
int fifo_fd = -1;
char fifo_name[32];
int fifo_linked = -1;

// Próba zamknięcia i odlinkowania FIFO
void cleanup_fifo() {
    int sys_f_res;
    if (fifo_fd != -1) {
        sys_f_res = close(fifo_fd);
        if (sys_f_res == -1) {
            perror("close");
            std::cerr << "errno: " << errno << std::endl;
        }
    }
    if (fifo_linked != -1) {
        sys_f_res = unlink(fifo_name);
        if (sys_f_res == -1) {
            perror("unlink");
            std::cerr << "errno: " << errno << std::endl;
        }
    }
}

// Obsługa sygnałów

// Sygnał pożaru
void handle_fire_signal(int signo) {
    sem_lock(semaphore);

    // Usunięcie klienta z listy klientów w sklepie
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (state->clients[i] == client_pid) {
            state->clients[i] = -1;
            break;
        }
    }

    sem_unlock(semaphore);

    // Zamknij i odlinkuj FIFO
    cleanup_fifo();

    std::cout << info_important << "Client " << client_pid << ": Evacuating supermarket!" << reset_color << std::endl;
    exit(0);
}

// Obsługa sygnału SIGINT
void handle_sigint_signal(int signo) {
    std::cout << warning << "Client " << client_pid << " received SIGINT. Ignoring signal. Use SIGUSR2 to kill." << reset_color << std::endl;
}

// Obsługa sygnału SIGUSR2 - Natychmiastowe zamknięcie
void handle_sigusr2_signal(int signo) {
    std::cout << warning << "Client " << client_pid << " received request to leave immediately." << reset_color << std::endl;

    sem_lock(semaphore);

    // Usunięcie klienta z listy klientów w sklepie
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (state->clients[i] == client_pid) {
            state->clients[i] = -1;
            break;
        }
    }

    sem_unlock(semaphore);

    // Zamknij i odlinkuj FIFO
    cleanup_fifo();

    std::cout << info_important << "Client " << client_pid << ": Left supermarket due to request." << reset_color << std::endl;
    exit(0);
}

int main() {
    // Inicjalizacja zmiennych i pamięci dzielonej
    client_pid = getpid();

    sprintf(fifo_name, "/tmp/client_%d", client_pid);

    state = get_shared_memory();
    if (state == nullptr) {
        std::cout << fatal << "Client " << client_pid << ": The store is unavailable." << reset_color << std::endl;
        return EXIT_FAILURE;
    }

    semaphore = sem_open(SEM_NAME, 0);
    if (semaphore == SEM_FAILED) {
        std::cout << fatal << "Client " << client_pid << ": The store is unavailable." << reset_color << std::endl;
        perror("sem_open");
        std::cerr << "errno: " << errno << std::endl;
        return EXIT_FAILURE;
    }

    // Rejestracja funkcji obsługujących sygnały
    // Sygnał pożaru
    sighandler_t sig;
    sig = signal(SIGUSR1, handle_fire_signal);
    if (sig == SIG_ERR) {
        std::cout << fatal << "Client " << client_pid << ": Unable to set up properly." << reset_color << std::endl;
        perror("signal");
        std::cerr << "errno: " << errno << std::endl;
        return EXIT_FAILURE;
    }
    // Sygnał SIGINT
    sig = signal(SIGINT, handle_sigint_signal);
    if (sig == SIG_ERR) {
        std::cout << fatal << "Client " << client_pid << ": Unable to set up properly." << reset_color << std::endl;
        perror("signal");
        std::cerr << "errno: " << errno << std::endl;
        return EXIT_FAILURE;
    }
    // Sygnał SIGUSR2 - Zamknięcie programu
    sig = signal(SIGUSR2, handle_sigusr2_signal);
    if (sig == SIG_ERR) {
        std::cout << fatal << "Client " << client_pid << ": Unable to set up properly." << reset_color << std::endl;
        perror("signal");
        std::cerr << "errno: " << errno << std::endl;
        return EXIT_FAILURE;
    }

    // Utworzenie FIFO do komunikacji z kasjerem
    fifo_linked = mkfifo(fifo_name, 0666);
    if (fifo_linked == -1) {
        std::cout << fatal << "Client " << client_pid << ": Unable to set up properly." << reset_color << std::endl;
        perror("mkfifo");
        std::cerr << "errno: " << errno << std::endl;
        return EXIT_FAILURE;
    }

    srand(time(nullptr) ^ client_pid);

    // Sprawdzenie statusu sklepu i próba dodania klienta do listy klientów
    sem_lock(semaphore);

    bool evacuation = state->evacuation;

    sem_unlock(semaphore);

    // Zakończenie pracy programu w przypadku trwającej ewakuacji
    if (evacuation) {
        std::cout << error << "Client " << client_pid << ": Store is being evacuated, cannot enter." << reset_color << std::endl;
        cleanup_fifo();
        return EXIT_FAILURE;
    }

    sem_lock(semaphore);

    // Próba dołączenia do listy klientów
    bool added = false;
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (state->clients[i] == -1) {
            state->clients[i] = client_pid;
            added = true;
            break;
        }
    }

    sem_unlock(semaphore);

    // Zakończenie pracy programu w przypadku nieudanej próby wejścia do sklepu
    if (!added) {
        std::cout << error << "Client " << client_pid << ": Could not enter the supermarket." << reset_color << std::endl;
        cleanup_fifo();
        return EXIT_FAILURE;
    }

    std::cout << success << "Client " << client_pid << ": Entered the supermarket." << reset_color << std::endl;
    
    // Symulacja zakupów
    int shopping_time = rand() % 8 + 3; // Zakupy trwają 3-10 sekund
    sleep(shopping_time);
    std::cout << info << "Client " << client_pid << ": Finished shopping in " << shopping_time << " seconds." << reset_color << std::endl;

    // Próba ustawienia się w kolejce
    // Liczba prób ustawienia się w kolejce
    int attempts = 2; 
    bool queued = false;
    int selected_queue;

    for (int attempt = 0; attempt < attempts; ++attempt) {
        sem_lock(semaphore);

        // Znajdowanie najkrótszej kolejki
        int min_queue_length = MAX_QUEUE + 1;
        selected_queue = -1;

        for (int i = 0; i < MAX_CHECKOUTS; ++i) {
            if (state->checkout_statuses[i] == OPEN) {
                int queue_length = 0;
                for (int j = 0; j < MAX_QUEUE; ++j) {
                    if (state->queues[i][j] != -1) {
                        ++queue_length;
                    }
                }

                if (queue_length < min_queue_length && queue_length < MAX_QUEUE) {
                    min_queue_length = queue_length;
                    selected_queue = i;
                }
            }
        }

        // Dodanie klienta do wybranej kolejki
        if (selected_queue != -1) {
            for (int j = 0; j < MAX_QUEUE; ++j) {
                if (state->queues[selected_queue][j] == -1) {
                    state->queues[selected_queue][j] = client_pid;
                    queued = true;
                    std::cout << info << "Client " << client_pid << ": Joined queue " << selected_queue + 1 << "." << reset_color << std::endl;
                    break;
                }
            }
        }

        sem_unlock(semaphore);

        if (queued) break;

        // Odczekanie losowego czasu przed kolejną próbą
        std::cout << warning << "Client " << client_pid << ": All queues are full. Retrying in a moment." << reset_color << std::endl;
        sleep(rand() % 3 + 1); 
    }

    // Zakończenie pracy programu po wyczerpaniu prób ustawienia się w kolejce
    if (!queued) {
        std::cout << warning << "Client " << client_pid << ": Could not join any queue. Leaving without purchases." << reset_color << std::endl;
        
        sem_lock(semaphore);

        // Usunięcie klienta z listy klientów w sklepie
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (state->clients[i] == client_pid) {
                state->clients[i] = -1;
                break;
            }
        }

        sem_unlock(semaphore);

        // Odlinkowanie FIFO
        cleanup_fifo();

        return EXIT_FAILURE;
    }

    // Oczekiwanie obsłużenie
    int fifo_fd = open(fifo_name, O_RDONLY | O_NONBLOCK);

    if (fifo_fd == -1) {
        perror("open");
        std::cerr << "errno: " << errno << std::endl;
    } else {
        char buffer[16];
        while (true) {
            int bytes_read = read(fifo_fd, buffer, sizeof(buffer) - 1);
            if (bytes_read > 0) {
                buffer[bytes_read] = '\0';
                if (strcmp(buffer, "DONE") == 0) {
                    std::cout << success_important << "Client " << client_pid << ": Finished checkout. Leaving supermarket." << reset_color << std::endl;
                    break;
                }
            }
            else if (bytes_read == -1) {
                // FIFO jest puste, sprawdzenie, czy klient nadal jest w kolejce
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
            bool in_queue = false;

            sem_lock(semaphore);

            for (int i = 0; i < MAX_QUEUE; i++) {
                if (state->queues[selected_queue][i] == client_pid) {
                    in_queue = true;
                    break;
                }
            }

            sem_unlock(semaphore);

            if (!in_queue) {
                std::cout << error << "Client " << client_pid << ": Was removed from queue. Leaving supermarket." << reset_color << std::endl;
                break;
                    }
                    
                } else {
                    perror("read");
                    std::cerr << "errno: " << errno << std::endl;
                    break;
                }
            }
            
                sleep(1);
        }
    }

    // Usunięcie klienta z listy klientów w sklepie po zakończeniu zakupów
    sem_lock(semaphore);

    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (state->clients[i] == client_pid) {
            state->clients[i] = -1;
            break;
        }
    }

    sem_unlock(semaphore);

    // Zamknij i odlinkuj FIFO
    cleanup_fifo();

    return 0;
}
