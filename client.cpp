#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

const int MAX_MESSAGE_LENGTH = 4096;

int main() {
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        std::cerr << "Erro ao criar o socket" << std::endl;
        return 1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(12345);

    if (inet_pton(AF_INET, "127.0.0.1", &(serverAddress.sin_addr)) <= 0) {
        std::cerr << "Endereço inválido" << std::endl;
        return 1;
    }

    if (connect(clientSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) < 0) {
        std::cerr << "Erro na conexão" << std::endl;
        return 1;
    }

    std::cout << "Conexão estabelecida. Digite seu apelido:" << std::endl;
    std::string nickname;
    std::getline(std::cin, nickname);

    send(clientSocket, nickname.c_str(), nickname.size(), 0);

    char buffer[MAX_MESSAGE_LENGTH];

    while (true) {
        std::string message;
        std::getline(std::cin, message);

        if (message == "/quit") {
            send(clientSocket, message.c_str(), message.size(), 0);
            break;
        }

        send(clientSocket, message.c_str(), message.size(), 0);

        int bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesRead <= 0) {
            std::cerr << "Conexão encerrada pelo servidor" << std::endl;
            break;
        }

        buffer[bytesRead] = '\0';
        std::cout << "Resposta do servidor: " << buffer << std::endl;
    }

    close(clientSocket);

    return 0;
}
