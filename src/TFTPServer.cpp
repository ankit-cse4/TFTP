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
    serverAddress.sin_port = htons(69);
    if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        std::cerr << "Error binding server socket" << std::endl;
        exit(1);
    }
    std::cout << "Server binded to port 69" << std::endl;
}


void TFTPServer::handleWriteRequest(int clientSocket, const std::string& filename, struct sockaddr_in clientAddress, int clientId, std::map<std::string, int>& files) {
    std::string directory = "serverDatabase/";
    std::string filePath = directory + filename;
    if (fileExists(filename, files)) {
        // Send an error packet (File already exists. - Error Code 6)
        const std::string errorMessage = "File already exists.";
        sendError(clientSocket, ERROR_FILE_ALREADY_EXISTS, errorMessage, clientAddress);
        return;
    }
    std::ofstream file(filePath, std::ios::binary); 
    if (!file) {
        // Send an error packet (Disk full or allocation exceeded - Error Code 3)
        const std::string errorMessage = "Disk full or allocation exceeded.";
        sendError(clientSocket, ERROR_DISK_FULL, errorMessage, clientAddress);
        return;
    }

    uint16_t blockNumber = 0;
    char recievedBuffer[516];
    memset(recievedBuffer, 0, sizeof(recievedBuffer));
    sendACK(clientSocket, blockNumber, clientAddress);
    uint16_t expectedBlockNumber = 1;
    int retry = MAX_RETRY;
    while (retry)
    {
        int readBytes = recvfrom(clientSocket, recievedBuffer, sizeof(recievedBuffer), 0, nullptr, nullptr);
        if (readBytes < 0) {
            std::cerr << "TIMEOUT occured" << std::endl;
            retry--;
            continue;
        }
        else if (readBytes > 516)
        {
            std::cerr << "Illegal Packet Recieved" << std::endl;
            std::string errorMessage = "Illegal TFTP operation";
            sendError(clientSocket, ERROR_ILLEGAL_TFTP_OPERATION, errorMessage, clientAddress);
            continue;
        }
        else {
            retry = MAX_RETRY;
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
        // std::cerr << "Received Data: " << std::endl << std::endl << recvData << std::endl;
        std::cerr << "Writing data in file" << std::endl;
        file.write(reinterpret_cast<char*>(recvData), dataLength);
        if (file.fail())
        {
            std::cerr << "File write error" << std::endl;
            const std::string errorMessage = "Disk full or allocation exceeded.";
            sendError(clientSocket, ERROR_DISK_FULL, errorMessage, clientAddress);
            // code logic for rewrite.
            break;
        }
        sendACK(clientSocket, expectedBlockNumber, clientAddress);
        expectedBlockNumber++;
        if(dataLength < 512) {
            std::cerr << "File recieved Successfuly." << std::endl;
            files.insert(std::make_pair(filename, 0));
            break;
        }
    }
    // TODO: Handle the write operation
    file.close();
    if (!retry)
    {
        std::cerr << "Max retry for receiving timeout exceeded. Shutting down server" << std::endl;
    }
    
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

void TFTPServer::handleReadRequest(int clientSocket, const std::string& filename, struct sockaddr_in clientAddress, int clientId, std::map<std::string, int>& files) {
    std::string directory = "serverDatabase/";
    std::string filePath = directory + filename;
    std::ifstream file(filePath, std::ios::binary);
    if (!fileExists(filename, files)) {
        // Send an error packet (File not found - Error Code 1)
        const std::string errorMessage = "File not found";
        sendError(clientSocket, ERROR_FILE_NOT_FOUND, errorMessage, clientAddress);
        return;
    }
    int readers = files[filename];
    readers++;
    auto it = files.find(filename);
    files.erase(it);
    files.insert(std::make_pair(filename, readers));
    // Read and send data in blocks of 512 bytes
    uint16_t blockNumber = 1;
    char dataBuffer[512];
    int retry = MAX_RETRY;
    while (retry) {
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
            std::cerr << "TIMEOUT Occured" << std::endl;
            retry--;
            continue;
        }
        else if (ackSize > MAX_PACKET_SIZE) {
            std::cerr << "Invalid acknowledgment received" << std::endl;
            const std::string errorMessage = "Illegal TFTP operation";
            sendError(clientSocket, ERROR_ACCESS_VIOLATION, errorMessage, clientAddress);
            continue;
        }
        else {
            retry = MAX_RETRY;
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
    if (!retry)
    {
        std::cerr << "Max retry for receiving timeout exceeded. Shutting down server" << std::endl;
    }
    readers = files[filename];
    readers--;
    auto iter = files.find(filename);
    files.erase(iter);
    files.insert(std::make_pair(filename, readers));
}


void TFTPServer::sendACK(int clientSocket, uint16_t blockNumber, struct sockaddr_in clientAddress) {
    uint8_t packet[4];
    TFTPPacket::createACKPacket(packet, blockNumber);
    if (sendto(clientSocket, packet, sizeof(packet), 0, (struct sockaddr*)&clientAddress, sizeof(clientAddress)) < 0) {
        std::cerr << "[ERROR] : fail to send ACK packet" << std::endl;
        return;
    }
    std::cerr << "[LOG] : ACK packet send to client " << clientAddress.sin_addr.s_addr << " with Block Number: " << blockNumber << std::endl;
}

void TFTPServer::handleClientThread(int serverThreadSocket, const std::string& filename, struct sockaddr_in clientAddress,  int clientId, uint16_t opcode, std::map<int, std::tuple<std::thread, bool>>& clientThreads, std::map<std::string, int>& files) {

    struct timeval timeout;
    timeout.tv_sec = 5;  // seconds
    timeout.tv_usec = 0; // microseconds
    if (setsockopt(serverThreadSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        std::cerr << "Error setting receive timeout" << std::endl;
        return;
    }

    // Handle RRQ request (Opcode 1)
    if (opcode == TFTP_OPCODE_RRQ) {
        handleReadRequest(serverThreadSocket, filename, clientAddress, clientId, files);
    }
    // Handle WRQ request (Opcode 2)
    else if (opcode == TFTP_OPCODE_WRQ) {
        handleWriteRequest(serverThreadSocket, filename, clientAddress, clientId, files);
    }
    else if(opcode == TFTP_OPCODE_DELETE) {
        handleDeleteRequest(serverThreadSocket, filename, clientAddress, clientId, files); 
    }
    else if (opcode == TFTP_OPCODE_LS) {
        // handle the list files
        handleLSRequest(serverThreadSocket, clientAddress, clientId, files);
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
    std::cerr << "[LOG]: destroy client thread started" << std::endl;
    std::vector<int> keyToDelete;
    // Sleep for 30 seconds
    while(!destroyTFTPServer){
        std::this_thread::sleep_for(std::chrono::seconds(15));
        for (auto& pair : clientThreads) {
            int key = pair.first;
            auto& threadTuple = pair.second;
            if (std::get<1>(threadTuple))
            {
                // Join the thread
                std::get<0>(threadTuple).join();
                std::cerr << " joined Thread ID: " << key << std::endl;
                // clientThreads.erase(key);
                keyToDelete.push_back(key);
            }
        }
        for(int value : keyToDelete){
            auto it = clientThreads.find(value);
            if (it != clientThreads.end()) {
                clientThreads.erase(it);
                std::cerr << "Removed the thread id from Map: " << value << std::endl;
            }
        }
        keyToDelete.clear();
    }
    for (auto& pair : clientThreads) {
        int key = pair.first;
        auto& threadTuple = pair.second;
        // Join the thread
        std::get<0>(threadTuple).join();
        std::cerr << " joined Thread ID: " << key << std::endl;
    }
    clientThreads.clear();
    std::cerr << "All threads joined" << std::endl;
}

void TFTPServer::destroyTFTP(int signum, siginfo_t* info, void* ptr) {
    char ans = '0';
shutdown_prompt:
	std::cout << "tftp> Would you like to shutdown the Server? (y/n): ";
	std::cin >> ans;
	switch(ans){
		case 'n':
			return;
		case 'y':
			break;
		default:
			goto shutdown_prompt;
	}
    std::cout << "Shutting Down the TFTP server" << std::endl;
    destroyTFTPServer = true;
    return;
}

void TFTPServer::start() {
    destroyTFTPServer = DESTROY_SERVER;
    std::thread destroyThread([this] {
        destroyClientThreads(clientThreads);
    });
    struct sigaction act;
	memset(&act,0,sizeof(act));
	act.sa_handler = SIG_IGN;
	act.sa_flags = SA_SIGINFO;
	act.sa_sigaction = &destroyTFTPHandler;
	sigaction(SIGINT, &act, NULL);

    initializeFileMap(files);

    std::cerr << "Server is started and waiting to recieve data" << std::endl;
    // bindSocket();
    struct timeval timeout;
    timeout.tv_sec = 5;  // seconds
    timeout.tv_usec = 0; // microseconds
    while (!destroyTFTPServer) {
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

        if (opcode == TFTP_OPCODE_LS)
        {
            int clientId = 9800 + nextClientId++;
            std::string filename;
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
                handleClientThread(serverThreadSocket, filename, clientAddress, clientId, opcode, clientThreads, files);
                
            }), false);

        }
        // Handle RRQ request (Opcode 1)
        else if (opcode == TFTP_OPCODE_RRQ || opcode == TFTP_OPCODE_WRQ || opcode == TFTP_OPCODE_DELETE) {
            int clientId = 9800 + nextClientId++;
            std::cerr << "buffer read: " << buffer << std::endl;
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
                handleClientThread(serverThreadSocket, filename, clientAddress, clientId, opcode, clientThreads, files);
                
            }), false);
            
        }
        else {
            std::cerr << "Incorrect opcode recieved" << std::endl;
            const std::string errorMessage = "Illegal TFTP operation";
            sendError(serverSocket, ERROR_ILLEGAL_TFTP_OPERATION, errorMessage, clientAddress);
            continue;
        }
    }
    if(destroyTFTPServer) {
        std::cerr << "Shutting Down Server...." << std::endl;
    }
    else {
        std::cerr << "Error Occured. Force shutdown server" << std::endl;
    }
    std::cerr << "All threads joined" << std::endl;
    destroyThread.join();
    std::cerr << "destroy threads thread joined" << std::endl;
    std::cerr << "Server shut down process completed" << std::endl;
    kill(getpid(),SIGTERM);
}

void TFTPServer::destroyTFTPHandler(int signo, siginfo_t* info, void* context) {
    // Forward the call to an instance of the class
    staticInstance->destroyTFTP(signo, info, context);
}

TFTPServer*& TFTPServer::getStaticInstance() {
    return staticInstance;
}

TFTPServer* TFTPServer::staticInstance = nullptr;

void TFTPServer::initializeFileMap(std::map<std::string, int>& files) {
    std::string directoryPath = "serverDatabase";
    std::cerr << "Initializing files in the File Map" << std::endl;
    for (const auto& entry : fs::directory_iterator(directoryPath)) {
        if (entry.is_regular_file()) {
            files.insert(std::make_pair(entry.path().filename().string(), 0));
            // files.push_back(entry.path().filename().string());
        }
    }
    std::cerr << "Initialized files in the map" << std::endl;
    for (const auto& pair : files) {
        std::cerr << "FILE NAME: " << pair.first << ", READERS: " << pair.second << std::endl;
    }
}

bool TFTPServer::fileExists(const std::string& filename, std::map<std::string, int>& files) {
    auto it = files.find(filename);
    if(it != files.end()) {
        std::cerr << filename << " exists in the server database" << std::endl;
        return true;
    }
    else {
        std::cerr << filename << " does not exists in the server database" << std::endl;
        return false;
    }
}


bool TFTPServer::canDelete(const std::string& filename, std::map<std::string, int>& files) {
    auto it = files.find(filename);
    if(it == files.end()) {
        std::cerr << filename << " does not exists in the server database" << std::endl;
        return false;
    }

    if (files[filename] == 0){
        std::cerr << filename << " has no active readers" << std::endl;
        return true;
    }
    else {
        std::cerr << filename << " has active readers. Can not delete." << std::endl;
        return false;
    }
}

void TFTPServer::handleDeleteRequest(int clientSocket, const std::string& filename, struct sockaddr_in clientAddress,  int clientId, std::map<std::string, int>& files) {
    if (!fileExists(filename, files)){
        // send error regarding file not exists.
        const std::string errorMessage = "File not found";
        sendError(clientSocket, ERROR_FILE_NOT_FOUND, errorMessage, clientAddress);
        return;
    }
    else if (!canDelete(filename, files)) {
        //send error regarding active readers. cannot delete file.
        const std::string errorMessage = "File has active readers. Cannot delete file.";
        sendError(clientSocket, ERROR_NOT_DEFINED, errorMessage, clientAddress);
        return;
    }
    std::string directory = "serverDatabase/";
    std::string filePath = directory + filename;
    try {
        // Check if the file exists before attempting to delete
        if (fs::exists(filePath)) {
            fs::remove(filePath);
            std::cout << "File deleted successfully.\n";
            auto it = files.find(filename);
            files.erase(it);
            // send ack that file deleted succesfully.
            sendACK(clientSocket, ACK_OK, clientAddress);
            return;
        } else {
            const std::string errorMessage = "File not found";
            sendError(clientSocket, ERROR_FILE_NOT_FOUND, errorMessage, clientAddress);
            return;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        const std::string errorMessage = "Exception Occured. cannot delete file";
        sendError(clientSocket, ERROR_NOT_DEFINED, errorMessage, clientAddress);
        return;
    }
}

void TFTPServer::handleLSRequest(int clientSocket, struct sockaddr_in clientAddress,  int clientId, std::map<std::string, int>& files) {
    std::string filepath = "serverDatabase/ls.txt";
    std::ofstream outputFile(filepath, std::ios::binary);
    if (!outputFile) {
        std::cerr << "Error creating the file." << std::endl;
        return;
    }
    for (const auto& pair : files) {
        outputFile << pair.first << "\t [Active Readers] : " << pair.second << "\n";
    }
    outputFile.close();
    std::cerr << "[LOG] << ls.txt created and updated successfully." << std::endl;
    // std::ifstream file(filepath, std::ios::binary);
    // if (!file.is_open()) {
    //     std::cerr << "Error opening the LS file." << std::endl;
    //     return;
    // }
    uint16_t blockNumber = 1;
    int retry = MAX_RETRY;
    std::ifstream file(filepath, std::ios::binary);
    while (retry)
    {
        char dataBuffer[512];
        uint8_t packet[516];
        memset(packet, 0, sizeof(packet));
        memset(dataBuffer, 0, 512);
        size_t dataSize = file.gcount();
        dataSize = TFTPPacket::readDataBlock(filepath, blockNumber, dataBuffer, dataSize);
        std::cerr << "data of size: " << dataSize << " read successfully. " << dataBuffer << std::endl;
        if (dataSize == 0) {
                std::cerr << "datasize = 0" << std::endl;
                break; // End of file
        }
        std::cerr << "Data Size: " << dataSize << std::endl;
        TFTPPacket::createDataPacket(packet, blockNumber, dataBuffer, dataSize);
        std::cerr << "Data packet created" << std::endl;
        if (sendto(clientSocket, packet, dataSize + 4, 0, (struct sockaddr*)&clientAddress, sizeof(clientAddress)) < 0) {
            std::cerr << "[ERROR] : fail to send Data packet" << std::endl;
            return;
        }
        std::cerr << "[LOG] : Data packet send to client " << clientAddress.sin_addr.s_addr << std::endl;
        char ackBuffer[4];
        int ackSize = recvfrom(clientSocket, ackBuffer, sizeof(ackBuffer), 0, nullptr, nullptr);
        if (ackSize < 0) {
            std::cerr << "TIMEOUT Occured" << std::endl;
            retry--;
            continue;
        }
        else if (ackSize > MAX_PACKET_SIZE) {
            std::cerr << "Invalid acknowledgment received" << std::endl;
            const std::string errorMessage = "Illegal TFTP operation";
            sendError(clientSocket, ERROR_ACCESS_VIOLATION, errorMessage, clientAddress);
            break;
        }
        else {
            retry = MAX_RETRY;
        }
        std::cerr << "Acknoledment recieved" <<std::endl;
        // Check the received acknowledgment packet (Opcode 4)
        uint16_t ackOpcode = (ackBuffer[0] << 8) | ackBuffer[1];
        uint16_t ackBlockNumber = (ackBuffer[2] << 8) | ackBuffer[3];
        if (ackOpcode != 4 || ackBlockNumber != blockNumber) {
            std::cerr << "Invalid acknowledgment received" << std::endl;
            // const std::string errorMessage = "Illegal TFTP operation";
            // sendError(clientSocket, ERROR_ACCESS_VIOLATION, errorMessage, clientAddress);
            continue;
        }
        std::cerr<< "Correct ACK packet recieved for blocknumber: " << ackBlockNumber << std::endl;

        if(dataSize < 512) {
            break;
        }

        ++blockNumber;
    
    }
    if (!retry)
    {
        std::cerr << "Max retry for receiving timeout exceeded. Shutting down server" << std::endl;
    }

}

int main() {
    std :: cout << "starting the server" << std::endl;
    TFTPServer server(54534);
    TFTPServer::getStaticInstance() = &server;
    std :: cout << "initialized the server" << std::endl;
    // start a thread that will destroy threads, pass the map as argument, 
    server.start();
    std :: cout << "server is started" << std::endl;
    return 0;
}

