#!/bin/bash

# Flaga sterująca działaniem skryptu
running=true

# Lista PID-ów klientów
client_pids=()

# Funkcja obsługi sygnału SIGINT (Ctrl+C)
handle_sigint() {
    echo -e "\nClient Factory: Received SIGINT. Stopping client creation."
    running=false
}

# Rejestracja handlera dla SIGINT
trap handle_sigint SIGINT

echo "Client Factory: Running..."

# Główna pętla tworząca procesy klientów
while $running; do
    # Losowy czas oczekiwania między kolejnymi klientami (1-5 sekund)
    sleep_time=$((RANDOM % 5 + 1))
    sleep "$sleep_time"

    # Tworzenie klienta w tle i dopisanie od listy
    ../bin/client &
    client_pid=$!
    client_pids+=($client_pid)
    echo "Client Factory: Created client process (PID: $client_pid)."
done

echo "Client Factory: Stopping client creation. Waiting for active clients to finish."

# Upewnienie się, że procesy klientów zakończyły pracę
for pid in "${client_pids[@]}"; do
    if kill -0 "$pid" 2>/dev/null; then
        wait "$pid"
        echo "Client Factory: Client process (PID: $pid) finished."
    fi
done

echo "Client Factory: All clients have finished. Exiting."
