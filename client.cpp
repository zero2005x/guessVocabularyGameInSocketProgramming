#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>

#pragma comment(lib, "Ws2_32.lib")

int main()
{
    WSADATA wsaData;
    SOCKET sock;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};

     // Initialize Winsock
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    sock = socket(AF_INET, SOCK_STREAM, 0);

    // Setup the client socket
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8080);
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

    bool gameEnded = false;

    while (true)
    {
        int bytesReceived = recv(sock, buffer, 1024, 0);
        if (bytesReceived <= 0)
        {
            std::cout << "Server has terminated the connection." << std::endl;
            break;
        }

           
        buffer[bytesReceived] = '\0';
        std::string response(buffer);

        // Ignore heartbeat acknowledgements
        if (response.find("heartbeat acknowledged") != std::string::npos)
        {
            continue;
        }

        std::cout << response << std::endl;

        // Check for game end or play again query
        if (response.find("Game over") != std::string::npos ||
            response.find("Congratulations") != std::string::npos ||
            response.find("Do you want to play again?") != std::string::npos)
        {

            std::cout << "Your answer (yes/no): ";
            std::string input;
            std::getline(std::cin, input);
            send(sock, input.c_str(), input.length(), 0);

            if (input == "no")
                break; // Exit loop if player chooses not to continue
        }
        else
        {
            // Regular guess input
            std::cout << "Guess a letter: ";
            std::string input;
            std::getline(std::cin, input);
            if (!input.empty())
            {
                char guess = input[0];
                send(sock, &guess, 1, 0);
            }
            else
            {
                std::cout << "You did not enter a letter. Please try again." << std::endl;
            }

             // Send a heartbeat message
            std::string heartbeat = "heartbeat";
            send(sock, heartbeat.c_str(), heartbeat.size(), 0);
        }
    }
    // Cleanup
    closesocket(sock);
    WSACleanup();
    return 0;
}
