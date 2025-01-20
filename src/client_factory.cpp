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

// Flaga kontrolująca działanie programu, liczba procesów potomnych, maksymalna liczba procesow potomnych
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

        // Zastosowanie oczekiwania bez blokowania
        pid_t pid = waitpid(-1, &status, WNOHANG); 
        if (pid > 0) {
            children--;
            std::cout << info_alt << "Client Factory: Process (PID: " << pid << ") finished." << reset_color << std::endl;
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
            children--;
            std::cout << info_alt << "Client Factory: Process (PID: " << pid << ") finished after shutdown." << reset_color <<std::endl;
        } else {
            // Brak więcej procesów potomnych
            break;
        }
    }

    return NULL;
}

int main(int argc, char* argv[]) {
    // Rejestracja funkcji obsługujących sygnały
    // Sygnał SIGINT
    sighandler_t sig;
    sig = signal(SIGINT, handle_sigint);
    if (sig == SIG_ERR) {
        perror("signal");
        std::cerr << "errno: " << errno << std::endl;
        return EXIT_FAILURE;
    }

    // Walidacja argumentu uruchomienia
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

    // Utworzenie wątku do oczyszczania procesów potomnych
    int res;
    pthread_t wait_children_thread;
    res = pthread_create(&wait_children_thread, NULL, wait_children, NULL);
    if (res != 0) {
        perror("pthread_create");
        std::cerr << "errno: " << errno << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << success_important << "Client Factory (" << getpid() << "): Running..." << reset_color << std::endl;

    while (running) {
        // Losowy czas oczekiwania między tworzeniem klientów (1-5 sekund)
        int sleep_time = rand() % 5 + 1;
        sleep(sleep_time);

        if (children >= max_children) {
            continue;
        }

        pid_t pid = fork();
        // Utowrzenie procesu klienta
        if (pid == 0) {
            execl("./client", "./client", nullptr);
            perror("execl");
            std::cerr << "errno: " << errno << std::endl;
            exit(EXIT_FAILURE);
        } else if (pid > 0) {
            children++;
            std::cout << success << "Client Factory: Created client process (PID: " << pid << ")." << reset_color << std::endl;
        } else {
            perror("fork");
            std::cerr << "errno: " << errno << std::endl;
        }

    }

    std::cout << warning << "Client Factory: Stopping client creation. Waiting for active clients to finish." << reset_color << std::endl;

    // Dołączanie wątku odpowiedzialnego za czyszczenie procesów potomnych
    res = pthread_join(wait_children_thread, NULL);
    if (res != 0) {
        perror("pthread_join");
        std::cerr << "errno: " << errno << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << success_important << "Client Factory: All clients have finished. Exiting." << reset_color << std::endl;
    return 0;
}
