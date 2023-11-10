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


void TFTPServer::handleWriteRequest(int clientSocket, const std::string& filename, struct sockaddr_in clientAddress, int clientId) {
    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        // Send an error packet (Access violation - Error Code 2)
        uint8_t packet[516];
        memset(packet, 0, sizeof(packet));
        const std::string errorMessage = "Access violation";
        TFTPPacket::createErrorPacket(packet, 2, errorMessage);
        packet[4 + errorMessage.size()] = '\0';
        size_t packetSize = 4 + errorMessage.size() + 1;
        sendto(clientSocket, packet, packetSize, 0, (struct sockaddr*)&clientAddress, sizeof(clientAddress));
        return;
    }
   
    // TODO: Handle the write operation

    file.close();
}

void TFTPServer::sendError(int clientSocket, uint16_t errorCode, const std::string& errorMsg, struct sockaddr_in clientAddress) {
    // uint16_t opcode = 5; // ERROR opcode
    uint8_t packet[4 + errorMsg.size() + 1];
    TFTPPacket::createErrorPacket(packet, errorCode, errorMsg);
    packet[4 + errorMsg.size()] = '\0';
    sendto(clientSocket, packet, 4 + errorMsg.size() + 1, 0, (struct sockaddr*)&clientAddress, sizeof(clientAddress));
}

void TFTPServer::handleReadRequest(int clientSocket, const std::string& filename, struct sockaddr_in clientAddress, int clientId) {
    std::string directory = "serverDatabase/";
    std::string filePath = directory + filename;
    std::ifstream file(filePath, std::ios::binary);
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
        uint8_t packet[516];
        memset(packet, 0, sizeof(packet));
        memset(dataBuffer, 0, 512);
        // file.read(dataBuffer, 512);
        size_t dataSize = file.gcount();
        dataSize = TFTPPacket::readDataBlock(filePath, blockNumber, dataBuffer, dataSize);
        if (dataSize == 0) {
            break; // End of file
        }
        std::cerr << "Data Size: " << dataSize << std::endl;
        TFTPPacket::createDataPacket(packet, blockNumber, dataBuffer, dataSize);
        sendto(clientSocket, packet, dataSize + 4, 0, (struct sockaddr*)&clientAddress, sizeof(clientAddress));

        // Wait for acknowledgment
        char ackBuffer[4];
        int ackSize = recvfrom(clientSocket, ackBuffer, sizeof(ackBuffer), 0, nullptr, nullptr);
        if (ackSize < 0) {
            std::cerr << "Error receiving acknowledgment" << std::endl;
            return;
        }
        std::cerr << "Acknoledment recieved" <<std::endl;
        // Check the received acknowledgment packet (Opcode 4)
        uint16_t ackOpcode = (ackBuffer[0] << 8) | ackBuffer[1];
        uint16_t ackBlockNumber = (ackBuffer[2] << 8) | ackBuffer[3];
        if (ackOpcode != 4 || ackBlockNumber != blockNumber) {
            std::cerr << "Invalid acknowledgment received" << std::endl;
            uint8_t packet[516];
            memset(packet, 0, sizeof(packet));
            std::cerr << "Send an error packet Illegal TFTP operation - Error Code 2" << std::endl;
            const std::string errorMessage = "Illegal TFTP operation";
            TFTPPacket::createErrorPacket(packet, 2, errorMessage);
            packet[4 + errorMessage.size()] = '\0';
            size_t packetSize = 4 + errorMessage.size() + 1;
            sendto(clientSocket, packet, packetSize, 0, (struct sockaddr*)&clientAddress, sizeof(clientAddress));
            continue;
        }
        std::cerr<< "Correct ACK packet recieved for blocknumber: " << ackBlockNumber << std::endl;

        if(dataSize < 512) {
            break;
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
    // uint16_t opcode = 4; // ACK opcode
    uint8_t packet[4];
    TFTPPacket::createACKPacket(packet, blockNumber);
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
    else if(opcode == 6) {
        // define new opcode 
    }
    else {
        uint8_t packet[516];
        memset(packet, 0, sizeof(packet));
        // Send an error packet (File not found - Error Code 1)
        const std::string errorMessage = "Illegal TFTP operation";
        TFTPPacket::createErrorPacket(packet, 4, errorMessage);
        packet[4 + errorMessage.size()] = '\0';
        size_t packetSize = 4 + errorMessage.size() + 1;
        sendto(serverThreadSocket, packet, packetSize, 0, (struct sockaddr*)&clientAddress, sizeof(clientAddress));
    }

    // set destroy thread bool variable to true.
    close(serverThreadSocket);
    std::get<1>(clientThreads[clientId]) = true;
}

void TFTPServer::destroyClientThreads(std::map<int, std::tuple<std::thread, bool>>& clientThreads){

}

void TFTPServer::start() {
    // bindSocket();
    while (!DESTROY_SERVER) {
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
            for (int i = 0; mode[i] != '\0'; i++) {
                mode[i] = std::tolower(mode[i]);
            }
            if (mode != "octet"){
                uint8_t packet[516];
                memset(packet, 0, sizeof(packet));
                // Send an error packet (File not found - Error Code 1)
                const std::string errorMessage = "Illegal TFTP operation";
                TFTPPacket::createErrorPacket(packet, 4, errorMessage);
                packet[4 + errorMessage.size()] = '\0';
                size_t packetSize = 4 + errorMessage.size() + 1;
                sendto(serverSocket, packet, packetSize, 0, (struct sockaddr*)&clientAddress, sizeof(clientAddress));
                continue;
            }
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
            // uint8_t packet[516];
            // memset(packet, 0, sizeof(packet));
            // // Send an error packet (File not found - Error Code 1)
            const std::string errorMessage = "Illegal TFTP operation";
            // TFTPPacket::createErrorPacket(packet, 4, errorMessage);
            // packet[4 + errorMessage.size()] = '\0';
            // size_t packetSize = 4 + errorMessage.size() + 1;
            // sendto(serverSocket, packet, packetSize, 0, (struct sockaddr*)&clientAddress, sizeof(clientAddress));
            sendError(serverSocket, 4, errorMessage, clientAddress);
            
            continue;
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