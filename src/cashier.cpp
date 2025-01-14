#include "shared.h"

int checkoutNumber;
sem_t* semaphore = nullptr;
SharedState* state = nullptr;

// Obsługa sygnałów
void handle_fire_signal(int signo) {
    sem_lock(semaphore);

    state->checkoutStatuses[checkoutNumber] = CLOSED;
    // TODO: Usuń klientów z kolejki

    sem_unlock(semaphore);

    std::cout << "Cashier " << checkoutNumber << ": Evacuating." << std::endl;
    exit(0);
}

void handle_closing_signal(int signo) {
    sem_lock(semaphore);

    state->checkoutStatuses[checkoutNumber] = CLOSING;

    sem_unlock(semaphore);

    std::cout << "Cashier " << checkoutNumber << ": Closing after serving remaining clients." << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: cashier <checkout_number>" << std::endl;
        return EXIT_FAILURE;
    }

    checkoutNumber = std::stoi(argv[1]);

    key_t key = ftok("manager.cpp", 65);
    int shmid = shmget(key, sizeof(SharedState), 0666);
    state = (SharedState*)shmat(shmid, nullptr, 0);
    semaphore = sem_open(SEM_NAME, 0);

    signal(SIGUSR1, handle_fire_signal);
    signal(SIGUSR2, handle_closing_signal);

    while (true) {
        sem_lock(semaphore);

        if (state->queues[checkoutNumber].size() == 0) {
            if (state->checkoutStatuses[checkoutNumber] == CLOSING) {
                state->checkoutStatuses[checkoutNumber] = CLOSED;
                sem_unlock(semaphore);

                break;
            }
            sem_unlock(semaphore);

            sleep(1);
            continue;
        }

        int clientPid = state->queues[checkoutNumber].front(); // Pobranie PID klienta
        state->queues[checkoutNumber].pop();

        sem_unlock(semaphore);

        if (clientPid > 0) {
            sleep(2); // Symulacja obsługi

            char clientFifo[32];
            sprintf(clientFifo, "/tmp/client_%d", clientPid);

            int clientFd = open(clientFifo, O_WRONLY);

            write(clientFd, "DONE", 4); // Powiadomienie klienta
            close(clientFd);
        }
    }

    if (shmdt(state) == -1) {
        perror("shmdt");
    }

    return 0;
}
