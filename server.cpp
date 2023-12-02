#include <iostream>
#include <vector>
#include <string>
#include <ctime>
#include <cstdlib>
#include <fstream>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

int main()
{
    WSADATA wsaData;
    SOCKET serverSocket, clientSocket;
    struct sockaddr_in server, client;
    char buffer[1024] = {0};

    // Initialize Winsock
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    // Setup the TCP listening socket
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(8080);

    // Bind the socket
    bind(serverSocket, (struct sockaddr *)&server, sizeof(server));
    // Start listening for client connections
    listen(serverSocket, 3);

    // Accept a client socket
    int c = sizeof(struct sockaddr_in);
    clientSocket = accept(serverSocket, (struct sockaddr *)&client, &c);
    bool keepPlaying = true;

    // Main game loop
    while (keepPlaying)
    {
        // Load the list of words
        std::vector<std::string> words;
        std::string word;
        std::ifstream file("dictionary.txt");
        while (getline(file, word))
        {
            words.push_back(word);
        }
        file.close();

        // Choose a random word
        srand(time(NULL));
        std::string selectedWord = words[rand() % words.size()];
        std::string displayWord(selectedWord.length(), '_');

        // Set initial number of guesses
        int guessesLeft = selectedWord.length() * 2; // 剩餘猜測次數
        send(clientSocket, displayWord.c_str(), displayWord.size(), 0);

        // Loop for guessing the word
        while (guessesLeft > 0 && displayWord != selectedWord)
        {
            int bytesReceived = recv(clientSocket, buffer, 1024, 0);
            if (bytesReceived == SOCKET_ERROR)
            {
                std::cerr << "Error in recv(): " << WSAGetLastError() << std::endl;
                break;
            }
            else if (bytesReceived == 0)
            {
                std::cout << "Client disconnected" << std::endl;
                break;
            }

            if (bytesReceived < 0)
            {
                break;
            }

            buffer[bytesReceived] = '\0';
            std::string receivedMsg(buffer);

            // Check for heartbeat
            if (receivedMsg == "heartbeat")
            {
                std::string response = "heartbeat acknowledged\n";
                if (send(clientSocket, response.c_str(), response.size(), 0) == SOCKET_ERROR)
                {
                    std::cerr << "Error in send(): " << WSAGetLastError() << std::endl;
                    break;
                }
                continue;
            }

            // Process the guess
            char guessedLetter = buffer[0];
            bool letterFound = false;
            for (size_t i = 0; i < selectedWord.length(); i++)
            {
                if (selectedWord[i] == guessedLetter)
                {
                    displayWord[i] = guessedLetter;
                    letterFound = true;
                }
            }

            if (!letterFound)
            {
                guessesLeft--; // let the guessLeft number down
            }

            if (guessesLeft == 0 || displayWord == selectedWord)
            {
                break; // End the loop if the word is guessed or guesses run out
            }

            // Send the current state of the word
            std::string message = displayWord + " Guesses left: " + std::to_string(guessesLeft) + "\n";
             if (send(clientSocket, message.c_str(), message.size(), 0) == SOCKET_ERROR)
            {
                std::cerr << "Error in send(): " << WSAGetLastError() << std::endl;
                break;
            }
        }
        // Send end game message
        std::string endGameMsg;
        if (displayWord == selectedWord)
        {
            endGameMsg = "Congratulations! You guessed the word correctly! The word was '" + selectedWord + "'.\n";
        }
        else
        {
            endGameMsg = "Game over! You've run out of guesses. The word was '" + selectedWord + "'.\n";
        }

        send(clientSocket, endGameMsg.c_str(), endGameMsg.size(), 0);

        // Ask if the client wants to play again
        std::string continueMsg = "Do you want to play again? (yes/no)\n";
        send(clientSocket, continueMsg.c_str(), continueMsg.size(), 0);

        int bytesReceived = recv(clientSocket, buffer, 1024, 0);
        if (bytesReceived <= 0)
        {
            break; // Break if the client disconnects or an error occurs
        }

        buffer[bytesReceived] = '\0';
        std::string response(buffer);
        if (response.find("no") != std::string::npos)
        {
            keepPlaying = false;
        }
    }

    // Cleanup
    closesocket(clientSocket);
    WSACleanup();
    return 0;
}
