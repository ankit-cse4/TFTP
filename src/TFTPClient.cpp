#include "TFTPClient.h"
#include <iostream>
#include <cstring>
#include <unistd.h>

TFTPClient::TFTPClient(const std::string& serverIP) {
    clientSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (clientSocket < 0) {
        std::cerr << "Error creating client socket" << std::endl;
        exit(1);
    }
    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = inet_addr(serverIP.c_str());
    serverAddress.sin_port = htons(SERVER_DEFAULT_PORT);

    struct timeval timeout;
    timeout.tv_sec = 5;  // seconds
    timeout.tv_usec = 0; // microseconds

    if (setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
            std::cerr << "Error setting receive timeout" << std::endl;
            exit(1);
        }
    std::cerr << "[LOG] Socket timeout set successfully" << std::endl;
}


bool TFTPClient::sendLSPacket(int clientSocket, struct sockaddr_in serverAddress){
    uint8_t packet[3];
    TFTPPacket::createLSPacket(packet);
    if (sendto(clientSocket, packet, TFTP_OPCODE_ACK, 0, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        std::cerr << "[ERROR] : fail to send ACK packet" << std::endl;
        return false;
    }
    std::cerr << "[LOG] : LS packet send to delete " << serverAddress.sin_addr.s_addr << std::endl;
    return true;
}

bool TFTPClient::sendDELETEPacket(int clientSocket, struct sockaddr_in serverAddress, const std::string& filename){
    uint8_t packet[2 + filename.size() + 1];
    TFTPPacket::createDeletePacket(packet, filename);
    if (sendto(clientSocket, packet, TFTP_OPCODE_ACK, 0, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        std::cerr << "[ERROR] : fail to send Delete packet" << std::endl;
        return false;
    }
    std::cerr << "[LOG] : Delete packet send to server " << serverAddress.sin_addr.s_addr << std::endl;
    return true;
}

void TFTPClient::startClient(int opcode, const std::string& filename) {
    struct sockaddr_in clientAddress;
    clientAddress.sin_family = AF_INET;
    clientAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
    clientAddress.sin_port = htons(CLIENT_DEFAULT_PORT);

    if (bind(clientSocket, (struct sockaddr*)&clientAddress, sizeof(clientAddress)) < 0) {
        std::cerr << "Error binding client socket" << std::endl;
        exit(1);
    }
    std::cout << "client binded to port " << CLIENT_DEFAULT_PORT << std::endl;
    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = inet_addr(serverIP.c_str());
    serverAddress.sin_port = htons(SERVER_DEFAULT_PORT);

    // implemented logic for handling client according to opcode
    switch (opcode)
    {
        case TFTP_OPCODE_LS:
            if (handleLSRequest(clientSocket, serverAddress))
            {
                std::cout << "Output is stored in file ls.txt in /clientDatabase directory." << std::endl;
                std::cerr << "exiting" << std::endl;
                break;
            }
            std::cout << "[ERROR received] " << std::endl;
            break;
        case TFTP_OPCODE_DELETE:
            if (handleDELETERequest(clientSocket, serverAddress, filename))
            {
                std::cout << "FILE deleted successfully" << std::endl;
                std::cerr << "exiting" << std::endl;
                break;
            }
            std::cout << "[ERROR received]: check error log " << std::endl;
            break;
        case TFTP_OPCODE_RRQ:
            if (handleRRQRequest(clientSocket, serverAddress, filename))
            {
                std::cout << "FILE READ successful." << std::endl;
                std::cerr << "exiting" << std::endl;
                break;
            }
            std::cout << "[ERROR received]: check error log " << std::endl;
            break;
        case TFTP_OPCODE_WRQ:
            if (handleWRQRequest(clientSocket, serverAddress, filename))
            {
                std::cout << "FILE WRITE successful." << std::endl;
                std::cerr << "exiting" << std::endl;
                break;
            }
            std::cout << "[ERROR received]: check error log " << std::endl;
            break;
        default:
            std::cerr << "Invalid Request Opcode" << std::endl;
            std::cout << "[ERROR received]: check error log " << std::endl;
            break;
    }
    close(clientSocket);
    return;
}

bool TFTPClient::handleRRQRequest(int clientSocket, struct sockaddr_in serverAddress, const std::string& filename) {
    if (filename.empty())
    {
        std::cerr << "[ERROR] : filename is empty" << std::endl;
        return false;
    }
    if (!sendDELETEPacket(clientSocket, serverAddress, filename))
    {
        std::cerr << "[ERROR] : fail to send RRQ packet" << std::endl;
        return false;
    }
    std::cerr << "[LOG] : sent RRQ packet" << std::endl;
    std::string directory = "clientDatabase/";
    std::string filePath = directory + filename;
    std::ofstream file(filePath, std::ios::binary);
    if (!file) {
        // Send an error packet (Disk full or allocation exceeded - Error Code 3)
        std::cerr << "[ERROR] : Cannot create file" << std::endl;
        const std::string errorMessage = "Disk full or allocation exceeded.";
        sendError(clientSocket, ERROR_DISK_FULL, errorMessage, clientAddress);
        return false;
    } 
    char recievedBuffer[516];
    memset(recievedBuffer, 0, sizeof(recievedBuffer));
    uint16_t expectedBlockNumber = 1;
    int retry = MAX_RETRY;
    bool initialPacket = true;
    while (retry)
    {
        struct sockaddr_in recvAddress;
        socklen_t recvAddressLen = sizeof(recvAddress);
        int readBytes = recvfrom(clientSocket, recievedBuffer, sizeof(recievedBuffer), 0, (struct sockaddr*)&recvAddress, &recvAddressLen);
        if (readBytes < 0)
        {
            std::cerr << "TIMEOUT Occured" << std::endl;
            retry--;
            continue;
        }
        else if (readBytes > MAX_PACKET_SIZE)
        {
            std::cerr << "Invalid packet received" << std::endl;
            const std::string errorMessage = "Illegal TFTP operation";
            sendError(clientSocket, ERROR_ACCESS_VIOLATION, errorMessage, recvAddress);
            continue;
        }
        else {
            retry = MAX_RETRY;
        }
        std::cerr << "Received packet" << std::endl;
        if (recvAddress.sin_addr.s_addr != serverAddress.sin_addr.s_addr)
        {
            std::cerr << "Corrupt packet from different IP received" << std::endl;
            const std::string errorMessage = "Unknown transfer ID";
            sendError(clientSocket, ERROR_UNKNOWN_TID, errorMessage, recvAddress);
            continue;
        }
        if (initialPacket)
        {
            serverAddress.sin_port = recvAddress.sin_port;
            initialPacket = false;
            std::cerr << "[LOG] : server port no. indentified" << std::endl;
        }
        else if (serverAddress.sin_port != recvAddress.sin_port)
        {
            std::cerr << "Corrupt packet from different port received" << std::endl;
            const std::string errorMessage = "Unknown transfer ID";
            sendError(clientSocket, ERROR_UNKNOWN_TID, errorMessage, recvAddress);
            continue;
        }
        std::cerr << "[LOG] : Source Verified" << std::endl;
        uint16_t opcode = (uint16_t)(((recievedBuffer[1] & 0xFF) << 8) | (recievedBuffer[0] & 0XFF));
        opcode = ntohs(opcode);
        std::cerr << "Recieved Opcode:" << opcode << std::endl;

        if (opcode != TFTP_OPCODE_ERROR || opcode != TFTP_OPCODE_DATA) {
            std::cerr << "Illegal Opcode Recieved" << std::endl;
            std::string errorMessage = "Illegal TFTP operation";
            sendError(clientSocket, ERROR_ILLEGAL_TFTP_OPERATION, errorMessage, recvAddress);
            file.close();
            return false;
        }
        std::cerr << "[LOG] : Opcode Verified" << std::endl;
        std::cerr << "Read Bytes: " << readBytes << std::endl;
        uint16_t recvBlockNumber = (uint16_t)(((recievedBuffer[3] & 0xFF) << 8) | (recievedBuffer[2] & 0XFF));
        recvBlockNumber = ntohs(recvBlockNumber);
        int dataLength = readBytes - 4;
        char recvData[dataLength];
        memset(recvData, 0, dataLength);
        std::cerr << "Copying data memory" << std::endl;
        memcpy(recvData, recievedBuffer + 4, dataLength);
        if (opcode == TFTP_OPCODE_ERROR)
        {
            std::cerr << "Error packet recieved from server with error code: " << recvBlockNumber << std::endl;
            std::cerr << "Recieved data size: " << dataLength << std::endl;
            std::cerr << "[ERROR " << recvBlockNumber << "] " << recvData << std::endl;
            std::cout << "[ERROR " << recvBlockNumber << "] " << recvData << std::endl;
            return false;
        }
        if (recvBlockNumber != expectedBlockNumber)
        {
            sendACK(clientSocket, expectedBlockNumber-1, serverAddress);
            continue;
        }
        
        std::cerr << "Writing data in file" << std::endl;
        file.write(reinterpret_cast<char*>(recvData), dataLength);
        if (file.fail())
        {
            std::cerr << "File write error" << std::endl;
            const std::string errorMessage = "Disk full or allocation exceeded.";
            sendError(clientSocket, ERROR_DISK_FULL, errorMessage, serverAddress);
            // code logic for rewrite.
            file.close();
            return false;
        }
        sendACK(clientSocket, expectedBlockNumber, clientAddress);
        expectedBlockNumber++;
        if(dataLength < 512) {
            std::cerr << "File recieved Successfuly." << std::endl;
            file.close();
            return true;
        }
        
    }
    if (!retry)
    {
        std::cerr << "Max retry for receiving timeout exceeded. Shutting down server" << std::endl;
        file.close();
    }
    return false;
}

bool TFTPClient::sendRRQPacket(int clientSocket, struct sockaddr_in serverAddress, const std::string& filename) {
    std::string mode = "octet";
    uint8_t packet[4 + filename.size() + mode.size()];
    TFTPPacket::createRRQPacket(packet, filename, mode);
    if (sendto(clientSocket, packet, TFTP_OPCODE_ACK, 0, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        std::cerr << "[ERROR] : fail to send RRQ request packet" << std::endl;
        return false;
    }
    std::cerr << "[LOG] : RRQ request packet send to server " << serverAddress.sin_addr.s_addr << std::endl;
    return true;
}

bool TFTPClient::handleWRQRequest(int clientSocket, struct sockaddr_in serverAddress, const std::string& filename) {
    return false;
}

bool TFTPClient::sendWRQPacket(int clientSocket, struct sockaddr_in serverAddress, const std::string& filename) {
    std::string mode = "octet";
    uint8_t packet[4 + filename.size() + mode.size()];
    TFTPPacket::createWRQPacket(packet, filename, mode);
    if (sendto(clientSocket, packet, TFTP_OPCODE_ACK, 0, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        std::cerr << "[ERROR] : fail to send WRQ request packet" << std::endl;
        return false;
    }
    std::cerr << "[LOG] : WRQ request packet send to server " << serverAddress.sin_addr.s_addr << std::endl;
    return true;
}

void TFTPClient::sendError(int clientSocket, uint16_t errorCode, const std::string& errorMsg, struct sockaddr_in serverAddress) {
    uint8_t packet[4 + errorMsg.size() + 1];
    TFTPPacket::createErrorPacket(packet, errorCode, errorMsg);
    packet[4 + errorMsg.size()] = '\0';
    if(sendto(clientSocket, packet, 4 + errorMsg.size() + 1, 0, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        std::cerr << "[ERROR] : fail to send error packet" << std::endl;
        return;
    }
    std::cerr << "[LOG] : Error packet send to server " << serverAddress.sin_addr.s_addr << " with error code: " << errorCode << std::endl;
}

bool TFTPClient::handleDELETERequest(int clientSocket, struct sockaddr_in serverAddress, const std::string& filename) {
    if (filename.empty())
    {
        std::cerr << "[ERROR] : filename is empty" << std::endl;
        return false;
    }
    if (!sendDELETEPacket(clientSocket, serverAddress, filename))
    {
        std::cerr << "[ERROR] : fail to send DELETE packet" << std::endl;
        return false;
    }
    std::cerr << "[LOG] : sent DELETE packet" << std::endl;
    char recievedBuffer[516];
    uint16_t expectedBlockNumber = 1;
    int retry = MAX_RETRY;
    while(retry)
    {
        struct sockaddr_in recvAddress;
        socklen_t recvAddressLen = sizeof(recvAddress);
        int readBytes = recvfrom(clientSocket, recievedBuffer, sizeof(recievedBuffer), 0, (struct sockaddr*)&recvAddress, &recvAddressLen);
        if (readBytes < 0)
        {
            std::cerr << "TIMEOUT Occured" << std::endl;
            retry--;
            continue;
        }
        else if (readBytes > MAX_PACKET_SIZE)
        {
            std::cerr << "Invalid packet received" << std::endl;
            const std::string errorMessage = "Illegal TFTP operation";
            sendError(clientSocket, ERROR_ACCESS_VIOLATION, errorMessage, recvAddress);
            continue;
        }
        else {
            retry = MAX_RETRY;
        }
        std::cerr << "Received data" << std::endl;
        if (recvAddress.sin_addr.s_addr != inet_addr(serverIP.c_str()))
        {
            std::cerr << "Corrupt packet from different IP received" << std::endl;
            const std::string errorMessage = "Unknown transfer ID";
            sendError(clientSocket, ERROR_UNKNOWN_TID, errorMessage, recvAddress);
            continue;
        }
        uint16_t opcode = (uint16_t)(((recievedBuffer[1] & 0xFF) << 8) | (recievedBuffer[0] & 0XFF));
        opcode = ntohs(opcode);
        std::cerr << "Recieved Opcode:" << opcode << std::endl;

        if (opcode != TFTP_OPCODE_ERROR || opcode != TFTP_OPCODE_ACK) {
            std::cerr << "Illegal Opcode Recieved" << std::endl;
            std::string errorMessage = "Illegal TFTP operation";
            sendError(clientSocket, ERROR_ILLEGAL_TFTP_OPERATION, errorMessage, recvAddress);
            continue;
        }
        std::cerr << "Read Bytes: " << readBytes << std::endl;
        uint16_t recvBlockNumber = (uint16_t)(((recievedBuffer[3] & 0xFF) << 8) | (recievedBuffer[2] & 0XFF));
        recvBlockNumber = ntohs(recvBlockNumber);
        if (opcode == TFTP_OPCODE_ERROR)
        {
            std::cerr << "Error packet recieved from server with error code: " << recvBlockNumber << std::endl;
            int dataLength = readBytes - 4;
            char recvData[dataLength];
            memset(recvData, 0, dataLength);
            std::cerr << "Copying data memory" << std::endl;
            memcpy(recvData, recievedBuffer + 4, dataLength);
            std::cerr << "Recieved data size: " << dataLength << std::endl;
            std::cerr << "[ERROR " << recvBlockNumber << "] " << recvData << std::endl;
            std::cout << "[ERROR " << recvBlockNumber << "] " << recvData << std::endl;
            return false;
        }
        if (opcode == TFTP_OPCODE_ACK)
        {
            std::cerr << "ACK recieved" << std::endl;
            if (recvBlockNumber == ACK_OK) 
            {
                std::cout << "ACK OK Recieved." << std::endl;
                return true;
            }
            std::cerr << "[ERROR] Incorrect ACK recieved with code: " << recvBlockNumber << std::endl;
            return false;
        }
        
    }
    std::cerr << "Max retry exceeded for timeout." << std::endl;
    return false;
}

bool TFTPClient::handleLSRequest(int clientSocket, struct sockaddr_in serverAddress) {
    if (!sendLSPacket(clientSocket, serverAddress)) {
        std::cerr << "[ERROR] : fail to send LS packet" << std::endl;
        return false;
    }
    std::cerr << "[LOG] : sent LS packet" << std::endl;
    std::string directory = "clientDatabase/";
    std::string filePath = directory + DEFAULT_LS_FILE_NAME;
    std::ofstream file(filePath, std::ios::binary); 
    if (!file) {
        // Send an error packet (Disk full or allocation exceeded - Error Code 3)
        const std::string errorMessage = "Disk full or allocation exceeded.";
        sendError(clientSocket, ERROR_DISK_FULL, errorMessage, serverAddress);
        return false;
    }
    char recievedBuffer[516];
    // recieve the data
    int retry = MAX_RETRY;
    uint16_t expectedBlockNumber = 1;
    bool success = false;
    while(retry)
    {
        struct sockaddr_in recvAddress;
        socklen_t recvAddressLen = sizeof(recvAddress);
        int readBytes = recvfrom(clientSocket, recievedBuffer, sizeof(recievedBuffer), 0, (struct sockaddr*)&recvAddress, &recvAddressLen);
        if (readBytes < 0)
        {
            std::cerr << "TIMEOUT Occured" << std::endl;
            retry--;
            continue;
        }
        else if (readBytes > MAX_PACKET_SIZE)
        {
            std::cerr << "Invalid packet received" << std::endl;
            const std::string errorMessage = "Illegal TFTP operation";
            sendError(clientSocket, ERROR_ACCESS_VIOLATION, errorMessage, recvAddress);
            continue;
        }
        else {
            retry = MAX_RETRY;
        }
        std::cerr << "Received data" << std::endl;
        if (recvAddress.sin_addr.s_addr != inet_addr(serverIP.c_str()))
        {
            std::cerr << "Corrupt packet from different IP received" << std::endl;
            const std::string errorMessage = "Unknown transfer ID";
            sendError(clientSocket, ERROR_UNKNOWN_TID, errorMessage, recvAddress);
            continue;
        }
        
        // Extract the opcode from the received packet
        uint16_t opcode = (uint16_t)(((recievedBuffer[1] & 0xFF) << 8) | (recievedBuffer[0] & 0XFF));
        opcode = ntohs(opcode);
        std::cerr << "Recieved Opcode:" << opcode << std::endl;

        if (opcode != TFTP_OPCODE_DATA) {
            std::cerr << "Illegal Opcode Recieved" << std::endl;
            std::string errorMessage = "Illegal TFTP operation";
            sendError(clientSocket, ERROR_ILLEGAL_TFTP_OPERATION, errorMessage, recvAddress);
            continue;
        }
        uint16_t recvBlockNumber = (uint16_t)(((recievedBuffer[3] & 0xFF) << 8) | (recievedBuffer[2] & 0XFF));
        recvBlockNumber = ntohs(recvBlockNumber);
        std::cerr << "Recieved Block Number:" << recvBlockNumber << std::endl;
        if (recvBlockNumber != expectedBlockNumber) {
            std::cerr << "Incorrect Block Number Recieved" << std::endl;
            std::string errorMessage = "Incorrect Block Number Recieved. Resend";
            sendError(clientSocket, ERROR_NOT_DEFINED, errorMessage, recvAddress);
            continue;
        }
        std::cerr << "Read Bytes: " << readBytes << std::endl;
        int dataLength = readBytes - 4;
        char recvData[dataLength];
        memset(recvData, 0, dataLength);
        std::cerr << "Copying data memory" << std::endl;
        memcpy(recvData, recievedBuffer + 4, dataLength);
        std::cerr << "Recieved data size: " << dataLength << std::endl;
        // std::cerr << "Received Data: " << std::endl << std::endl << recvData << std::endl;
        std::cerr << "Writing data in file" << std::endl;
        file.write(reinterpret_cast<char*>(recvData), dataLength);
        if (file.fail())
        {
            std::cerr << "File write error" << std::endl;
            // code logic for rewrite.
            break;
        }
        sendACK(clientSocket, expectedBlockNumber, recvAddress);
        expectedBlockNumber++;
        if (readBytes < MAX_PACKET_SIZE)
        {
            success = true;
            std::cerr << "File recieved Successfuly." << std::endl;
            /* code */
            break;
        }
        // recieve till readbytes < 516
    }
    file.close();
    if (!retry)
    {
        std::cerr << "Max retry for receiving timeout exceeded." << std::endl;
    }
    return success;
}

void TFTPClient::sendACK(int clientSocket, uint16_t blockNumber, struct sockaddr_in serverAddress) {
    uint8_t packet[4];
    TFTPPacket::createACKPacket(packet, blockNumber);
    if (sendto(clientSocket, packet, TFTP_OPCODE_ACK, 0, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        std::cerr << "[ERROR] : fail to send ACK packet" << std::endl;
        return;
    }
    std::cerr << "[LOG] : ACK packet send to client " << serverAddress.sin_addr.s_addr << " with Block Number: " << blockNumber << std::endl;
}

int main(int argc, char* argv[]){
    int opcode;
    std::string filename;
    std::string serverIP;
    std::string request;
    switch (argc)
    {
        case 4:
            serverIP = argv[3];
            request = argv[1];
            filename = argv[2];
            if(request == "READ") 
            {
                opcode = TFTP_OPCODE_RRQ;
            }
            else if (request == "WRITE")
            {
                opcode = TFTP_OPCODE_WRQ;
            }
            else if (request == "DELETE")
            {
                opcode = TFTP_OPCODE_DELETE;
            }
            else
            {
                std::cerr << "[ERROR] TFTP Client : Invalid Request Type { READ | WRITE | DELETE | LS }";
                std::cout << "TFTP Client : Invalid Request Type { READ | WRITE | DELETE | LS }";
                break;
            }
            break;
        case 3:
            serverIP = argv[2];
            request = argv[1];
            if (request != "LS") {
                std::cerr << "[ERROR] TFTP Client : Invalid Request Type { READ | WRITE | DELETE | LS }";
                std::cout << "TFTP Client : Invalid Request Type { READ | WRITE | DELETE | LS }";
                break;
            }
            opcode = TFTP_OPCODE_LS;
            break;
        default:
            std::cerr << "[ERROR] TFTP Client : Invalid Request Type { READ | WRITE | DELETE | LS }";
            std::cout << "TFTP Client : Invalid Request Type { READ | WRITE | DELETE | LS }";
            break;
    }

    TFTPClient client(serverIP);
    client.startClient(opcode, filename);
}
