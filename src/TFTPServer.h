#ifndef TFTP_SERVER_H
#define TFTP_SERVER_H

#include <string>
#include <vector>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>
#include <map>
#include <chrono>
#include <signal.h>
#include "TFTPPacket.h"

#define DESTROY_SERVER false
#define MAX_RETRY   5
#define DEFAULT_SLEEP_TIME 30

class TFTPServer {
public:
    TFTPServer(int port);
    static TFTPServer*& getStaticInstance(); 
    void start();
private:
    int port;
    int serverSocket;
    struct sockaddr_in serverAddress;
    static void destroyTFTPHandler(int signo, siginfo_t* info, void* context);
    void handleReadRequest(int clientSocket, const std::string& filename, struct sockaddr_in clientAddress,  int clientId);
    void sendACK(int clientSocket, uint16_t blockNumber, struct sockaddr_in clientAddress);
    void handleWriteRequest(int clientSocket, const std::string& filename, struct sockaddr_in clientAddress,  int clientId);
    void sendError(int clientSocket, uint16_t errorCode, const std::string& errorMsg, struct sockaddr_in clientAddress);
    void handleClientThread(int serverThreadSocket, const std::string& filename, struct sockaddr_in clientAddress,  int clientId, uint16_t opcode, std::map<int, std::tuple<std::thread, bool>>& clientThreads);
    void destroyClientThreads(std::map<int, std::tuple<std::thread, bool>>& clientThreads);
    void destroyTFTP(int signum, siginfo_t* info, void* ptr);
    std::map<int, std::tuple<std::thread, bool>> clientThreads;
    int nextClientId;
    bool destroyTFTPServer;
    static TFTPServer* staticInstance;
};

#endif
