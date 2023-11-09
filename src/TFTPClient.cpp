#include "TFTPClient.h"
#include <iostream>
#include <cstring>
#include <unistd.h>

TFTPClient::TFTPClient(const std::string& serverIP, int serverPort) : serverIP(serverIP), serverPort(serverPort) {
    clientSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (clientSocket < 0) {
        std::cerr << "Error creating client socket" << std::endl;
        exit(1);
    }

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = inet_addr(serverIP.c_str());
    serverAddress.sin_port = htons(serverPort);
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
