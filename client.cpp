#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

int main() {
    // Criação do socket
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        std::cerr << "Erro ao criar o socket";
        return 1;
    }

    // Configuração do endereço do servidor
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(12345); // Porta do servidor

    // Conexão ao servidor
    if (connect(clientSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) < 0) {
        std::cerr << "Erro ao conectar ao servidor";
        return 1;
    }

    std::cout << "Conectado ao servidor!" << std::endl;

    // Recebe e envia mensagens
    while (true) {
        std::cout << "Digite sua mensagem: ";
        std::string message;
        std::getline(std::cin, message);

        int bytesSent = send(clientSocket, message.c_str(), message.size(), 0);
        if (bytesSent <= 0) {
            std::cerr << "Erro no envio da mensagem";
            break;
        }

        char receivedMessage[4096];
        int bytesRead = recv(clientSocket, receivedMessage, sizeof(receivedMessage), 0);
        if (bytesRead <= 0) {
            std::cerr << "Erro na recepção da mensagem";
            break;
        }

        receivedMessage[bytesRead] = '\0';
        std::cout << "Servidor: " << receivedMessage << std::endl;
    }

    // Encerramento da conexão
    close(clientSocket);

    return 0;
}
