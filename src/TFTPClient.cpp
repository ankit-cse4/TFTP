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
    serverAddress.sin_addr.s_addr = inet_addr(serverIP);
    serverAddress.sin_port = htons(SERVER_DEFAULT_PORT);

    struct timeval timeout;
    timeout.tv_sec = 5;  // seconds
    timeout.tv_usec = 0; // microseconds

    if (setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
            std::cerr << "Error setting receive timeout" << std::endl;
            break;
        }
    std::cerr << "[LOG] Socket timeout set successfully" << std::endl;
    

}

bool TFTPClient::writeFile(const std::string& localFile, const std::string& remoteFile) {
    // Send a Write Request (WRQ) packet
    if (!sendWriteRequest(remoteFile)) {
        std::cerr << "Error sending Write Request" << std::endl;
        return false;
    }

    // Receive ACK for the WRQ
    int blockNumber = 0;
    if (!receiveAck(blockNumber)) {
        std::cerr << "Error receiving ACK for WRQ" << std::endl;
        return false;
    }

    // Open the local file for reading
    std::ifstream file(localFile, std::ios::binary);
    if (!file) {
        std::cerr << "Error opening local file for reading" << std::endl;
        return false;
    }

    // TODO: Implement the logic to send file data as DATA packets and handle ACKs

    // Close the local file and end the transfer
    file.close();
    return true;
}

bool TFTPClient::sendWriteRequest(const std::string& filename) {
    // Construct and send a WRQ packet
    uint16_t opcode = 2; // WRQ opcode
    std::string mode = "octet"; // Binary mode
    std::string packet = createRequestPacket(opcode, filename, mode);

    return sendData(serverSocket, packet);
}

bool TFTPClient::receiveAck(int& blockNumber) {
    // TODO: Implement the logic to receive and handle ACK packets
}

bool TFTPClient::receiveError(std::string& errorMsg) {
    // TODO: Implement the logic to receive and handle ERROR packets
}

void TFTPClient::sendReadRequest(const std::string& filename) {
    // Implement logic for sending RRQ
}

void TFTPClient::receiveData() {
    // Implement logic for receiving and processing data packets
}

void TFTPClient::read(const std::string& filename) {
    sendReadRequest(filename);
    receiveData();
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
    uint8_t packet[2 + sizeof(filename) + 1];
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
    serverAddress.sin_addr.s_addr = inet_addr(serverIP);
    serverAddress.sin_port = htons(SERVER_DEFAULT_PORT);

    // implement logic for handling client according to opcode
    if (opcode == TFTP_OPCODE_LS) {
    }
    switch (opcode)
    {
        case TFTP_OPCODE_LS:
            if (handleLSRequest(clientSocket, serverAddress))
            {
                std::cout << "Output is stored in file ls.txt in /clientDatabase directory." << std::endl;
                std::cerr << "exiting" << std::endl;
                return;
            }
            std::cout << "[ERROR received] " << std::endl;
            return;
        case TFTP_OPCODE_DELETE:
            if (handleDELETERequest(clientSocket, serverAddress, filename))
            {
                std::cout << "Output is stored in file ls.txt in /clientDatabase directory." << std::endl;
                std::cerr << "exiting" << std::endl;
                return;
            }
            std::cout << "[ERROR received] " << std::endl;
            return;
        case TFTP_OPCODE_RRQ:
            break;
        case TFTP_OPCODE_WRQ:
            break;
        default:
            break;
    } 
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
        std::cerr << "[ERROR] : filename is empty"
        return false;
    }
    if (!sendDELETEPacket(clientSocket, serverAddress, filename))
    {
        std::cerr << "[ERROR] : fail to send DELETE packet" << std::endl;
        return false;
    }
    std::cerr << "[LOG] : sent DELETE packet" << std::endl;
    
    
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
        return;
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
            std::cerr << "Invalid acknowledgment received" << std::endl;
            const std::string errorMessage = "Illegal TFTP operation";
            sendError(clientSocket, ERROR_ACCESS_VIOLATION, errorMessage, recvAddress);
            continue;
        }
        else {
            retry = MAX_RETRY;
        }
        std::cerr << "Received data" << std::endl;
        if (recvAddress.sin_addr.sin_addr != inet_addr(serverIP))
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
    switch (argc)
    {
    case 4:
        std::string serverIP = argv[3];
        std::string request = argv[1];
        std::string filename = argv[2];
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
        TFTPClient client(serverIP);
        client.startClient(opcode, filename);
        break;
    case 3:
        std::string serverIP = argv[2];
        std::string request = argv[1];
        std::string filename;
        if (request != "LS") {
            std::cerr << "[ERROR] TFTP Client : Invalid Request Type { READ | WRITE | DELETE | LS }";
            std::cout << "TFTP Client : Invalid Request Type { READ | WRITE | DELETE | LS }";
            break;
        }
        opcode = TFTP_OPCODE_LS;
        TFTPClient client(serverIP);
        client.startClient(opcode, filename);
        break;
    default:
        std::cerr << "[ERROR] TFTP Client : Invalid Request Type { READ | WRITE | DELETE | LS }";
        std::cout << "TFTP Client : Invalid Request Type { READ | WRITE | DELETE | LS }";
        break;
    }
}
