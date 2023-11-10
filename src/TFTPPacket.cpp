#include "TFTPPacket.h"
#include <fstream>
#include <cstring>

void TFTPPacket::createRequestPacket(uint8_t* packet, uint16_t opcode, const std::string& filename, const std::string& mode) {
    // Create a RRQ or WRQ packet
    packet[0] = (opcode >> 8) & 0xFF;
    packet[1] = opcode & 0xFF;

    size_t index = 2;
    for (char c : filename) {
        packet[index++] = static_cast<uint8_t>(c);
        if (c == '\0') {
            break;
        }
    }

    for (char c : mode) {
        packet[index++] = static_cast<uint8_t>(c);
        if (c == '\0') {
            break;
        }
    }
}

void TFTPPacket::createRRQPacket(uint8_t* packet, const std::string& filename, const std::string& mode) {
    createRequestPacket(packet, 1, filename, mode);
}

void TFTPPacket::createWRQPacket(uint8_t* packet, const std::string& filename, const std::string& mode) {
    createRequestPacket(packet, 2, filename, mode);
}

void TFTPPacket::createDataPacket(uint8_t* packet, uint16_t blockNumber, const char* data, size_t dataSize) {
    // Create a DATA packet
    packet[0] = 0x00;
    packet[1] = 0x03;
    packet[2] = (blockNumber >> 8) & 0xFF;
    packet[3] = blockNumber & 0xFF;
    std::memcpy(packet + 4, data, dataSize);
}

void TFTPPacket::createACKPacket(uint8_t* packet, uint16_t blockNumber) {
    // Create an ACK packet
    packet[0] = 0x00;
    packet[1] = 0x04;
    packet[2] = (blockNumber >> 8) & 0xFF;
    packet[3] = blockNumber & 0xFF;
}

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
