#include "ccol.h"
#include <iostream>
#include <thread>
#include <csignal>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdlib>

#define MAX_CHILDREN_DEF 500
#define MIN_CHILDREN_DEF 1

// Flaga kontrolująca działanie programu, liczba procesów potomnych
bool running = true;
int children = 0;
int max_children = 200;

// Obsługa sygnału SIGINT
void handle_sigint(int signo) {
    running = false;
    std::cout << warning << "\nClient Factory: Received SIGINT. Stopping client creation." << reset_color << std::endl;
}

// Funkcja obsługująca oczyszczanie procesów potomnych
void* wait_children(void *arg) {
    while (running) {
        int status;
        pid_t pid = waitpid(-1, &status, WNOHANG); // Sprawdzaj bez blokowania
        if (pid > 0) {
            children--;
            std::cout << info_alt << "Client Factory: Process (PID: " << pid << ") finished." << reset_color << std::endl;
        } else {
        }
    }

    // Po zatrzymaniu programu upewnij się, że wszystkie procesy są oczyszczone
    while (true) {
        int status;
        pid_t pid = waitpid(-1, &status, 0); // Blokujące oczekiwanie
        if (pid > 0) {
            children--;
            std::cout << info_alt << "Client Factory: Process (PID: " << pid << ") finished after shutdown." << reset_color <<std::endl;
        } else {
            break; // Brak więcej procesów potomnych
        }
    }

    return NULL;
}

int main(int argc, char* argv[]) {
    // Rejestracja handlera dla SIGINT
    signal(SIGINT, handle_sigint);

    if (argc != 2) {
        std::cout << warning << "Client Factory: Max children count not provided, using default value: " << max_children << reset_color << std::endl << std::endl;
    }
    else {
        try {
            max_children = std::stoi(argv[1]);
        } catch (const std::exception& e) {
            std::cout << warning << "Client Factory: Max children count parsing error, using default value: " << max_children << reset_color << std::endl << std::endl;
        }

        if (max_children > MAX_CHILDREN_DEF) {
            max_children = MAX_CHILDREN_DEF;
            std::cout << warning << "Client Factory: Max children count can not exceed max value: " << MAX_CHILDREN_DEF << ", using max value." << reset_color << std::endl << std::endl;
        }
        else if (max_children < MIN_CHILDREN_DEF) {
            max_children = MIN_CHILDREN_DEF;
            std::cout << warning << "Client Factory: Max children count can not be negative, using minimum value: " << MIN_CHILDREN_DEF << reset_color << std::endl << std::endl;
        }
    }

    // Wątek do oczyszczania procesów potomnych
    pthread_t wait_children_thread;
    pthread_create(&wait_children_thread, NULL, wait_children, NULL);

    std::cout << success_important << "Client Factory (" << getpid() << "): Running..." << reset_color << std::endl;

    while (running) {
        // Losowy czas oczekiwania między tworzeniem klientów (1-5 sekund)
        int sleep_time = rand() % 5 + 1;
        sleep(sleep_time);

        if (children >= max_children) {
            continue;
        }

        pid_t pid = fork();
        if (pid == 0) {
            // Proces klienta
            execl("./client", "./client", nullptr);
            perror("execl");
            exit(EXIT_FAILURE);
        } else if (pid > 0) {
            children++;
            std::cout << success << "Client Factory: Created client process (PID: " << pid << ")." << reset_color << std::endl;
        } else {
            perror("fork");
        }

    }

    std::cout << warning << "Client Factory: Stopping client creation. Waiting for active clients to finish." << reset_color << std::endl;

    // Dołączanie wątku odpowiedzialnego za czyszczenie procesów potomnych
    pthread_join(wait_children_thread, NULL);

    std::cout << success_important << "Client Factory: All clients have finished. Exiting." << reset_color << std::endl;
    return 0;
}
