#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <algorithm>
#include <thread>
#include <mutex>

const int MAX_MESSAGE_LENGTH = 4096;
const int MAX_RETRIES = 5;

std::mutex clientsMutex;

struct Client {
    int socket;
    std::string nickname;
    bool connected;
    int retries;

    Client(int socket) : socket(socket), connected(true), retries(0) {}
};

std::vector<Client> clients;

void broadcastMessage(const std::string& message, int senderSocket) {
    std::lock_guard<std::mutex> lock(clientsMutex);

    for (const auto& client : clients) {
        if (client.connected && client.socket != senderSocket) {
            std::string fullMessage = client.nickname + ": " + message + "\n";
            send(client.socket, fullMessage.c_str(), fullMessage.size(), 0);
        }
    }
}

void clientHandler(int clientSocket) {
    char buffer[MAX_MESSAGE_LENGTH];

    std::string nickname;

    while (true) {
        int bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesRead <= 0) {
            if (!nickname.empty()) {
                std::lock_guard<std::mutex> lock(clientsMutex);
                auto clientIt = std::find_if(clients.begin(), clients.end(),
                    [clientSocket](const Client& client) {
                        return client.socket == clientSocket;
                    });

                if (clientIt != clients.end()) {
                    Client& client = *clientIt;
                    if (client.connected) {
                        std::cout << "Cliente desconectado: " << client.nickname << std::endl;
                        client.connected = false;
                    }
                }
            }
            break;
        }

        buffer[bytesRead] = '\0';

        std::string message(buffer);

        if (message == "/quit") {
            if (!nickname.empty()) {
                std::cout << "Cliente fechou a conexão: " << nickname << std::endl;
                std::lock_guard<std::mutex> lock(clientsMutex);
                auto clientIt = std::find_if(clients.begin(), clients.end(),
                    [clientSocket](const Client& client) {
                        return client.socket == clientSocket;
                    });

                if (clientIt != clients.end()) {
                    Client& client = *clientIt;
                    if (client.connected) {
                        client.connected = false;
                        close(client.socket);
                    }
                }
            }
            break;
        }

        if (nickname.empty()) {
            nickname = message;
            std::cout << "Cliente conectado: " << nickname << std::endl;

            std::lock_guard<std::mutex> lock(clientsMutex);
            clients.emplace_back(clientSocket);
        } else {
            if (message == "/ping") {
                std::string pongMessage = "pong\n";
                send(clientSocket, pongMessage.c_str(), pongMessage.size(), 0);
            } else {
                broadcastMessage(nickname + ": " + message, clientSocket);
            }
        }
    }

    close(clientSocket);
}

int main() {
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        std::cerr << "Erro ao criar o socket" << std::endl;
        return 1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(12345);

    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) < 0) {
        std::cerr << "Erro ao vincular o socket à porta" << std::endl;
        return 1;
    }

    if (listen(serverSocket, SOMAXCONN) < 0) {
        std::cerr << "Erro na escuta por conexões" << std::endl;
        return 1;
    }

    std::cout << "Aguardando conexões de clientes..." << std::endl;

    while (true) {
        sockaddr_in clientAddress{};
        socklen_t clientAddressLength = sizeof(clientAddress);
        int clientSocket = accept(serverSocket, reinterpret_cast<sockaddr*>(&clientAddress), &clientAddressLength);
        if (clientSocket < 0) {
            std::cerr << "Erro ao aceitar a conexão" << std::endl;
            return 1;
        }

        std::thread clientThread(clientHandler, clientSocket);
        clientThread.detach();
    }

    close(serverSocket);

    return 0;
}
