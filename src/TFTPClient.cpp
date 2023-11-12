#include "TFTPClient.h"
#include "TFTPCompression.h"
#include <iostream>
#include <cstring>
#include <unistd.h>







TFTPClient::TFTPClient(const std::string& serverIP) {
    // Create a UDP socket
    clientSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (clientSocket < 0) {
        std::cerr << "Error creating client socket" << std::endl;
        exit(1);
    }
    // Configure the server address
    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = inet_addr(serverIP.c_str());
    serverAddress.sin_port = htons(SERVER_DEFAULT_PORT);

    // Set a timeout for socket operations
    struct timeval timeout;
    timeout.tv_sec = 5;  // seconds
    timeout.tv_usec = 0; // microseconds

    // Connect to the server (optional, but can be useful for a connection-oriented protocol)
    if (setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
            std::cerr << "Error setting receive timeout" << std::endl;
            exit(1);
        }
    std::cerr << "[LOG] Socket timeout set successfully" << std::endl;

}

/**
 * @brief Sends a List (LS) packet to the TFTP server.
 *
 * This function constructs an LS packet using the TFTPPacket::createLSPacket method
 * and sends it to the specified server address using the provided client socket.
 *
 * @param clientSocket The socket descriptor for communication with the server.
 * @param serverAddress The server's sockaddr_in structure containing the IP address and port.
 * @return true if the LS packet is successfully sent, false otherwise.
 */
bool TFTPClient::sendLSPacket(int clientSocket, struct sockaddr_in serverAddress){
    uint8_t packet[3];
    TFTPPacket::createLSPacket(packet);
    if (sendto(clientSocket, packet, sizeof(packet), 0, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        std::cerr << "[ERROR] : fail to send LS packet" << std::endl;
        return false;
    }
    std::cerr << "[LOG] : LS packet send to server " << serverAddress.sin_addr.s_addr << std::endl;
    return true;
}


/**
 * @brief Sends a DELETE packet to the TFTP server.
 *
 * This function constructs a DELETE packet using the TFTPPacket::createDeletePacket method,
 * including the specified filename, and sends it to the server using the provided client socket.
 *
 * @param clientSocket The socket descriptor for communication with the server.
 * @param serverAddress The server's sockaddr_in structure containing the IP address and port.
 * @param filename The name of the file to be deleted.
 * @return true if the DELETE packet is successfully sent, false otherwise.
 */

bool TFTPClient::sendDELETEPacket(int clientSocket, struct sockaddr_in serverAddress, const std::string& filename){
    std::string mode = "octet";
    uint8_t packet[4 + filename.size() + mode.size()];
    std::cerr << "filename: " << filename << std::endl;
    TFTPPacket::createDeletePacket(packet, filename);
    std::cerr << "Packet content: ";
    std::cerr << std::hex << static_cast<uint8_t>(packet[0]) << std::hex << static_cast<uint8_t>(packet[0]);
    for (std::size_t i = 2; i < sizeof(packet); ++i) {
        std::cerr << packet[i];
    }
    std::cerr << std::endl;
    if (sendto(clientSocket, packet, sizeof(packet), 0, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        std::cerr << "[ERROR] : fail to send Delete packet" << std::endl;
        return false;
    }
    std::cerr << "[LOG] : Delete packet send to server " << serverAddress.sin_addr.s_addr << std::endl;
    return true;
}


/**
 * @brief Starts the TFTP client with the specified operation and filename.
 *
 * This function initializes the client's socket, binds it to a default port, and
 * communicates with the TFTP server based on the provided operation code (opcode) and filename.
 *
 * @param opcode The TFTP operation code (e.g., TFTP_OPCODE_LS, TFTP_OPCODE_DELETE, etc.).
 * @param filename The name of the file associated with the operation.
 */
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
    serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverAddress.sin_port = htons(69);

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


/**
 * @brief Handles Read Request (RRQ) operation with the TFTP server.
 *
 * This function sends a RRQ packet to the TFTP server, receives data blocks,
 * writes the data to a file, and performs decompression on the received file.
 *
 * @param clientSocket The socket descriptor for communication with the server.
 * @param serverAddress The server's sockaddr_in structure containing the IP address and port.
 * @param prevFilename The original filename associated with the RRQ request.
 * @return true if the RRQ operation is successful, false otherwise.
 */
bool TFTPClient::handleRRQRequest(int clientSocket, struct sockaddr_in serverAddress, const std::string& prevFilename) {
    if (prevFilename.empty())
    {
        std::cerr << "[ERROR] : filename is empty" << std::endl;
        return false;
    }
    std::string strFilename(prevFilename);
    std::string filenameWithoutExtension = strFilename.substr(0, strFilename.find_last_of("."));
    std::string filename = filenameWithoutExtension + "compress.bin";
    if (!sendRRQPacket(clientSocket, serverAddress, filename))
    {
        std::cerr << "[ERROR] : fail to send RRQ packet" << std::endl;
        return false;
    }
    std::cerr << "[LOG] : sent RRQ packet" << std::endl;
    // std::string directory = "clientDatabase/";
    // std::string filePath = directory + filename;
    std::ofstream file(filename, std::ios::binary);
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
        int readBytes = recvfrom(clientSocket, recievedBuffer, 516, 0, (struct sockaddr*)&recvAddress, &recvAddressLen);
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

        if (opcode != TFTP_OPCODE_ERROR && opcode != TFTP_OPCODE_DATA) {
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
            std::cerr << "Block number did no match" << std::endl;
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
        sendACK(clientSocket, recvBlockNumber, clientAddress);
        expectedBlockNumber++;
        if(dataLength < 512) {
            std::cerr << "File recieved Successfuly." << std::endl;
            file.close();
            std::cerr << "Starting decompression of filename: " << filename << std::endl;
            const char delimiter[] = {static_cast<char>(0x7F), static_cast<char>(0xFE)};
            size_t delimiterSize = sizeof(delimiter);
            separateBinaryFile(filename, "output_file1.bin", "output_file2.bin", delimiter, delimiterSize);
            std::map<char, std::string> result = decodeBinaryFileToMap("output_file1.bin");
            std::cerr << "Decompressed file" << std::endl;
            readBinaryFile("output_file2.bin");
            // call decompression function here
            inflate("output_file2.txt",result, prevFilename);
            std::string myStringArray[4] = {"output_file1.bin", "output_file2.bin", "output_file2.txt", "encoded_data.bin"};
            for (int i = 0; i < 4; i++)
            {
                if (fs::exists(myStringArray[i])) {
                    fs::remove(myStringArray[i]);
                }
            }
            std::cerr << "[DEBUG] : removed unnecessory files" << std::endl;

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


/**
 * @brief Sends a Read Request (RRQ) packet to the TFTP server.
 *
 * This function constructs a RRQ packet using the TFTPPacket::createRRQPacket method,
 * including the specified filename and transfer mode, and sends it to the server using
 * the provided client socket and server address.
 *
 * @param clientSocket The socket descriptor for communication with the server.
 * @param serverAddress The server's sockaddr_in structure containing the IP address and port.
 * @param filename The name of the file to be requested.
 * @return true if the RRQ packet is successfully sent, false otherwise.
 */

bool TFTPClient::sendRRQPacket(int clientSocket, struct sockaddr_in serverAddress, const std::string& filename) {
    std::string mode = "octet";
    uint8_t packet[4 + filename.size() + mode.size()];
    TFTPPacket::createRRQPacket(packet, filename, mode);
    std::cerr << "RRQ Packet created successfully of size: " << sizeof(packet) << std::endl; 
    std::cerr << "Packet content: ";
    std::cerr << std::hex << static_cast<uint8_t>(packet[0]) << std::hex << static_cast<uint8_t>(packet[0]);
    for (std::size_t i = 2; i < sizeof(packet); ++i) {
        std::cerr << packet[i];
    }
    std::cerr << std::endl;
    if (sendto(clientSocket, packet, sizeof(packet), 0, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        std::cerr << "[ERROR] : fail to send RRQ request packet" << std::endl;
        return false;
    }
    std::cerr << "[LOG] : RRQ request packet send to server " << serverAddress.sin_addr.s_addr << std::endl;
    return true;
}


/**
 * @brief Handles Write Request (WRQ) operation with the TFTP server.
 *
 * This function compresses the specified file, sends a WRQ packet to the TFTP server,
 * receives ACK packets, and sends data packets to write the compressed file on the server.
 *
 * @param clientSocket The socket descriptor for communication with the server.
 * @param serverAddress The server's sockaddr_in structure containing the IP address and port.
 * @param prevFilename The original filename associated with the WRQ request.
 * @return true if the WRQ operation is successful, false otherwise.
 */
bool TFTPClient::handleWRQRequest(int clientSocket, struct sockaddr_in serverAddress, const std::string& prevFilename) {
    if (prevFilename.empty())
    {
        std::cerr << "[ERROR] : filename is empty" << std::endl;
        return false;
    }
    // std::string path = "clientDatabase/";
    // std::string newFilePath = path + prevFilename;
    deflate(prevFilename);
    std::cerr << "File compressed" << std::endl;
    std::string strFilename(prevFilename);
    std::string filenameWithoutExtension = strFilename.substr(0, strFilename.find_last_of("."));
    std::string filename = filenameWithoutExtension + "compress.bin";
    if (!sendWRQPacket(clientSocket, serverAddress, filename))
    {
        std::cerr << "[ERROR] : fail to send WRQ packet" << std::endl;
        return false;
    }
    std::cerr << "[LOG] : sent WRQ packet" << std::endl;

    char dataBuffer[512];
    uint16_t expectedBlockNumber = 0;
    int retry = MAX_RETRY;
    bool initialPacket = true;
    // std::string directory = "clientDatabase/";
    // std::string filePath = directory + filename;
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        // Send an error packet (Disk full or allocation exceeded - Error Code 3)
        std::cerr << "[ERROR] : Cannot create file" << std::endl;
        const std::string errorMessage = "Disk full or allocation exceeded.";
        sendError(clientSocket, ERROR_DISK_FULL, errorMessage, clientAddress);
        return false;
    }
    while (retry)
    {
        char recievedBuffer[516];
        memset(recievedBuffer, 0, sizeof(recievedBuffer));
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
        if (opcode != TFTP_OPCODE_ERROR && opcode != TFTP_OPCODE_ACK) {
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
            if (recvBlockNumber != expectedBlockNumber) {
                std::cerr << "Illegal ACK Recieved" << std::endl;
                if (expectedBlockNumber == 0)
                {    
                    std::string errorMessage = "Illegal TFTP operation";
                    sendError(clientSocket, ERROR_ILLEGAL_TFTP_OPERATION, errorMessage, recvAddress);
                    file.close();
                    return false;
                }
                continue;
            }
        }
        std::cerr << "[LOG] : ACK Verified" << std::endl;
        expectedBlockNumber++;
        

        uint8_t packet[516];
        memset(packet, 0, sizeof(packet));
        memset(dataBuffer, 0, 512);
        size_t dataSize = file.gcount();
        dataSize = TFTPPacket::readDataBlock(filename, expectedBlockNumber, dataBuffer, dataSize);
        if (dataSize == 0) {
            break; // End of file
        }
        std::cerr << "Data Size: " << dataSize << std::endl;
        TFTPPacket::createDataPacket(packet, expectedBlockNumber, dataBuffer, dataSize);
        if (sendto(clientSocket, packet, dataSize + 4, 0, (struct sockaddr*)&recvAddress, sizeof(recvAddress)) < 0) {
            std::cerr << "[ERROR] : fail to send DATA packet" << std::endl;
            return false;
        }
        std::cerr << "[LOG] : DATA packet send to server " << serverAddress.sin_addr.s_addr << std::endl;
        if (dataSize < 512) {
            std::cerr << "[LOG] : File recieved Successfuly." << std::endl;
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


/**
 * @brief Sends a Write Request (WRQ) packet to the TFTP server.
 *
 * This function constructs a WRQ packet using the TFTPPacket::createWRQPacket method,
 * including the specified filename and transfer mode, and sends it to the server using
 * the provided client socket and server address.
 *
 * @param clientSocket The socket descriptor for communication with the server.
 * @param serverAddress The server's sockaddr_in structure containing the IP address and port.
 * @param filename The name of the file to be written.
 * @return true if the WRQ packet is successfully sent, false otherwise.
 */

bool TFTPClient::sendWRQPacket(int clientSocket, struct sockaddr_in serverAddress, const std::string& filename) {
    std::string mode = "octet";
    uint8_t packet[4 + filename.size() + mode.size()];
    TFTPPacket::createWRQPacket(packet, filename, mode);
    if (sendto(clientSocket, packet, 4 + filename.size() + mode.size(), 0, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        std::cerr << "[ERROR] : fail to send WRQ request packet" << std::endl;
        return false;
    }
    std::cerr << "[LOG] : WRQ request packet send to server " << serverAddress.sin_addr.s_addr << std::endl;
    return true;
}


/**
 * @brief Sends an error packet to the TFTP server.
 *
 * This function constructs an error packet using the TFTPPacket::createErrorPacket method,
 * including the specified error code and error message, and sends it to the server using
 * the provided client socket and server address.
 *
 * @param clientSocket The socket descriptor for communication with the server.
 * @param errorCode The TFTP error code.
 * @param errorMsg The error message associated with the error code.
 * @param serverAddress The server's sockaddr_in structure containing the IP address and port.
 */

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



/**
 * @brief Handles a DELETE request by sending a DELETE packet to the TFTP server and handling the server's response.
 *
 * This function takes a filename, constructs a DELETE packet using the sendDELETEPacket method,
 * sends it to the server using the provided client socket and server address, and then processes
 * the server's response, checking for acknowledgments (ACK) or error packets.
 *
 * @param clientSocket The socket descriptor for communication with the server.
 * @param serverAddress The server's sockaddr_in structure containing the IP address and port.
 * @param filename The name of the file to be deleted.
 * @return true if the DELETE request is successful, false otherwise.
 */
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
        if (recvAddress.sin_addr.s_addr != serverAddress.sin_addr.s_addr)
        {
            std::cerr << "Corrupt packet from different IP received" << std::endl;
            const std::string errorMessage = "Unknown transfer ID";
            sendError(clientSocket, ERROR_UNKNOWN_TID, errorMessage, recvAddress);
            continue;
        }
        uint16_t opcode = (uint16_t)(((recievedBuffer[1] & 0xFF) << 8) | (recievedBuffer[0] & 0XFF));
        opcode = ntohs(opcode);
        std::cerr << "Recieved Opcode:" << opcode << std::endl;

        if (opcode != TFTP_OPCODE_ERROR && opcode != TFTP_OPCODE_ACK) {
            std::cerr << "Illegal Opcode Recieved" << std::endl;
            std::string errorMessage = "Illegal TFTP operation";
            sendError(clientSocket, ERROR_ILLEGAL_TFTP_OPERATION, errorMessage, recvAddress);
            return false;
        }
        std::cerr << "Read Bytes: " << readBytes << std::endl;
        uint16_t recvBlockNumber = (uint16_t)(((recievedBuffer[3] & 0xFF) << 8) | (recievedBuffer[2] & 0XFF));
        recvBlockNumber = ntohs(recvBlockNumber);
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
        
    }
    std::cerr << "Max retry exceeded for timeout." << std::endl;
    return false;
}



/**
 * @brief Handles a DELETE request by sending a DELETE packet to the TFTP server and handling the server's response.
 *
 * This function takes a filename, constructs a DELETE packet using the sendDELETEPacket method,
 * sends it to the server using the provided client socket and server address, and then processes
 * the server's response, checking for acknowledgments (ACK) or error packets.
 *
 * @param clientSocket The socket descriptor for communication with the server.
 * @param serverAddress The server's sockaddr_in structure containing the IP address and port.
 * @param filename The name of the file to be deleted.
 * @return true if the DELETE request is successful, false otherwise.
 */
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
        if (recvAddress.sin_addr.s_addr != serverAddress.sin_addr.s_addr)
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
            return false;
        }
        uint16_t recvBlockNumber = (uint16_t)(((recievedBuffer[3] & 0xFF) << 8) | (recievedBuffer[2] & 0XFF));
        recvBlockNumber = ntohs(recvBlockNumber);
        std::cerr << "Recieved Block Number:" << recvBlockNumber << std::endl;
        if (recvBlockNumber != expectedBlockNumber) {
            std::cerr << "Incorrect Block Number Recieved" << std::endl;
            // std::string errorMessage = "Incorrect Block Number Recieved. Resend";
            // sendError(clientSocket, ERROR_NOT_DEFINED, errorMessage, recvAddress);
            // return false;
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
            sendError(clientSocket, ERROR_DISK_FULL, errorMessage, serverAddress);
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


/**
 * @brief Sends an ACK (Acknowledgment) packet to the TFTP server for the specified block number.
 *
 * This function creates and sends an ACK packet with the given block number to the server. It retries the sending process
 * up to 5 times in case of failure. If the ACK packet is sent successfully, the function logs the event and returns.
 *
 * @param clientSocket The socket descriptor for communication with the server.
 * @param blockNumber The block number for which the ACK packet is being sent.
 * @param serverAddress The server's sockaddr_in structure containing the IP address and port.
 */
void TFTPClient::sendACK(int clientSocket, uint16_t blockNumber, struct sockaddr_in serverAddress) {
    uint8_t packet[4];
    TFTPPacket::createACKPacket(packet, blockNumber);
    int retry = 5;
    while (retry)
    {
        if (sendto(clientSocket, packet, 4, 0, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
            std::cerr << "[ERROR] : fail to send ACK packet. Retryinggg" << std::endl;
            retry--;
            continue;
        }
        std::cerr << "[LOG] : ACK packet send to client " << serverAddress.sin_addr.s_addr << " with Block Number: " << blockNumber << std::endl;
        return;
        /* code */
    }
    
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
            exit(1);
    }

    TFTPClient client(serverIP);
    client.startClient(opcode, filename);

    return 1;
}
