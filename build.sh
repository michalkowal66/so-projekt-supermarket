g++ ./src/manager.cpp ./src/shared.cpp ./src/ccol.cpp -lpthread -o ./bin/manager
g++ ./src/cashier.cpp ./src/shared.cpp ./src/ccol.cpp -lpthread -o ./bin/cashier
g++ ./src/client.cpp ./src/shared.cpp ./src/ccol.cpp -lpthread -o ./bin/client
g++ ./src/fireman.cpp ./src/shared.cpp ./src/ccol.cpp -lpthread -o ./bin/fireman
g++ ./src/client_factory.cpp ./src/ccol.cpp -lpthread -o ./bin/client_factory

