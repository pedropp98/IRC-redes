all: server client

server: server.o
	g++ -std=c++17 -Wall -Wextra -pthread -o server server.o

client: client.o
	g++ -std=c++17 -Wall -Wextra -pthread -o client client.o

server.o: server.cpp
	g++ -std=c++17 -Wall -Wextra -pthread -c -o server.o server.cpp

client.o: client.cpp
	g++ -std=c++17 -Wall -Wextra -pthread -c -o client.o client.cpp

.PHONY: clean
clean:
	rm -f server client server.o client.o

run-server: server
	./server

run-client: client
	./client
