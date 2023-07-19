#include <iostream>
#include <cstring>
#include <unordered_map>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <algorithm>

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
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
    std::string channelName;
    bool isConnected;
    bool isAdmin;
    int failedAttempts;
};

struct Channel {
    std::vector<std::string> users;
    std::vector<std::string> mutedUsers;
    std::vector<std::string> kickedUsers;
    std::string adminNickname;
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
    bool isMuted(const std::string& username, const std::string& channelName) {
        if (channels_.count(channelName) > 0) {
            const auto& channel = channels_[channelName];
            return std::find(channel.mutedUsers.begin(), channel.mutedUsers.end(), username) != channel.mutedUsers.end();
        }
        return false;
    }

    bool isKicked(const std::string& username, const std::string& channelName) {
        if (channels_.count(channelName) > 0) {
            const auto& channel = channels_[channelName];
            return std::find(channel.kickedUsers.begin(), channel.kickedUsers.end(), username) != channel.kickedUsers.end();
        }
        return false;
    }

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
            clients_[clientId] = { clientSocket, "", "", false, false, 0 };

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
                removeUserFromChannel(clients_[clientId].channelName, clientId);
                break;
            } else if (bytesRead == -1) {
                std::cerr << "Failed to receive message from client " << clients_[clientId].nickname << std::endl;
                break;
            }
            buffer[bytesRead] = '\0';

            std::string message(buffer);
            if (message.find("/nickname") == 0) {
                clients_[clientId].nickname = message.substr(10);
            } else if (message.find("/connect") == 0) {
                clients_[clientId].isConnected = true;
                std::cout << clients_[clientId].nickname << " connected." << std::endl;
                broadcastMessage(clients_[clientId].nickname + " connected.\n", clients_[clientId].channelName);
            } else if (message.find("/join") == 0) {
                std::string channelName = message.substr(6);
                if (!channelName.empty()) {
                    joinChannel(channelName, clientId);
                }
            } else if (!message.empty() && message[0] != '/' && clients_[clientId].isConnected && !isMuted(clients_[clientId].nickname, clients_[clientId].channelName) && !isKicked(clients_[clientId].nickname, clients_[clientId].channelName)) {
                std::cout << clients_[clientId].nickname << ": " << message << std::endl;
                broadcastMessage(clients_[clientId].nickname + ": " + message, clients_[clientId].channelName);
            } else if (message.find("/ping") == 0) {
                std::cout << "Server: pong" << std::endl;
                broadcastMessage("Server: pong", clients_[clientId].channelName);
            } else if (!message.empty() && !clients_[clientId].isConnected && !clients_[clientId].channelName.empty()) {
                // Check if the client is muted or kicked
                if (clients_[clientId].failedAttempts < 5) {
                    if (!sendMessage(clientSocket, "Please use the /connect command to establish a connection.")) {
                        std::cerr << "Failed to send message to client " << clientId << std::endl;
                    }
                    clients_[clientId].failedAttempts++;
                } else {
                    std::cout << "Connection closed with client " << clients_[clientId].nickname << " due to multiple failed attempts." << std::endl;
                    removeUserFromChannel(clients_[clientId].channelName, clientId);
                    break;
                }
            } else if (message.find("/kick") == 0) {
                std::string username = message.substr(6);
                if (clients_[clientId].isAdmin) {
                    kickUser(username, clients_[clientId].channelName, clientId);
                } else {
                    if (!sendMessage(clientSocket, "You don't have permission to use the /kick command.")) {
                        std::cerr << "Failed to send message to client " << clientId << std::endl;
                    }
                }
            } else if (message.find("/mute") == 0) {
                std::string username = message.substr(6);
                if (clients_[clientId].isAdmin) {
                    muteUser(username, clients_[clientId].channelName);
                } else {
                    if (!sendMessage(clientSocket, "You don't have permission to use the /mute command.")) {
                        std::cerr << "Failed to send message to client " << clientId << std::endl;
                    }
                }
            } else if (message.find("/unmute") == 0) {
                std::string username = message.substr(8);
                if (clients_[clientId].isAdmin) {
                    unmuteUser(username, clients_[clientId].channelName);
                } else {
                    if (!sendMessage(clientSocket, "You don't have permission to use the /unmute command.")) {
                        std::cerr << "Failed to send message to client " << clientId << std::endl;
                    }
                }
            } else if (message.find("/whois") == 0) {
                std::string username = message.substr(7);
                if (clients_[clientId].isAdmin) {
                    sendUserIP(username, clientSocket);
                } else {
                    if (!sendMessage(clientSocket, "You don't have permission to use the /whois command.")) {
                        std::cerr << "Failed to send message to client " << clientId << std::endl;
                    }
                }
            }
        }

        clients_.erase(clientId);

#ifdef _WIN32
        closesocket(clientSocket);
#else
        close(clientSocket);
#endif
    }

    void joinChannel(const std::string& channelName, const std::string& clientId) {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        if (channelNameToAdmin_.count(channelName) == 0) {
            channelNameToAdmin_[channelName] = clientId;
            clients_[clientId].isAdmin = true;
        }
        clients_[clientId].channelName = channelName;
        channels_[channelName].users.push_back(clients_[clientId].nickname);
        if (channels_[channelName].adminNickname.empty()) {
            channels_[channelName].adminNickname = clients_[clientId].nickname;
        }
        std::cout << clients_[clientId].nickname << " joined channel " << channelName << std::endl;
    }

    void removeUserFromChannel(const std::string& channelName, const std::string& clientId) {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        if (channels_.count(channelName) > 0) {
            auto& channel = channels_[channelName];
            auto it = std::find(channel.users.begin(), channel.users.end(), clients_[clientId].nickname);
            if (it != channel.users.end()) {
                channel.users.erase(it);
                if (channel.adminNickname == clients_[clientId].nickname) {
                    if (!channel.users.empty()) {
                        channel.adminNickname = channel.users.front();
                    } else {
                        channel.adminNickname.clear();
                    }
                }
            }
        }
    }

    void kickUser(const std::string& username, const std::string& channelName, const std::string& kickedClientId) {
        if (channels_.count(channelName) > 0) {
            auto& channel = channels_[channelName];
            if (std::find(channel.kickedUsers.begin(), channel.kickedUsers.end(), username) == channel.kickedUsers.end()) {
                channel.kickedUsers.push_back(username);
                std::cout << "User " << username << " has been kicked from the channel." << std::endl;
                removeUserFromChannel(channelName, kickedClientId);
                broadcastMessage("User " + username + " has been kicked from the channel.", channelName);
            }
        }
    }
            
    

    void muteUser(const std::string& username, const std::string& channelName) {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        if (channels_.count(channelName) > 0) {
            auto& channel = channels_[channelName];
            if (std::find(channel.mutedUsers.begin(), channel.mutedUsers.end(), username) == channel.mutedUsers.end()) {
                channel.mutedUsers.push_back(username);
                std::cout << "User " << username << " has been muted in channel " << channelName << std::endl;
            }
        }
    }

    void unmuteUser(const std::string& username, const std::string& channelName) {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        if (channels_.count(channelName) > 0) {
            auto& channel = channels_[channelName];
            auto it = std::find(channel.mutedUsers.begin(), channel.mutedUsers.end(), username);
            if (it != channel.mutedUsers.end()) {
                channel.mutedUsers.erase(it);
                std::cout << "User " << username << " has been unmuted in channel " << channelName << std::endl;
            }
        }
    }

    void sendUserIP(const std::string& username, int clientSocket) {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        for (const auto& client : clients_) {
            if (client.second.nickname == username) {
                std::string ip = getClientIP(client.second.socket);
                if (!sendMessage(clientSocket, "User " + username + " IP: " + ip)) {
                    std::cerr << "Failed to send IP information to the administrator" << std::endl;
                }
                break;
            }
        }
    }

    std::string getClientIP(int socket) {
        sockaddr_in addr{};
        socklen_t len = sizeof(addr);
        getpeername(socket, reinterpret_cast<struct sockaddr*>(&addr), &len);
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(addr.sin_addr), ip, INET_ADDRSTRLEN);
        return std::string(ip);
    }

    void broadcastMessage(const std::string& message, const std::string& channelName) {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        if (channels_.count(channelName) > 0) {
            auto& channel = channels_[channelName];
            for (const auto& client : clients_) {
                if (client.second.isConnected && client.second.channelName == channelName &&
                    //!isMuted(client.second.nickname, channelName) &&
                    !isKicked(client.second.nickname, channelName)) {
                    if (!sendMessage(client.second.socket, message)) {
                        std::cerr << "Failed to send message to client " << client.first << std::endl;
                    }
                }
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
    std::unordered_map<std::string, std::string> channelNameToAdmin_;
    std::unordered_map<std::string, Channel> channels_;
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

#ifdef _WIN32
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);
#else
    signal(SIGINT, CtrlHandler);
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
        if (input.find("/quit") != std::string::npos) {
            break;
        }
    }

    server.stop();

    return 0;
}
