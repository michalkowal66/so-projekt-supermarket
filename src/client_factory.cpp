#include <signal.h>
#include <unistd.h>
#include <cstdlib>
#include <iostream>
#include <algorithm>
#include <sys/wait.h>

bool running = true; // Flaga dla głównej pętli

// Obsługa sygnału SIGINT (Ctrl+C)
void handle_sigint(int signo) {
    running = false;
    std::cout << "\nClient Factory: Received SIGINT. Stopping client creation.\n";
}

int main() {
    // Rejestracja handlera sygnału
    signal(SIGINT, handle_sigint);

    srand(time(nullptr) ^ getpid());

    std::cout << "Client Factory: Running...\n";

    while (running) {
        sleep(rand() % 5 + 1); // Tworzenie klienta co 1-5 sekund

        pid_t pid = fork();
        if (pid == 0) {
            // Proces klienta
            execl("./client", "./client", nullptr);
            perror("execl");
            exit(EXIT_FAILURE);
        } else if (pid > 0) {
            // Proces nadrzędny
            std::cout << "Client Factory: Created client process (PID: " << pid << ").\n";
        } else {
            perror("fork");
        }
    }

    std::cout << "Client Factory: Stopping client creation.\n";

    return 0;
}
