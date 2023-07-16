#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

int main() {
    // Criação do socket
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        std::cerr << "Erro ao criar o socket";
        return 1;
    }

    // Configuração do endereço do servidor
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(12345); // Porta do servidor

    // Vinculação do socket à porta
    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) < 0) {
        std::cerr << "Erro ao vincular o socket à porta";
        return 1;
    }

    // Espera por conexões de clientes
    if (listen(serverSocket, 1) < 0) {
        std::cerr << "Erro na escuta por conexões";
        return 1;
    }

    std::cout << "Aguardando conexão do cliente..." << std::endl;

    // Aceita uma conexão de cliente
    sockaddr_in clientAddress{};
    socklen_t clientAddressLength = sizeof(clientAddress);
    int clientSocket = accept(serverSocket, reinterpret_cast<sockaddr*>(&clientAddress), &clientAddressLength);
    if (clientSocket < 0) {
        std::cerr << "Erro ao aceitar a conexão";
        return 1;
    }

    std::cout << "Cliente conectado!" << std::endl;

    // Recebe e envia mensagens
    while (true) {
        char receivedMessage[4096];
        int bytesRead = recv(clientSocket, receivedMessage, sizeof(receivedMessage), 0);
        if (bytesRead <= 0) {
            std::cerr << "Erro na recepção da mensagem";
            break;
        }

        receivedMessage[bytesRead] = '\0';
        std::cout << "Cliente: " << receivedMessage << std::endl;

        std::cout << "Digite sua mensagem: ";
        std::string message;
        std::getline(std::cin, message);

        int bytesSent = send(clientSocket, message.c_str(), message.size(), 0);
        if (bytesSent <= 0) {
            std::cerr << "Erro no envio da mensagem";
            break;
        }
    }

    // Encerramento da conexão
    close(clientSocket);
    close(serverSocket);

    return 0;
}
