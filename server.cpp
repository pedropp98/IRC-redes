#include <iostream>
#include <cstring>
#include <unordered_map>
#include <thread>
#include <mutex>

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

constexpr int MAX_MESSAGE_LENGTH = 4096;

#ifdef _WIN32
BOOL CtrlHandler(DWORD fdwCtrlType) {
    if (fdwCtrlType == CTRL_C_EVENT) {
        return TRUE;
    }
    return FALSE;
}
#else
void CtrlHandler(int) {}
#endif

struct Client {
    int socket;
    std::string nickname;
};

class Server {
public:
    Server(int port) : serverSocket_(0), port_(port), nextClientId_(1), running_(false) {}

    ~Server() {
        stop();
    }

    bool start() {
        if (running_) {
            std::cerr << "Server is already running" << std::endl;
            return false;
        }

        if (!createServerSocket()) {
            return false;
        }

        if (!bindServerSocket()) {
            return false;
        }

        if (!listenForClients()) {
            return false;
        }

        running_ = true;

        std::cout << "Server started on port " << port_ << std::endl;

        acceptClients();

        return true;
    }

    void stop() {
        if (!running_) {
            return;
        }

        running_ = false;

        for (auto& client : clients_) {
#ifdef _WIN32
            closesocket(client.second.socket);
#else
            close(client.second.socket);
#endif
        }

#ifdef _WIN32
        closesocket(serverSocket_);
        WSACleanup();
#else
        close(serverSocket_);
#endif
    }

private:
    bool createServerSocket() {
        serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket_ == -1) {
            std::cerr << "Failed to create server socket" << std::endl;
            return false;
        }
        return true;
    }

    bool bindServerSocket() {
        sockaddr_in serverAddress{};
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_port = htons(port_);
        serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(serverSocket_, reinterpret_cast<struct sockaddr*>(&serverAddress), sizeof(serverAddress)) == -1) {
            std::cerr << "Failed to bind server socket" << std::endl;
#ifdef _WIN32
            closesocket(serverSocket_);
#else
            close(serverSocket_);
#endif
            return false;
        }

        return true;
    }

    bool listenForClients() {
        if (listen(serverSocket_, SOMAXCONN) == -1) {
            std::cerr << "Failed to listen for clients" << std::endl;
#ifdef _WIN32
            closesocket(serverSocket_);
#else
            close(serverSocket_);
#endif
            return false;
        }

        return true;
    }

    void acceptClients() {
        while (running_) {
            int clientSocket = accept(serverSocket_, nullptr, nullptr);
            if (clientSocket == -1) {
                std::cerr << "Failed to accept client connection" << std::endl;
                continue;
            }

            std::string clientId = "client_" + std::to_string(nextClientId_++);
            clients_[clientId] = { clientSocket, "" };

            std::thread clientThread(&Server::handleClient, this, clientSocket, clientId);
            clientThread.detach();
        }
    }

    void handleClient(int clientSocket, const std::string& clientId) {
        char buffer[MAX_MESSAGE_LENGTH] = { 0 };

        while (running_) {
            int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
            if (bytesRead == 0) {
                std::cout << "Client " << clients_[clientId].nickname << " disconnected" << std::endl;
                break;
            } else if (bytesRead == -1) {
                std::cerr << "Failed to receive message from client " << clients_[clientId].nickname << std::endl;
                break;
            }
            buffer[bytesRead] = '\0';

            std::string message(buffer);
            if (!message.empty() && message[0] != '/') {
                std::cout << clients_[clientId].nickname << ": " << message << std::endl;
                broadcastMessage(clients_[clientId].nickname + ": " + message);
            } else if (message.find("/nickname") == 0) {
                clients_[clientId].nickname = message.substr(10);
            }
        }

        clients_.erase(clientId);

#ifdef _WIN32
        closesocket(clientSocket);
#else
        close(clientSocket);
#endif
    }

    void broadcastMessage(const std::string& message) {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        for (const auto& client : clients_) {
            //std::cout << client.second.nickname << sendMessage(client.second.socket, message) << std::endl;
            if (!sendMessage(client.second.socket, message)) {
                std::cerr << "Failed to send message to client " << client.first << std::endl;
            }
        }
    }

    bool sendMessage(int socket, const std::string& message) {
        if (send(socket, message.c_str(), message.length(), 0) == -1) {
            return false;
        }
        return true;
    }

private:
    int serverSocket_;
    int port_;
    int nextClientId_;
    std::unordered_map<std::string, Client> clients_;
    std::mutex clientsMutex_;
    bool running_;
};

int main() {
#ifdef _WIN32
    WSADATA wsData;
    if (WSAStartup(MAKEWORD(2, 2), &wsData) != 0) {
        std::cerr << "Failed to initialize winsock" << std::endl;
        return 1;
    }
#endif

    int serverPort;
    std::cout << "Enter server port: ";
    std::cin >> serverPort;

    Server server(serverPort);
    server.start();

    std::string input;
    std::cout << "Type '/quit' to stop the server" << std::endl;
    while (true) {
        std::getline(std::cin, input);
        if (input == "/quit") {
            break;
        }
    }

    server.stop();

    return 0;
}
