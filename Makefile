CC := g++
CFLAGS := -std=c++11

all: server client

server: server.cpp
	$(CC) $(CFLAGS) -o server server.cpp

client: client.cpp
	$(CC) $(CFLAGS) -o client client.cpp

.PHONY: clean

clean:
	rm -f server client