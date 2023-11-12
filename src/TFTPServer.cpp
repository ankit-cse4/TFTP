#include "TFTPServer.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <fstream>

/**
 * @brief Constructor for the TFTPServer class.
 *
 * This constructor initializes the TFTPServer with the specified port.
 * @action: Server is established and ready to accept clients
 * @param port The port on which the server will listen for TFTP requests.
 */
TFTPServer::TFTPServer(int port) : port(port), nextClientId(1) {
    // Create a UDP socket for the server.
    serverSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (serverSocket < 0) {
        std::cerr << "Error creating server socket" << std::endl;
        exit(1);
    }
    // Set up server address information.
    serverAddress.sin_family = AF_INET;             
    serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverAddress.sin_port = htons(69);
    // Bind the socket to the specified address and port.
    if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        std::cerr << "Error binding server socket" << std::endl;
        exit(1);
    }
    std::cout << "Server binded to port 69" << std::endl;
}

/**
 * @brief Handles the write request from a TFTP client.
 *
 * This function processes the write request from a TFTP client, receives data packets,
 * and writes the data to a file.
 *
 * @param clientSocket The socket used for communication with the TFTP client.
 * @param filename The name of the file to be written.
 * @param clientAddress The client's address information.
 * @param clientId The unique identifier for the client.
 * @param files A map containing information about existing files on the server.
 */
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


/**
 * @brief Sends an error packet to the TFTP client.
 *
 * This function constructs and sends an error packet to the TFTP client
 * with the specified error code and error message.
 *
 * @param clientSocket The socket used for communication with the TFTP client.
 * @param errorCode The TFTP error code to be sent in the error packet.
 * @param errorMsg The error message associated with the error code.
 * @param clientAddress The client's address information.
 */
void TFTPServer::sendError(int clientSocket, uint16_t errorCode, const std::string& errorMsg, struct sockaddr_in clientAddress) {
    // Calculate the size of the error packet
    uint8_t packet[4 + errorMsg.size() + 1];

    // Create the error packet with the specified error code and message
    TFTPPacket::createErrorPacket(packet, errorCode, errorMsg);

    // Add a null terminator to the error message
    packet[4 + errorMsg.size()] = '\0';

    // Send the error packet to the client
    if(sendto(clientSocket, packet, 4 + errorMsg.size() + 1, 0, (struct sockaddr*)&clientAddress, sizeof(clientAddress)) < 0) {
        std::cerr << "[ERROR] : fail to send error packet" << std::endl;
        return;
    }

    // Log the successful sending of the error packet
    std::cerr << "[LOG] : Error packet send to client " << clientAddress.sin_addr.s_addr << " with error code: " << errorCode << std::endl;
}

/**
 * @brief Handles a read request from a TFTP client.
 *
 * This function processes a read request from a TFTP client, reads the
 * specified file in blocks of 512 bytes, and sends the data to the client.
 *
 * @param clientSocket The socket used for communication with the TFTP client.
 * @param filename The name of the file requested by the client.
 * @param clientAddress The client's address information.
 * @param clientId The unique identifier for the client.
 * @param files A map containing information about the files being accessed.
 */
void TFTPServer::handleReadRequest(int clientSocket, const std::string& filename, struct sockaddr_in clientAddress, int clientId, std::map<std::string, int>& files) {
    // Set up file paths and open the requested file
    std::string directory = "serverDatabase/";
    std::string filePath = directory + filename;
    std::ifstream file(filePath, std::ios::binary);

    // Check if the file exists
    if (!fileExists(filename, files)) {
        // Send an error packet (File not found - Error Code 1)
        const std::string errorMessage = "File not found";
        sendError(clientSocket, ERROR_FILE_NOT_FOUND, errorMessage, clientAddress);
        return;
    }

    // Update the number of readers for the file
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
        // Prepare data packet
        uint8_t packet[516];
        memset(packet, 0, sizeof(packet));
        memset(dataBuffer, 0, 512);
        size_t dataSize = file.gcount();

        // Read data from the file
        dataSize = TFTPPacket::readDataBlock(filePath, blockNumber, dataBuffer, dataSize);
        if (dataSize == 0) {
            break; // End of file
        }
        std::cerr << "Data Size: " << dataSize << std::endl;

        // Create and send data packet
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

// Close the file
    file.close();

    // Check for maximum retry limit
    if (!retry)
    {
        std::cerr << "Max retry for receiving timeout exceeded. Shutting down server" << std::endl;
    }

    // Update the number of readers for the file
    readers = files[filename];
    readers--;
    auto iter = files.find(filename);
    files.erase(iter);
    files.insert(std::make_pair(filename, readers));
}

/**
 * @brief Sends an ACK (Acknowledgment) packet to the TFTP client.
 *
 * This function creates and sends an ACK packet to acknowledge the successful
 * reception of a data packet from the TFTP client.
 *
 * @param clientSocket The socket used for communication with the TFTP client.
 * @param blockNumber The block number being acknowledged.
 * @param clientAddress The client's address information.
 */
void TFTPServer::sendACK(int clientSocket, uint16_t blockNumber, struct sockaddr_in clientAddress) {
    // Prepare the ACK packet
    uint8_t packet[4];
    TFTPPacket::createACKPacket(packet, blockNumber);

    // Send the ACK packet
    if (sendto(clientSocket, packet, sizeof(packet), 0, (struct sockaddr*)&clientAddress, sizeof(clientAddress)) < 0) {
        std::cerr << "[ERROR] : fail to send ACK packet" << std::endl;
        return;
    }

    // Log the successful ACK packet transmission
    std::cerr << "[LOG] : ACK packet send to client " << clientAddress.sin_addr.s_addr << " with Block Number: " << blockNumber << std::endl;
}

/**
 * @brief Handles the TFTP client request in a separate thread.
 *
 * This function is responsible for handling the TFTP client request based on the
 * provided opcode. It supports Read Request (RRQ), Write Request (WRQ), Delete Request (DELETE),
 * List Files Request (LS), and sends an error packet for an illegal TFTP operation.
 *
 * @param serverThreadSocket The socket for communication with the TFTP client in the thread.
 * @param filename The requested filename for RRQ, WRQ, or DELETE operations.
 * @param clientAddress The client's address information.
 * @param clientId The unique identifier for the client thread.
 * @param opcode The TFTP operation code received from the client.
 * @param clientThreads A map containing client thread information.
 * @param files A map containing information about files on the server.
 */
void TFTPServer::handleClientThread(int serverThreadSocket, const std::string& filename, struct sockaddr_in clientAddress,  int clientId, uint16_t opcode, std::map<int, std::tuple<std::thread, bool>>& clientThreads, std::map<std::string, int>& files) {
    // Set a receive timeout of 5 seconds for the socket
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


/**
 * @brief Destroys completed client threads after a specified interval.
 *
 * This function periodically checks for completed client threads, joins them, and removes
 * their entries from the clientThreads map. It repeats this process until the destroyTFTPServer
 * flag is set to true.
 *
 * @param clientThreads A map containing information about active client threads.
 */
void TFTPServer::destroyClientThreads(std::map<int, std::tuple<std::thread, bool>>& clientThreads){
    std::cerr << "[LOG]: destroy client thread started" << std::endl;
    std::vector<int> keyToDelete;
    // Sleep for 15 seconds
    // Periodically check for completed threads and join them
    while(!destroyTFTPServer){
        std::this_thread::sleep_for(std::chrono::seconds(15));

        // Iterate through clientThreads map
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

        // Remove completed threads from the clientThreads map
        for(int value : keyToDelete){
            auto it = clientThreads.find(value);
            if (it != clientThreads.end()) {
                clientThreads.erase(it);
                std::cerr << "Removed the thread id from Map: " << value << std::endl;
            }
        }
        keyToDelete.clear();
    }

    // Join any remaining threads when the destroyTFTPServer flag is set
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

/**
 * @brief Signal handler function for shutting down the TFTP server.
 *
 * This function is invoked when a specific signal is received. It prompts the user to confirm
 * whether they want to shut down the server. If the user chooses to shut down, the `destroyTFTPServer`
 * flag is set to true, indicating that the server should be shut down.
 *
 * @param signum The signal number.
 * @param info Signal information.
 * @param ptr Additional signal-related data.
 */
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
    // Perform server shutdown
    destroyTFTPServer = true;
    return;
}

/**
 * @brief Start the TFTP server and handle incoming requests.
 *
 * This function initializes the server, sets up signal handling for server shutdown,
 * and enters a loop to listen for incoming TFTP requests. It creates threads to handle
 * different types of requests and manages the server's lifecycle.
 */
void TFTPServer::start() {
    destroyTFTPServer = DESTROY_SERVER;

    // Create a thread to destroy client threads periodically
    std::thread destroyThread([this] {
        destroyClientThreads(clientThreads);
    });

    // Set up signal handling for graceful shutdown
    struct sigaction act;
	memset(&act,0,sizeof(act));
	act.sa_handler = SIG_IGN;
	act.sa_flags = SA_SIGINFO;
	act.sa_sigaction = &destroyTFTPHandler;
	sigaction(SIGINT, &act, NULL);

    // Initialize the file map
    initializeFileMap(files);

    std::cerr << "Server is started and waiting to recieve data" << std::endl;
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
        // Receive data from clients
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

        // Extract the opcode from the received packet
        uint16_t opcode = (uint16_t)(((buffer[1] & 0xFF) << 8) | (buffer[0] & 0XFF));
        opcode = ntohs(opcode);
        std::cerr << "Opcode:" << opcode << std::endl;

        // Handle the list files request
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
        // Handle RRQ, WRQ, or DELETE request
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

            // Check if the mode is "octet"
            if (mode != "octet"){
                const std::string errorMessage = "Illegal TFTP operation";
                sendError(serverSocket, ERROR_ILLEGAL_TFTP_OPERATION, errorMessage, clientAddress);
                continue;
            }
            // Create a thread to handle the client request
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
            // Incorrect opcode received
            std::cerr << "Incorrect opcode recieved" << std::endl;
            const std::string errorMessage = "Illegal TFTP operation";
            sendError(serverSocket, ERROR_ILLEGAL_TFTP_OPERATION, errorMessage, clientAddress);
            continue;
        }
    }
    // Check if the server is shutting down
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
    
    // Terminate the server process
    kill(getpid(),SIGTERM);
}

/**
 * @brief Signal handler for destroying the TFTP server.
 *
 * This function is called when a specific signal is received. It forwards the
 * call to the `destroyTFTP` method of the static instance of the TFTPServer class.
 *
 * @param signo The signal number.
 * @param info Signal information.
 * @param context Signal context.
 */
void TFTPServer::destroyTFTPHandler(int signo, siginfo_t* info, void* context) {
    // Forward the call to an instance of the class
    staticInstance->destroyTFTP(signo, info, context);
}


/**
 * @brief Get the static instance of the TFTPServer class.
 *
 * This method returns a reference to the static instance of the TFTPServer class.
 *
 * @return A reference to the static instance of the TFTPServer class.
 */
TFTPServer*& TFTPServer::getStaticInstance() {
    return staticInstance;
}


/**
 * Static member initialization. It will be used as a static instance of the TFTPServer class.
 */
TFTPServer* TFTPServer::staticInstance = nullptr;


/**
 * @brief Initialize the file map with filenames and initial reader count.
 *
 * This method populates the provided map with filenames from a specified directory
 * and initializes the associated reader count to zero.
 *
 * @param files The map to be initialized with filenames and initial reader count.
 */
void TFTPServer::initializeFileMap(std::map<std::string, int>& files) {
    // Specify the directory path where files are located
    std::string directoryPath = "serverDatabase";
    std::cerr << "Initializing files in the File Map" << std::endl;

    // Iterate over the files in the specified directory
    for (const auto& entry : fs::directory_iterator(directoryPath)) {
        if (entry.is_regular_file()) {
            // Insert the filename into the map with an initial reader count of 0
            files.insert(std::make_pair(entry.path().filename().string(), 0));
        }
    }
    std::cerr << "Initialized files in the map" << std::endl;
    
    // Display the initialized files and their reader counts
    for (const auto& pair : files) {
        std::cerr << "FILE NAME: " << pair.first << ", READERS: " << pair.second << std::endl;
    }
}


/**
 * @brief Check if a file exists in the server database.
 *
 * This function checks whether the specified filename exists in the provided map.
 *
 * @param filename The name of the file to check.
 * @param files The map containing filenames and associated reader counts.
 * @return True if the file exists, false otherwise.
 */
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

/**
 * @brief Check if a file can be deleted based on its existence and active readers.
 *
 * This function checks whether the specified filename exists in the provided map
 * and if it has no active readers (reader count is zero).
 *
 * @param filename The name of the file to check for deletion.
 * @param files The map containing filenames and associated reader counts.
 * @return True if the file can be deleted, false otherwise.
 */
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


/**
 * @brief Handle the DELETE request from the client.
 *
 * This function processes the client's request to delete a file.
 *
 * @param clientSocket The socket connected to the client.
 * @param filename The name of the file to be deleted.
 * @param clientAddress The client's address information.
 * @param clientId The unique identifier for the client.
 * @param files The map containing filenames and associated reader counts.
 */
void TFTPServer::handleDeleteRequest(int clientSocket, const std::string& filename, struct sockaddr_in clientAddress,  int clientId, std::map<std::string, int>& files) {
    // Check if the file exists in the server's database
    if (!fileExists(filename, files)){
        // send error regarding file not exists.
        const std::string errorMessage = "File not found";
        sendError(clientSocket, ERROR_FILE_NOT_FOUND, errorMessage, clientAddress);
        return;
    }
    else if (!canDelete(filename, files)) {
        // Send an error packet (File has active readers - Error Code 0, Not defined in RFC)
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


/**
 * @brief Handle the LS request from the client.
 *
 * This function processes the client's request to list files and active readers.
 *
 * @param clientSocket The socket connected to the client.
 * @param clientAddress The client's address information.
 * @param clientId The unique identifier for the client.
 * @param files The map containing filenames and associated reader counts.
 */
void TFTPServer::handleLSRequest(int clientSocket, struct sockaddr_in clientAddress,  int clientId, std::map<std::string, int>& files) {
    // Create a file to store the list of files and their reader counts
    std::string filepath = "serverDatabase/ls.txt";
    std::ofstream outputFile(filepath, std::ios::binary);
    if (!outputFile) {
        std::cerr << "Error creating the file." << std::endl;
        return;
    }

    // Write file information to the ls.txt file
    for (const auto& pair : files) {
        outputFile << pair.first << "\t [Active Readers] : " << pair.second << "\n";
    }
    outputFile.close();
    std::cerr << "[LOG] << ls.txt created and updated successfully." << std::endl;
    uint16_t blockNumber = 1;
    int retry = MAX_RETRY;
    std::ifstream file(filepath, std::ios::binary);
    // Continue sending data blocks until the file is fully transmitted
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

        // Create a data packet with file information
        std::cerr << "Data Size: " << dataSize << std::endl;
        TFTPPacket::createDataPacket(packet, blockNumber, dataBuffer, dataSize);
        std::cerr << "Data packet created" << std::endl;

        // Send the data packet to the client
        if (sendto(clientSocket, packet, dataSize + 4, 0, (struct sockaddr*)&clientAddress, sizeof(clientAddress)) < 0) {
            std::cerr << "[ERROR] : fail to send Data packet" << std::endl;
            return;
        }
        std::cerr << "[LOG] : Data packet send to client " << clientAddress.sin_addr.s_addr << std::endl;
        char ackBuffer[4];

        // Wait for acknowledgment from the client
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
    server.start();
    std :: cout << "server is started" << std::endl;
    return 0;
}

