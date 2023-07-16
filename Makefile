CC := g++
CFLAGS := -std=c++11

all: server client

server: server.cpp
	$(CC) $(CFLAGS) -o server server.cpp

client: client.cpp
	$(CC) $(CFLAGS) -o client client.cpp

run-server:
	./server 
 
run-client:
	./client

.PHONY: clean

clean:
	rm -f server client