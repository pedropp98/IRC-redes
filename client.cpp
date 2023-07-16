#include <iostream>
#include <cstring>
#include <thread>

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

void receiveMessages(int socket) {
    char buffer[MAX_MESSAGE_LENGTH] = {0};
    while (true) {
        int bytesRead = recv(socket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead == 0) {
            std::cout << "Server disconnected" << std::endl;
            break;
        } else if (bytesRead == -1) {
            std::cerr << "Failed to receive message from server" << std::endl;
            break;
        }
        buffer[bytesRead] = '\0';

        std::string receivedMessage(buffer);
        std::cout << receivedMessage << std::endl;
    }
}

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

    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        std::cerr << "Failed to create client socket" << std::endl;
        return 1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(serverPort);
    serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(clientSocket, reinterpret_cast<struct sockaddr*>(&serverAddress), sizeof(serverAddress)) == -1) {
        std::cerr << "Failed to connect to server" << std::endl;
#ifdef _WIN32
        closesocket(clientSocket);
#else
        close(clientSocket);
#endif
        return 1;
    }

    std::cout << "Connected to server" << std::endl;

    std::thread receiveThread(receiveMessages, clientSocket);

    std::string nickname;
    std::cout << "Enter your nickname: ";
    std::cin.ignore(); // Ignore newline character from previous input
    std::getline(std::cin, nickname);
    std::string nicknameCommand = "/nickname " + nickname + '\n';

    if (send(clientSocket, nicknameCommand.c_str(), nicknameCommand.length(), 0) == -1) {
        std::cerr << "Failed to send nickname to server" << std::endl;
        receiveThread.join();
#ifdef _WIN32
        closesocket(clientSocket);
#else
        close(clientSocket);
#endif
        return 1;
    }

    std::string message;
    while (true) {
        std::getline(std::cin, message);

        if (message == "/quit") {
            break;
        }

        message = nickname + ": " + message + '\n';

        if (send(clientSocket, message.c_str(), message.length(), 0) == -1) {
            std::cerr << "Failed to send message to server" << std::endl;
            break;
        }
    }

    receiveThread.join();

#ifdef _WIN32
    closesocket(clientSocket);
    WSACleanup();
#else
    close(clientSocket);
#endif

    return 0;
}
