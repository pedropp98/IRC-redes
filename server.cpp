#include <iostream>
#include <cstring>
#include <vector>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define BUFFER_SIZE 4096
#define MAX_CLIENTS 10

struct ClientData {
    int sock;
    std::string apelido;
};

std::vector<ClientData> clients;
pthread_mutex_t clientsMutex = PTHREAD_MUTEX_INITIALIZER;

void *clientHandler(void *arg) {
    ClientData client = *((ClientData*)arg);
    char buffer[BUFFER_SIZE];

    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        if (recv(client.sock, buffer, BUFFER_SIZE, 0) <= 0) {
            pthread_mutex_lock(&clientsMutex);
            auto it = std::find_if(clients.begin(), clients.end(), [&](const ClientData& c) {
                return c.sock == client.sock;
            });

            if (it != clients.end()) {
                clients.erase(it);
                std::cout << "Cliente " << client.apelido << " desconectado." << std::endl;
            }

            pthread_mutex_unlock(&clientsMutex);
            break;
        }

        std::string message = client.apelido + ": " + buffer;

        pthread_mutex_lock(&clientsMutex);
        for (const auto& otherClient : clients) {
            if (otherClient.sock != client.sock) {
                if (send(otherClient.sock, message.c_str(), message.size(), 0) < 0) {
                    std::cerr << "Erro ao enviar a mensagem para o cliente" << std::endl;
                    break;
                }
            }
        }
        pthread_mutex_unlock(&clientsMutex);
    }

    close(client.sock);
    delete (ClientData*)arg;
    pthread_exit(nullptr);
}

int main() {
    int serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock == -1) {
        std::cerr << "Erro ao criar o socket do servidor" << std::endl;
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8080);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Erro ao realizar o bind do servidor" << std::endl;
        return 1;
    }

    if (listen(serverSock, MAX_CLIENTS) < 0) {
        std::cerr << "Erro ao iniciar a escuta do servidor" << std::endl;
        return 1;
    }

    std::cout << "Servidor aguardando conexões..." << std::endl;

    while (true) {
        sockaddr_in clientAddr{};
        socklen_t clientAddrSize = sizeof(clientAddr);

        int clientSock = accept(serverSock, (struct sockaddr*)&clientAddr, &clientAddrSize);
        if (clientSock < 0) {
            std::cerr << "Erro ao aceitar a conexão do cliente" << std::endl;
            continue;
        }

        pthread_mutex_lock(&clientsMutex);
        if (clients.size() >= MAX_CLIENTS) {
            std::cerr << "Número máximo de clientes atingido. Conexão rejeitada." << std::endl;
            close(clientSock);
            pthread_mutex_unlock(&clientsMutex);
            continue;
        }

        ClientData *clientData = new ClientData;
        clientData->sock = clientSock;
        clientData->apelido = std::to_string(clientSock);

        clients.push_back(*clientData);
        pthread_t threadId;
        pthread_create(&threadId, nullptr, clientHandler, (void*)clientData);
        pthread_detach(threadId);

        std::cout << "Novo cliente conectado: " << clientData->apelido << std::endl;
        pthread_mutex_unlock(&clientsMutex);
    }

    close(serverSock);
    return 0;
}
