// #include <iostream>
// #include <string.h>
// #include <errno.h>
// #include <fstream>
// #include <sstream>
// #include <ostream>

// #define		TFTP_OPCODE_RRQ		1
// #define		TFTP_OPCODE_WRQ		2
// #define		TFTP_OPCODE_DATA	3
// #define		TFTP_OPCODE_ACK		4
// #define		TFTP_OPCODE_ERROR	5

// #define		TFTP_DEFAULT_TRANSFER_MODE		"octet"

// #define NOT_CONNECTED 0
// #define CONNECTED 1

// #define ACK_WAITING 0
// #define ACK_OK 1

// /* Error Codes */
// #define ERROR_NOT_DEFINED 0
// #define ERROR_FILE_NOT_FOUND 1
// #define ERROR_ACCESS_VIOLATION 2
// #define ERROR_DISK_FULL 3
// #define ERROR_ILLEGAL_TFTP_OPERATION 4
// #define ERROR_UNKNOWN_TID 5
// #define ERROR_FILE_ALREADY_EXISTS 6
// #define ERROR_NO_SUCH_USER 7

// /* Packet Outlines
 
// [RRQ/WRQ Packet]
// 2 bytes     string    1 byte   string    1 byte
// ------------------------------------------------
// | Opcode |  Filename  |  0  |   Mode    |   0  |
// ------------------------------------------------

// [DATA Packet]
// 2 bytes    2 bytes     n bytes
// ---------------------------------
// | Opcode |  Block # |   Data    |
// ---------------------------------

// [ACK Packet]
// 2 bytes    2 bytes
// ---------------------
// | Opcode |  Block # |
// ---------------------

// [ERROR Packet]
// 2 bytes    2 bytes     string    1 byte
// -----------------------------------------
// | Opcode | ErrorCode |  ErrMsg  |   0   |
// -----------------------------------------
// */

// class TFTPPacket{
//   private:
//     int packet_size;
//     unsigned char data[516];

//   public:
//     TFTPPacket();
//     void printData();
//     int createRRQPacket(char* filename);
//     int createWRQPacket(char* filename);
//     int createACKPacket(int packet_id);
//     int createDATAPacket(int block, char* data, int dataSize);
//     int createERRORPacket(int error_code, char* errorMsg);

// };



#ifndef TFTP_PACKET_H
#define TFTP_PACKET_H

#include <string>
#include <cstdint>

class TFTPPacket {
public:
    static void createRRQPacket(uint8_t* packet, const std::string& filename, const std::string& mode);
    static void createWRQPacket(uint8_t* packet, const std::string& filename, const std::string& mode);
    static void createDataPacket(uint8_t* packet, uint16_t blockNumber, const char* data, size_t dataSize);
    static void createACKPacket(uint8_t* packet, uint16_t blockNumber);
    static void createErrorPacket(uint8_t* packet, uint16_t errorCode, const std::string& errorMsg);

    static size_t readDataBlock(const std::string& filename, uint16_t blockNumber, char* data, size_t& dataSize);

private:
    static void createRequestPacket(uint8_t* packet, uint16_t opcode, const std::string& filename, const std::string& mode);
};

#endif
