#ifndef TFTP_CLIENT_H
#define TFTP_CLIENT_H

#include <string>
#include <netinet/in.h>

class TFTPClient {
public:
    TFTPClient(const std::string& serverIP, int serverPort);
    void read(const std::string& filename);
    bool writeFile(const std::string& localFile, const std::string& remoteFile);

private:
    std::string serverIP;
    int serverPort;
    int clientSocket;
    struct sockaddr_in serverAddress;
    void sendReadRequest(const std::string& filename);
    void receiveData();
    bool sendWriteRequest(const std::string& filename);
    bool receiveAck(int& blockNumber);
    bool receiveError(std::string& errorMsg);
};

#endif
