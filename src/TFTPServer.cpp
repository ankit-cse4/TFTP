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
    std::string directory = "serverDatabase/";
    std::string filePath = directory + filename;
    std::ofstream file(filePath, std::ios::binary);
    bool fileExist = false; 
    if (!file) {
        // Send an error packet (Disk full or allocation exceeded - Error Code 3)
        const std::string errorMessage = "Disk full or allocation exceeded.";
        sendError(clientSocket, ERROR_DISK_FULL, errorMessage, clientAddress);
        return;
    }
    else if (fileExist) {
        // Send an error packet (File already exists. - Error Code 6)
        const std::string errorMessage = "File already exists.";
        sendError(clientSocket, ERROR_FILE_ALREADY_EXISTS, errorMessage, clientAddress);
        return;
    }

    uint16_t blockNumber = 0;
    char recievedBuffer[516];
    memset(recievedBuffer, 0, sizeof(recievedBuffer));
    sendACK(clientSocket, blockNumber, clientAddress);
    uint16_t expectedBlockNumber = 1;
    while (true)
    {
        int readBytes = recvfrom(clientSocket, recievedBuffer, sizeof(recievedBuffer), 0, nullptr, nullptr);
        if (readBytes < 0) {
            std::cerr << "Error receiving data" << std::endl;
            continue;
        }
        else if (readBytes > 516)
        {
            /* code logic */
            std::cerr << "Incorrect packet Recieved" << std::endl;
            continue;
        }
        
        // check for correct clientaddress and client port

        // Extract the opcode from the received packet
        uint16_t opcode = (uint16_t)(((recievedBuffer[1] & 0xFF) << 8) | (recievedBuffer[0] & 0XFF));
        opcode = ntohs(opcode);
        std::cerr << "Recieved Opcode:" << opcode << std::endl;
        if (opcode != 3) {
            std::cerr << "Illegal Opcode Recieved" << std::endl;
            std::string errorMessage = "Illegal TFTP operation";
            sendError(clientSocket, ERROR_ILLEGAL_TFTP_OPERATION, errorMessage, clientAddress);
            continue;
        }
        uint16_t recvBlockNumber = (uint16_t)(((recievedBuffer[3] & 0xFF) << 8) | (recievedBuffer[2] & 0XFF));
        recvBlockNumber = ntohs(recvBlockNumber);
        std::cerr << "Recieved Block Number:" << recvBlockNumber << std::endl;
        if (recvBlockNumber != expectedBlockNumber) {
            std::cerr << "Incorrect Block Number Recieved" << std::endl;
            std::string errorMessage = "Incorrect Block Number Recieved. Resend";
            sendError(clientSocket, ERROR_NOT_DEFINED, errorMessage, clientAddress);
            continue;
        }
        std::cerr << "Read Bytes: " << readBytes << std::endl;
        int dataLength = readBytes - 4;
        char recvData[dataLength];
        memset(recvData, 0, dataLength);
        std::cerr << "Copying data memory" << std::endl;
        memcpy(recvData, recievedBuffer + 4, dataLength);
        std::cerr << "Recieved data size: " << dataLength << std::endl;
        std::cerr << "Received Data: " << std::endl << std::endl << recvData << std::endl;
        std::cerr << "Writing data in file" << std::endl;
        file.write(reinterpret_cast<char*>(recvData), dataLength);
        if (file.fail())
        {
            std::cerr << "File write error" << std::endl;
            // code logic for rewrite.
            continue;
        }
        sendACK(clientSocket, expectedBlockNumber, clientAddress);
        expectedBlockNumber++;
        if(dataLength < 512) {
            std::cerr << "File recieved Successfuly." << std::endl;
            break;
        }
    }
    // TODO: Handle the write operation
    file.close();
}

void TFTPServer::sendError(int clientSocket, uint16_t errorCode, const std::string& errorMsg, struct sockaddr_in clientAddress) {
    uint8_t packet[4 + errorMsg.size() + 1];
    TFTPPacket::createErrorPacket(packet, errorCode, errorMsg);
    packet[4 + errorMsg.size()] = '\0';
    if(sendto(clientSocket, packet, 4 + errorMsg.size() + 1, 0, (struct sockaddr*)&clientAddress, sizeof(clientAddress)) < 0) {
        std::cerr << "[ERROR] : fail to send error packet" << std::endl;
        return;
    }
    std::cerr << "[LOG] : Error packet send to client " << clientAddress.sin_addr.s_addr << " with error code: " << errorCode << std::endl;
}

void TFTPServer::handleReadRequest(int clientSocket, const std::string& filename, struct sockaddr_in clientAddress, int clientId) {
    std::string directory = "serverDatabase/";
    std::string filePath = directory + filename;
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        // Send an error packet (File not found - Error Code 1)
        const std::string errorMessage = "File not found";
        sendError(clientSocket, ERROR_FILE_NOT_FOUND, errorMessage, clientAddress);
        return;
    }

    // Read and send data in blocks of 512 bytes
    uint16_t blockNumber = 1;
    char dataBuffer[512];
    while (true) {
        uint8_t packet[516];
        memset(packet, 0, sizeof(packet));
        memset(dataBuffer, 0, 512);
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
            const std::string errorMessage = "Illegal TFTP operation";
            sendError(clientSocket, ERROR_ACCESS_VIOLATION, errorMessage, clientAddress);
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


void TFTPServer::sendACK(int clientSocket, uint16_t blockNumber, struct sockaddr_in clientAddress) {
    uint8_t packet[4];
    TFTPPacket::createACKPacket(packet, blockNumber);
    if (sendto(clientSocket, packet, TFTP_OPCODE_ACK, 0, (struct sockaddr*)&clientAddress, sizeof(clientAddress)) < 0) {
        std::cerr << "[ERROR] : fail to send ACK packet" << std::endl;
        return;
    }
    std::cerr << "[LOG] : ACK packet send to client " << clientAddress.sin_addr.s_addr << " with Block Number: " << blockNumber << std::endl;
}

void TFTPServer::handleClientThread(int serverThreadSocket, const std::string& filename, struct sockaddr_in clientAddress,  int clientId, uint16_t opcode, std::map<int, std::tuple<std::thread, bool>>& clientThreads) {

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
        // Send an error packet (Illegal TFTP operation - Error Code 4)
        const std::string errorMessage = "Illegal TFTP operation";
        sendError(serverThreadSocket, ERROR_ILLEGAL_TFTP_OPERATION, errorMessage, clientAddress);
    }

    // set destroy thread bool variable to true.
    close(serverThreadSocket);
    std::get<1>(clientThreads[clientId]) = true;
    std::cerr << "[LOG]: Thread work completed. Can be Destroyed" << std::endl;
}

void TFTPServer::destroyClientThreads(std::map<int, std::tuple<std::thread, bool>>& clientThreads){

}

void TFTPServer::start() {
    std::cerr << "Server is started and waiting to recieve data" << std::endl;
    // bindSocket();
    struct timeval timeout;
    timeout.tv_sec = 5;  // seconds
    timeout.tv_usec = 0; // microseconds
    while (!DESTROY_SERVER) {
        if (setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
            std::cerr << "Error setting receive timeout" << std::endl;
            break;
        }
        struct sockaddr_in clientAddress;
        socklen_t clientAddrLen = sizeof(clientAddress);
        char buffer[1024];
        memset(buffer, 0, sizeof(buffer));
        std::cerr << "start receiving data" << std::endl;
        int bytesRead = recvfrom(serverSocket, buffer, sizeof(buffer), 0, (struct sockaddr*)&clientAddress, &clientAddrLen);
        if (bytesRead < 0) {
            std::cerr << "Timeout Occured while listening" << std::endl;
            continue;
        }
        else if (bytesRead > 516) {
            std::cerr << "Error receiving data" << std::endl;
            const std::string errorMessage = "Illegal TFTP operation";
            sendError(serverSocket, ERROR_ILLEGAL_TFTP_OPERATION, errorMessage, clientAddress);
            continue;
        }
        std::cerr << "completed received data" << std::endl;
        // std::cerr << "buffer recieved" << std::endl;

        // Extract the opcode from the received packet
        uint16_t opcode = (uint16_t)(((buffer[1] & 0xFF) << 8) | (buffer[0] & 0XFF));
        opcode = ntohs(opcode);
        std::cerr << "Opcode:" << opcode << std::endl;


        // Handle RRQ request (Opcode 1)
        if (opcode == TFTP_OPCODE_RRQ || opcode == TFTP_OPCODE_WRQ) {
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
                // Send an error packet (File not found - Error Code 1)
                const std::string errorMessage = "Illegal TFTP operation";
                sendError(serverSocket, ERROR_ILLEGAL_TFTP_OPERATION, errorMessage, clientAddress);
                continue;
            }
            clientThreads[clientId] = std::make_tuple(std::thread([this, clientId, clientAddress, buffer, bytesRead, filename, opcode] {
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
                handleClientThread(serverThreadSocket, filename, clientAddress, clientId, opcode, clientThreads);
                
            }), false);
            
        }
        else {
            std::cerr << "Incorrect opcode recieved" << std::endl;
            const std::string errorMessage = "Illegal TFTP operation";
            sendError(serverSocket, ERROR_ILLEGAL_TFTP_OPERATION, errorMessage, clientAddress);
            continue;
        }
    }

    if(DESTROY_SERVER) {
        std::cerr << "Shutting Down Server...." << std::endl;
    }
    else {
        std::cerr << "Error Occured. Force shutdown server" << std::endl;
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