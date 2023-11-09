#include "TFTPServer.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <fstream>

// using namespace std;

TFTPServer::TFTPServer(int port) : port(port), nextClientId(1) {
    serverSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (serverSocket < 0) {
        std::cerr << "Error creating server socket" << std::endl;
        exit(1);
    }

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverAddress.sin_port = htons(port);
    if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        std::cerr << "Error binding server socket" << std::endl;
        exit(1);
    }
    std::cout << "Server binded to port 69" << std::endl;
}


void TFTPServer::handleRequest(int clientSocket, struct sockaddr_in clientAddress) {
    char buffer[1024];
    int bytesRead = recvfrom(clientSocket, buffer, sizeof(buffer), 0, nullptr, nullptr);
    if (bytesRead < 0) {
        std::cerr << "Error receiving data" << std::endl;
        return;
    }

    // Extract the opcode from the received packet
    uint16_t opcode = (buffer[0] << 8) | buffer[1];

    // Handle RRQ request (Opcode 1)
    if (opcode == 1) {
        std::string filename(buffer + 2);
        // handleReadRequest(clientSocket, filename, clientAddress);
    }
    // Handle WRQ request (Opcode 2)
    else if (opcode == 2) {
        std::string filename(buffer + 2);
        // handleWriteRequest(clientSocket, filename, clientAddress);
    }
}

void TFTPServer::handleWriteRequest(int clientSocket, const std::string& filename, struct sockaddr_in clientAddress, int clientId) {
    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        // Send an error packet (Access violation - Error Code 2)
        sendError(clientSocket, 2, "Access violation", clientAddress);
        return;
    }
   
    // TODO: Handle the write operation

    file.close();
}

void TFTPServer::sendError(int clientSocket, uint16_t errorCode, const std::string& errorMsg, struct sockaddr_in clientAddress) {
    uint16_t opcode = 5; // ERROR opcode
    char packet[4 + errorMsg.size() + 1];
    packet[0] = opcode >> 8;
    packet[1] = opcode;
    packet[2] = errorCode >> 8;
    packet[3] = errorCode;
    strcpy(packet + 4, errorMsg.c_str());
    packet[4 + errorMsg.size()] = '\0';

    sendto(clientSocket, packet, 4 + errorMsg.size() + 1, 0, (struct sockaddr*)&clientAddress, sizeof(clientAddress));
}

void TFTPServer::handleReadRequest(int clientSocket, const std::string& filename, struct sockaddr_in clientAddress, int clientId) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        uint8_t packet[516];
        memset(packet, 0, sizeof(packet));
        // Send an error packet (File not found - Error Code 1)
        const std::string errorMessage = "File not found";
        TFTPPacket::createErrorPacket(packet, 1, errorMessage);
        packet[4 + errorMessage.size()] = '\0';
        size_t packetSize = 4 + errorMessage.size() + 1;
        sendto(clientSocket, packet, packetSize, 0, (struct sockaddr*)&clientAddress, sizeof(clientAddress));
        return;
    }

    // Read and send data in blocks of 512 bytes
    uint16_t blockNumber = 1;
    char dataBuffer[512];
    while (true) {
        file.read(dataBuffer, 512);
        int dataSize = file.gcount();
        if (dataSize == 0) {
            break; // End of file
        }

        // Send data packet
        sendData(clientSocket, dataBuffer, dataSize, clientAddress);

        // Wait for acknowledgment
        char ackBuffer[4];
        int ackSize = recvfrom(clientSocket, ackBuffer, sizeof(ackBuffer), 0, nullptr, nullptr);
        if (ackSize < 0) {
            std::cerr << "Error receiving acknowledgment" << std::endl;
            return;
        }

        // Check the received acknowledgment packet (Opcode 4)
        uint16_t ackOpcode = (ackBuffer[0] << 8) | ackBuffer[1];
        uint16_t ackBlockNumber = (ackBuffer[2] << 8) | ackBuffer[3];
        if (ackOpcode != 4 || ackBlockNumber != blockNumber) {
            std::cerr << "Invalid acknowledgment received" << std::endl;
            return;
        }

        ++blockNumber;
    }

    file.close();
}

void TFTPServer::sendData(int clientSocket, const char* data, int dataSize, struct sockaddr_in clientAddress) {
    uint16_t opcode = 3; // DATA opcode
    uint16_t blockNumber = htons(nextClientId); // Adjust the block number if needed
    char packet[516];
    packet[0] = opcode >> 8;
    packet[1] = opcode;
    packet[2] = blockNumber >> 8;
    packet[3] = blockNumber;
    memcpy(packet + 4, data, dataSize);
    std::cerr << "Sending this packet: " << packet << std::endl;
    sendto(clientSocket, packet, dataSize + 4, 0, (struct sockaddr*)&clientAddress, sizeof(clientAddress));
}

void TFTPServer::sendACK(int clientSocket, uint16_t blockNumber, struct sockaddr_in clientAddress) {
    uint16_t opcode = 4; // ACK opcode
    char packet[4];
    packet[0] = opcode >> 8;
    packet[1] = opcode;
    packet[2] = blockNumber >> 8;
    packet[3] = blockNumber;

    sendto(clientSocket, packet, 4, 0, (struct sockaddr*)&clientAddress, sizeof(clientAddress));
}

void TFTPServer::handleClientThread(struct sockaddr_in clientAddress, int clientId, uint16_t opcode, const std::string& filename, std::map<int, std::tuple<std::thread, bool>>& clientThreads) {
    int serverThreadSocket = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in serverThreadSocketAddr;
    serverThreadSocketAddr.sin_family = AF_INET;
    serverThreadSocketAddr.sin_port = htons(clientId);
    serverThreadSocketAddr.sin_addr.s_addr = inet_addr("127.0.0.1");   // INADDR_ANY;
    if (bind(serverThreadSocket, (struct sockaddr*)&serverThreadSocketAddr, sizeof(serverThreadSocketAddr)) < 0) {
        std::cerr << "Error binding server socket" << std::endl;
        exit(1);
    }
    std::cout << "Server binded to port " << clientId << std::endl;

    // Handle RRQ request (Opcode 1)
    if (opcode == 1) {
        handleReadRequest(serverThreadSocket, filename, clientAddress, clientId);
    }
    // Handle WRQ request (Opcode 2)
    else if (opcode == 2) {
        handleWriteRequest(serverThreadSocket, filename, clientAddress, clientId);
    }
    else {
        // send error packet
    }

    // set destroy thread bool variable to true.
    close(serverThreadSocket);
    std::get<1>(clientThreads[clientId]) = true;
}

void TFTPServer::destroyClientThreads(std::map<int, std::tuple<std::thread, bool>>& clientThreads){

}

void TFTPServer::start() {
    // bindSocket();
    while (true) {
        std::cerr << "Server is started and waiting to recieve data" << std::endl;
        struct sockaddr_in clientAddress;
        socklen_t clientAddrLen = sizeof(clientAddress);
        char buffer[1024];
        memset(buffer, 0, sizeof(buffer));
        std::cerr << "start receiving data" << std::endl;
        int bytesRead = recvfrom(serverSocket, buffer, sizeof(buffer), 0, (struct sockaddr*)&clientAddress, &clientAddrLen);
        std::cerr << "completed received data" << std::endl;
        if (bytesRead < 0) {
            std::cerr << "Error receiving data" << std::endl;
            continue;
        }

        std::cerr << "successfully receiving data" << std::endl;
        std::cerr << "buffer recieved" << std::endl;

        // Extract the opcode from the received packet
        uint16_t opcode = (uint16_t)(((buffer[1] & 0xFF) << 8) | (buffer[0] & 0XFF));
        opcode = ntohs(opcode);
        std::cerr << "Opcode:" << opcode << std::endl;


        // Handle RRQ request (Opcode 1)
        if (opcode == 1 || opcode == 2) {
            int clientId = 9800 + nextClientId++;
            std::string filename(buffer + 2);
            std::cerr << "filename:" << filename << std::endl;
            std::string mode(buffer + 2 + filename.length() + 1);
            std::cerr << "mode:" << mode << std::endl;
            std::cerr << "Starting client thread" << std::endl;
            clientThreads[clientId] = std::make_tuple(std::thread([this, clientId, clientAddress, buffer, bytesRead, filename] {
                 int serverThreadSocket = socket(AF_INET, SOCK_DGRAM, 0);
                struct sockaddr_in serverThreadSocketAddr;
                serverThreadSocketAddr.sin_family = AF_INET;
                serverThreadSocketAddr.sin_port = htons(clientId);
                serverThreadSocketAddr.sin_addr.s_addr = inet_addr("127.0.0.1");   // INADDR_ANY;
                if (bind(serverThreadSocket, (struct sockaddr*)&serverThreadSocketAddr, sizeof(serverThreadSocketAddr)) < 0) {
                    std::cerr << "Error binding server socket" << std::endl;
                    exit(1);
                }
                std::cout << "Server binded to port " << clientId << std::endl;
                handleReadRequest(serverThreadSocket, filename, clientAddress, clientId);
            }), false);
            
        }
        else {
            std::cerr << "Incorrect opcode recieved" << std::endl;
            // todo handle other opcodes condition also
            //send error packet
            continue;
        }
        if(DESTROY_SERVER) {
            break;
        }
    }
}


int main() {
    std :: cout << "starting the server" << std::endl;
    TFTPServer server(54534);
    std :: cout << "initialized the server" << std::endl;
    // start a thread that will destroy threads, pass the map as argument, 
    server.start();
    std :: cout << "server is started" << std::endl;
    return 0;
}