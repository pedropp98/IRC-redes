CC = g++
CFLAGS = -std=c++11 -Wall -Wextra -pthread
TARGETS = cliente servidor

all: $(TARGETS)

cliente: Cliente.cpp
	$(CC) $(CFLAGS) -o cliente Cliente.cpp

servidor: Servidor.cpp
	$(CC) $(CFLAGS) -o servidor Servidor.cpp

clean:
	rm -f $(TARGETS)
