#include "TFTPPacket.h"
#include <fstream>
#include <cstring>
#include <iostream>


/**
 * @brief Create a TFTP request packet.
 *
 * This function populates the provided packet with the specified opcode, filename, and mode
 * to create a TFTP request packet.
 *
 * @param packet Pointer to the buffer where the TFTP request packet will be stored.
 * @param opcode The TFTP opcode for the request (e.g., RRQ, WRQ).
 * @param filename The requested filename for the TFTP operation.
 * @param mode The TFTP transfer mode (e.g., "octet").
 */
void TFTPPacket::createRequestPacket(uint8_t* packet, uint16_t opcode, const std::string& filename, const std::string& mode) {
    size_t index = 2;
    
    // Copy the filename into the packet
    for (char c : filename) {
        packet[index++] = static_cast<uint8_t>(c);
        if (c == '\0') {
            break;
        }
    }
    packet[index++] = 0x00;

    // Copy the mode into the packet
    for (char c : mode) {
        packet[index++] = static_cast<uint8_t>(c);
        if (c == '\0') {
            break;
        }
    }
    packet[index++] = 0x00;
}

/**
 * @brief Create a Read Request (RRQ) packet.
 *
 * This function populates the provided packet with the RRQ opcode, filename, and mode
 * to create a TFTP Read Request packet.
 *
 * @param packet Pointer to the buffer where the RRQ packet will be stored.
 * @param filename The requested filename for the TFTP read operation.
 * @param mode The TFTP transfer mode (e.g., "octet").
 */
void TFTPPacket::createRRQPacket(uint8_t* packet, const std::string& filename, const std::string& mode) {
    packet[0] = 0x00;
    packet[1] = 0x01;
    createRequestPacket(packet, TFTP_OPCODE_RRQ, filename, mode);
}


/**
 * @brief Create a Write Request (WRQ) packet.
 *
 * This function populates the provided packet with the WRQ opcode, filename, and mode
 * to create a TFTP Write Request packet.
 *
 * @param packet Pointer to the buffer where the WRQ packet will be stored.
 * @param filename The requested filename for the TFTP write operation.
 * @param mode The TFTP transfer mode (e.g., "octet").
 */
void TFTPPacket::createWRQPacket(uint8_t* packet, const std::string& filename, const std::string& mode) {
    packet[0] = 0x00;
    packet[1] = 0x02;
    createRequestPacket(packet, TFTP_OPCODE_WRQ, filename, mode);
}


/**
 * @brief Create a Data packet.
 *
 * This function populates the provided packet with the DATA opcode, block number, and data
 * to create a TFTP Data packet.
 *
 * @param packet Pointer to the buffer where the Data packet will be stored.
 * @param blockNumber The block number associated with the data packet.
 * @param data Pointer to the data to be included in the packet.
 * @param dataSize The size of the data in bytes.
 */
void TFTPPacket::createDataPacket(uint8_t* packet, uint16_t blockNumber, const char* data, size_t dataSize) {
    // Create a DATA packet
    packet[0] = 0x00;
    packet[1] = 0x03;
    packet[2] = (blockNumber >> 8) & 0xFF;
    packet[3] = blockNumber & 0xFF;
    std::memcpy(packet + 4, data, dataSize);
}


/**
 * @brief Create an acknowledgment (ACK) packet.
 *
 * This function populates the provided packet with the ACK opcode and block number
 * to create a TFTP acknowledgment packet.
 *
 * @param packet Pointer to the buffer where the ACK packet will be stored.
 * @param blockNumber The block number being acknowledged.
 */
void TFTPPacket::createACKPacket(uint8_t* packet, uint16_t blockNumber) {
    // Create an ACK packet
    packet[0] = 0x00;
    packet[1] = 0x04;
    packet[2] = (blockNumber >> 8) & 0xFF;
    packet[3] = blockNumber & 0xFF;
}


/**
 * @brief Create an error packet.
 *
 * This function populates the provided packet with the ERROR opcode, error code, and error message
 * to create a TFTP error packet.
 *
 * @param packet Pointer to the buffer where the ERROR packet will be stored.
 * @param errorCode The TFTP error code.
 * @param errorMsg The error message associated with the error code.
 */
void TFTPPacket::createErrorPacket(uint8_t* packet, uint16_t errorCode, const std::string& errorMsg) {
    // Create an ERROR packet
    packet[0] = 0x00;
    packet[1] = 0x05;
    packet[2] = (errorCode >> 8) & 0xFF;
    packet[3] = errorCode & 0xFF;

    size_t index = 4;
    for (char c : errorMsg) {
        packet[index++] = static_cast<uint8_t>(c);
        if (c == '\0') {
            break;
        }
    }
}


/**
 * @brief Create a delete request packet.
 *
 * This function populates the provided packet with the DELETE opcode, filename, and "octet" mode
 * to create a TFTP delete request packet.
 *
 * @param packet Pointer to the buffer where the DELETE packet will be stored.
 * @param filename The filename to be deleted.
 */
void TFTPPacket::createDeletePacket(uint8_t* packet, const std::string& filename) {
    packet[0] = 0x00;
    packet[1] = 0x06;
    std::string mode = "octet";
    createRequestPacket(packet, TFTP_OPCODE_DELETE, filename, mode);
}



/**
 * @brief Create a list request packet.
 *
 * This function populates the provided packet with the LS opcode to create a TFTP list request packet.
 *
 * @param packet Pointer to the buffer where the LS packet will be stored.
 */
void TFTPPacket::createLSPacket(uint8_t* packet) {
    packet[0] = 0x00;
    packet[1] = 0x07;
    packet[2] = 0x00;
}


/**
 * @brief Read a data block from the file.
 *
 * This function reads 512 bytes of data for the specified block number from the file.
 *
 * @param filename The name of the file to read from.
 * @param blockNumber The block number to read.
 * @param data Pointer to the buffer where the read data will be stored.
 * @param dataSize Reference to the variable storing the actual size of the read data.
 * @return The size of the data read in bytes.
 */
size_t TFTPPacket::readDataBlock(const std::string& filename, uint16_t blockNumber, char* data, size_t& dataSize) {
    // Read 512 bytes of data for the specified block number from the file
    std::ifstream file(filename, std::ios::binary);
    dataSize = 0;

    if (file.is_open()) {
        file.seekg((blockNumber - 1) * 512);
        file.read(data, 512);
        dataSize = file.gcount();
    }

    return dataSize;
}
