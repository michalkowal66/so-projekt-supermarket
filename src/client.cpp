#include "shared.h"
#include <queue>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdlib>
#include <ctime>

sem_t* semaphore = nullptr;
SharedState* state = nullptr;
pid_t client_pid;

int fifo_fd = -1;
char fifo_name[32];

// Obsługa sygnału od strażaka
void handle_fire_signal(int signo) {
    sem_lock(semaphore);

    // Usuń klienta z listy klientów w sklepie
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (state->clients[i] == client_pid) {
            state->clients[i] = -1;
            break;
        }
    }

    sem_unlock(semaphore);

    if (fifo_fd != -1) {
        close(fifo_fd);
        unlink(fifo_name);
    }

    std::cout << "Client " << client_pid << ": Evacuating supermarket!" << std::endl;
    exit(0);
}

int main() {
    client_pid = getpid();
    sprintf(fifo_name, "/tmp/client_%d", client_pid);

    // Inicjalizacja
    state = get_shared_memory();
    if (state == nullptr) {
        std::cerr << "The store is unavailable." << std::endl;
        return EXIT_FAILURE;
    }

    semaphore = sem_open(SEM_NAME, 0);

    signal(SIGUSR1, handle_fire_signal);
    srand(time(nullptr) ^ client_pid);

    // Dodanie klienta do listy klientów w sklepie
    sem_lock(semaphore);
    bool evacuation = state->evacuation;
    sem_unlock(semaphore);
    if (evacuation) {
        std::cerr << "Client " << client_pid << ": Store is being evacuated, cannot enter." << std::endl;
        return EXIT_FAILURE;
    }

    sem_lock(semaphore);
    bool added = false;
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (state->clients[i] == -1) {
            state->clients[i] = client_pid;
            added = true;
            break;
        }
    }
    sem_unlock(semaphore);

    if (!added) {
        std::cerr << "Client " << client_pid << ": Could not enter the supermarket." << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Client " << client_pid << ": Entered the supermarket." << std::endl;

    // Symulacja zakupów
    int shopping_time = rand() % 8 + 3; // Zakupy trwają 3-10 sekund
    sleep(shopping_time);
    std::cout << "Client " << client_pid << ": Finished shopping in " << shopping_time << " seconds." << std::endl;

    // Próbuj ustawić się w kolejce
    int attempts = 2; // Liczba prób ustawienia się w kolejce
    bool queued = false;

    for (int attempt = 0; attempt < attempts; ++attempt) {
        sem_lock(semaphore);

        // Znajdź najkrótszą otwartą kolejkę
        int min_queue_length = MAX_QUEUE + 1;
        int selected_queue = -1;

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

        if (selected_queue != -1) {
            // Dodanie klienta do wybranej kolejki
            for (int j = 0; j < MAX_QUEUE; ++j) {
                if (state->queues[selected_queue][j] == -1) {
                    state->queues[selected_queue][j] = client_pid;
                    queued = true;
                    std::cout << "Client " << client_pid << ": Joined queue " << selected_queue + 1 << "." << std::endl;
                    break;
                }
            }
        }

        sem_unlock(semaphore);

        if (queued) {
            break;
        }

        std::cout << "Client " << client_pid << ": All queues are full. Retrying in a moment." << std::endl;
        sleep(rand() % 3 + 1); // Odczekaj losowy czas przed kolejną próbą
    }

    if (!queued) {
        std::cout << "Client " << client_pid << ": Could not join any queue. Leaving without purchases." << std::endl;
        sem_lock(semaphore);
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (state->clients[i] == client_pid) {
                state->clients[i] = -1;
                break;
            }
        }
        sem_unlock(semaphore);
        return EXIT_FAILURE;
    }

    // Utwórz FIFO do komunikacji z kasjerem
    mkfifo(fifo_name, 0666);

    // Czekaj na wiadomość od kasjera
    int fifo_fd = open(fifo_name, O_RDONLY);
    char buffer[16];
    while (true) {
        int bytes_read = read(fifo_fd, buffer, sizeof(buffer) - 1);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            if (strcmp(buffer, "DONE") == 0) {
                std::cout << "Client " << client_pid << ": Finished checkout. Leaving supermarket." << std::endl;
                break;
            }
        }
    }

    close(fifo_fd);
    unlink(fifo_name);

    // Usuń klienta z listy klientów w sklepie
    sem_lock(semaphore);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (state->clients[i] == client_pid) {
            state->clients[i] = -1;
            break;
        }
    }
    sem_unlock(semaphore);

    return 0;
}
