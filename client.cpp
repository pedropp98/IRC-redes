#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define BUFFER_SIZE 4096

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        std::cerr << "Erro ao criar o socket" << std::endl;
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8080);
    if (inet_pton(AF_INET, "127.0.0.1", &(serverAddr.sin_addr)) <= 0) {
        std::cerr << "Endereço inválido" << std::endl;
        return 1;
    }

    if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Erro ao conectar ao servidor" << std::endl;
        return 1;
    }

    char buffer[BUFFER_SIZE];

    std::cout << "Digite seu apelido: ";
    std::string apelido;
    std::getline(std::cin, apelido);

    std::string message;

    while (true) {
        std::cout << apelido << ": ";
        std::getline(std::cin, message);

        if (message == "/quit") {
            break;
        }

        if (send(sock, message.c_str(), message.size(), 0) < 0) {
            std::cerr << "Erro ao enviar a mensagem" << std::endl;
            break;
        }

        memset(buffer, 0, BUFFER_SIZE);
        if (recv(sock, buffer, BUFFER_SIZE, 0) < 0) {
            std::cerr << "Erro ao receber a resposta do servidor" << std::endl;
            break;
        }

        std::cout << "Resposta do servidor: " << buffer << std::endl;
    }

    close(sock);
    return 0;
}
