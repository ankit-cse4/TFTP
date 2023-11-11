#ifndef TFTP_CLIENT_H
#define TFTP_CLIENT_H

#include <string>
#include "TFTPPacket.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <filesystem>
#include <fstream>


#define SERVER_DEFAULT_PORT     69
#define MAX_RETRY               5
#define CLIENT_DEFAULT_PORT     9799

std::string DEFAULT_LS_FILE_NAME = "ls.txt";

class TFTPClient {
public:
    TFTPClient(const std::string& serverIP);
    struct sockaddr_in serverAddress;
    struct sockaddr_in clientAddress;
    void startClient(int opcode, const std::string& filename);

private:
    std::string serverIP;
    int serverPort;
    int clientSocket;
    bool handleRRQRequest(int clientSocket, struct sockaddr_in serverAddress, const std::string& filename);
    bool sendRRQPacket(int clientSocket, struct sockaddr_in serverAddress, const std::string& filename);
    bool handleWRQRequest(int clientSocket, struct sockaddr_in serverAddress, const std::string& filename);
    bool sendWRQPacket(int clientSocket, struct sockaddr_in serverAddress, const std::string& filename);
    bool handleLSRequest(int clientSocket, struct sockaddr_in serverAddress);
    bool sendLSPacket(int clientSocket, struct sockaddr_in serverAddress);
    bool handleDELETERequest(int clientSocket, struct sockaddr_in serverAddress, const std::string& filename);
    bool sendDELETEPacket(int clientSocket, struct sockaddr_in serverAddress, const std::string& filename);
    void sendACK(int clientSocket, uint16_t blockNumber, struct sockaddr_in serverAddress);
    void sendError(int clientSocket, uint16_t errorCode, const std::string& errorMsg, struct sockaddr_in clientAddress);
};

#endif
