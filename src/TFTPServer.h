#ifndef TFTP_SERVER_H
#define TFTP_SERVER_H

#include <string>
#include <vector>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>
#include <map>
#include "TFTPPacket.h"

#define DESTROY_SERVER false

class TFTPServer {
public:
    TFTPServer(int port);
    void start();
private:
    int port;
    int serverSocket;
    struct sockaddr_in serverAddress;
    // void bindSocket();
    void handleRequest(int clientSocket, struct sockaddr_in clientAddress);
    void handleReadRequest(int clientSocket, const std::string& filename, struct sockaddr_in clientAddress,  int clientId);
    void sendData(int clientSocket, const char* data, int dataSize, struct sockaddr_in clientAddress);
    void sendACK(int clientSocket, uint16_t blockNumber, struct sockaddr_in clientAddress);
    void handleWriteRequest(int clientSocket, const std::string& filename, struct sockaddr_in clientAddress,  int clientId);
    void sendError(int clientSocket, uint16_t errorCode, const std::string& errorMsg, struct sockaddr_in clientAddress);
    void handleClientThread(struct sockaddr_in clientAddress, int clientId, uint16_t opcode, const std::string& filename, std::map<int, std::tuple<std::thread, bool>>& clientThreads);
    void destroyClientThreads(std::map<int, std::tuple<std::thread, bool>>& clientThreads);
    std::map<int, std::tuple<std::thread, bool>> clientThreads;
    int nextClientId;
};

#endif
