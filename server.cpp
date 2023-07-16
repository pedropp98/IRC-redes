#include <iostream>
#include <cstring>
#include <thread>
#include <vector>
#include <map>
#include <atomic>
#include <csignal>
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

std::atomic<bool> g_running(true);

#ifdef _WIN32
BOOL CtrlHandler(DWORD fdwCtrlType) {
    if (fdwCtrlType == CTRL_C_EVENT) {
        g_running = false;
        return TRUE;
    }
    return FALSE;
}
#else
void CtrlHandler(int) {
    g_running = false;
}
#endif

class Client {
public:
    Client(int socket) : socket_(socket), nickname_("") {}

    int getSocket() const {
        return socket_;
    }

    std::string getNickname() const {
        return nickname_;
    }

    void setNickname(const std::string& nickname) {
        nickname_ = nickname;
    }

    bool sendMessage(const std::string& message) {
        if (send(socket_, message.c_str(), message.length(), 0) == -1) {
            std::cerr << "Failed to send message to client" << std::endl;
            return false;
        }
        return true;
    }

private:
    int socket_;
    std::string nickname_;
};

class Server {
public:
    Server(int port) : serverSocket_(0), port_(port) {}

    bool start() {
        if (!createServerSocket()) {
            return false;
        }

        if (!bindServerSocket()) {
            return false;
        }

        if (!listenForClients()) {
            return false;
        }

        std::cout << "Server started on port " << port_ << std::endl;

        return true;
    }

    void acceptClients() {
        while (g_running) {
            int clientSocket = accept(serverSocket_, nullptr, nullptr);
            if (clientSocket == -1) {
                std::cerr << "Failed to accept client connection" << std::endl;
                continue;
            }

            std::thread clientThread(&Server::handleClient, this, clientSocket);
            clientThread.detach();
        }
    }

    void stop() {
        g_running = false;
#ifdef _WIN32
        closesocket(serverSocket_);
        WSACleanup();
#else
        close(serverSocket_);
#endif
    }

private:
    bool createServerSocket() {
#ifdef _WIN32
        WSADATA wsData;
        if (WSAStartup(MAKEWORD(2, 2), &wsData) != 0) {
            std::cerr << "Failed to initialize winsock" << std::endl;
            return false;
        }
#endif
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
        serverAddress.sin_addr.s_addr = INADDR_ANY;

        if (bind(serverSocket_, reinterpret_cast<struct sockaddr*>(&serverAddress), sizeof(serverAddress)) == -1) {
            std::cerr << "Failed to bind server socket" << std::endl;
            return false;
        }
        return true;
    }

    bool listenForClients() {
        if (listen(serverSocket_, SOMAXCONN) == -1) {
            std::cerr << "Failed to listen for clients" << std::endl;
            return false;
        }
        return true;
    }

    void handleClient(int clientSocket) {
        Client client(clientSocket);

        char buffer[MAX_MESSAGE_LENGTH] = {0};
        while (g_running) {
            int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
            if (bytesRead == 0) {
                std::cout << "Client disconnected: " << client.getNickname() << std::endl;
                break;
            } else if (bytesRead == -1) {
                std::cerr << "Failed to receive message from client: " << client.getNickname() << std::endl;
                break;
            }
            buffer[bytesRead] = '\0';

            std::string message(buffer);

            if (message.find("/nickname") == 0) {
                std::string nickname = message.substr(10);
                client.setNickname(nickname);
                std::cout << "Client connected: " << client.getNickname() << std::endl;
                continue;
            }

            std::cout << client.getNickname() << ": " << message << std::endl;

            broadcastMessage(client.getNickname() + ": " + message);
        }

        close(clientSocket);
    }

    void broadcastMessage(const std::string& message) {
        std::lock_guard<std::mutex> lock(clientsMutex_);

        for (auto& [_, client] : clients_) {
            if (!client.sendMessage(message)) {
                std::cout << "Failed to send message to client: " << client.getNickname() << std::endl;
            }
        }
    }

private:
    int serverSocket_;
    int port_;
    std::map<int, Client> clients_;
    std::mutex clientsMutex_;
};

int main() {
    int serverPort;
    std::cout << "Enter server port: ";
    std::cin >> serverPort;

    Server server(serverPort);

    if (!server.start()) {
        std::cerr << "Failed to start the server" << std::endl;
        return 1;
    }

#ifdef _WIN32
    if (SetConsoleCtrlHandler(reinterpret_cast<PHANDLER_ROUTINE>(CtrlHandler), TRUE) == 0) {
        std::cerr << "Failed to set CtrlHandler" << std::endl;
        return 1;
    }
#else
    signal(SIGINT, CtrlHandler);
#endif

    std::thread acceptThread(&Server::acceptClients, &server);
    acceptThread.join();

    server.stop();

    return 0;
}
