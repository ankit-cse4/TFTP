#include <gtest/gtest.h>
#include "TFTPPacket.h"

TEST(tftpTests, Test1){
    
    uint8_t buffer[10];

    TFTPPacket::createLSPacket(buffer);

    uint8_t expected[10];
    expected[0] = 0x00;
    expected[1] = 0x07;
    expected[2] = 0x00;

    ASSERT_EQ(memcmp(expected, buffer, 3), 0);
}

TEST(tftpTests, Test2){
    
    uint8_t funcResult[10];
    uint16_t blockNum = 9;
    TFTPPacket::createACKPacket(funcResult, blockNum);

    uint8_t expected[10];
    expected[0] = 0x00;
    expected[1] = 0x04;
    expected[2] = 0x00;
    expected[3] = 0x09;

    ASSERT_EQ(memcmp(funcResult, expected, 4), 0);
}

TEST(tftpTests, Test3){
    std::string filename = "WriteTest";
    std::string mode = "octet";
    int packetSize = 2+filename.length() +1 +mode.length()+1;
    uint8_t funcResult[packetSize];

    TFTPPacket::createWRQPacket(funcResult, filename, mode);

    uint8_t expected[packetSize];
    expected[0] = 0x00;
    expected[1] = 0x02;
    memcpy(expected+2, filename.c_str(), filename.length());
    expected[2+filename.length()]=0x00;
    memcpy(expected +2 + filename.length()+1, mode.c_str(), mode.length());
    expected[2 + filename.length()+ 1+ mode.length()]=0x00;


    ASSERT_EQ(memcmp(funcResult, expected, packetSize), 0);
}



TEST(tftpTests, Test4){
    std::string filename = "WriteTest";
    std::string mode = "octet";
    int packetSize = 2+filename.length() +1 +mode.length()+1;
    uint8_t funcResult[packetSize];

    TFTPPacket::createRRQPacket(funcResult, filename, mode);

    uint8_t expected[packetSize];
    expected[0] = 0x00;
    expected[1] = 0x01;
    memcpy(expected+2, filename.c_str(), filename.length());
    expected[2+filename.length()]=0x00;
    memcpy(expected +2 + filename.length()+1, mode.c_str(), mode.length());
    expected[2 + filename.length()+ 1+ mode.length()]=0x00;


    ASSERT_EQ(memcmp(funcResult, expected, packetSize), 0);
}

TEST(tftpTests, Test5){ 
    
    uint16_t blockNum = 9;
    std::string data = "DataTest.txt";
    size_t dataSize = data.length();
    std::cout<<dataSize;
    int packetSize = 2 + 2 + dataSize;
    uint8_t funcResult[packetSize];

    TFTPPacket::createDataPacket(funcResult, blockNum, data.c_str(), dataSize);

    uint8_t expected[packetSize];
    expected[0] = 0x00;
    expected[1] = 0x03;
    expected[0] = 0x00;
    expected[1] = 0x09;
    memcpy(expected+4, data.c_str(), dataSize);


    ASSERT_NE(memcmp(funcResult, expected, packetSize), 0);
}

TEST(tftpTests, Test6){ 
    
    uint16_t blockNum = 1;
    std::string data = "File not found.";
    size_t dataSize = data.length();
    std::cout<<dataSize;
    int packetSize = 2 + 2 + dataSize + 1;
    uint8_t funcResult[packetSize];

    TFTPPacket::createErrorPacket(funcResult, blockNum, data);

    uint8_t expected[packetSize];
    expected[0] = 0x00;
    expected[1] = 0x05;
    expected[2] = 0x00;
    expected[3] = 0x01;
    memcpy(expected+4, data.c_str(), data.length());
    expected[2+2+dataSize]=0x00;


    ASSERT_EQ(memcmp(funcResult, expected, packetSize), 0);
}

TEST(tftpTests, Test7){ 
    
    uint16_t blockNum = 2;
    std::string data = "Access violation";
    size_t dataSize = data.length();
    std::cout<<dataSize;
    int packetSize = 2 + 2 + dataSize + 1;
    uint8_t funcResult[packetSize];

    TFTPPacket::createErrorPacket(funcResult, blockNum, data);

    uint8_t expected[packetSize];
    expected[0] = 0x00;
    expected[1] = 0x05;
    expected[2] = 0x00;
    expected[3] = 0x02;
    memcpy(expected+4, data.c_str(), data.length());
    expected[2+2+dataSize]=0x00;


    ASSERT_EQ(memcmp(funcResult, expected, packetSize), 0);
}

TEST(tftpTests, Test8){ 
    
    uint16_t blockNum = 3;
    std::string data = "Disk full or allocation exceeded";
    size_t dataSize = data.length();
    std::cout<<dataSize;
    int packetSize = 2 + 2 + dataSize + 1;
    uint8_t funcResult[packetSize];

    TFTPPacket::createErrorPacket(funcResult, blockNum, data);

    uint8_t expected[packetSize];
    expected[0] = 0x00;
    expected[1] = 0x05;
    expected[2] = 0x00;
    expected[3] = 0x03;
    memcpy(expected+4, data.c_str(), data.length());
    expected[2+2+dataSize]=0x00;


    ASSERT_EQ(memcmp(funcResult, expected, packetSize), 0);
}

TEST(tftpTests, Test9){ 
    
    uint16_t blockNum = 4;
    std::string data = "Illegal TFTP operation";
    size_t dataSize = data.length();
    std::cout<<dataSize;
    int packetSize = 2 + 2 + dataSize + 1;
    uint8_t funcResult[packetSize];

    TFTPPacket::createErrorPacket(funcResult, blockNum, data);

    uint8_t expected[packetSize];
    expected[0] = 0x00;
    expected[1] = 0x05;
    expected[2] = 0x00;
    expected[3] = 0x04;
    memcpy(expected+4, data.c_str(), data.length());
    expected[2+2+dataSize]=0x00;


    ASSERT_NE(memcmp(funcResult, expected, packetSize), 0);
}

TEST(tftpTests, Test10){ 
    
    uint16_t blockNum = 5;
    std::string data = "Unknown transfer ID";
    size_t dataSize = data.length();
    std::cout<<dataSize;
    int packetSize = 2 + 2 + dataSize + 1;
    uint8_t funcResult[packetSize];

    TFTPPacket::createErrorPacket(funcResult, blockNum, data);

    uint8_t expected[packetSize];
    expected[0] = 0x00;
    expected[1] = 0x05;
    expected[2] = 0x00;
    expected[3] = 0x05;
    memcpy(expected+4, data.c_str(), data.length());
    expected[2+2+dataSize]=0x00;


    ASSERT_EQ(memcmp(funcResult, expected, packetSize), 0);
}

TEST(tftpTests, Test11){ 
    
    uint16_t blockNum = 6;
    std::string data = "File already exists";
    size_t dataSize = data.length();
    std::cout<<dataSize;
    int packetSize = 2 + 2 + dataSize + 1;
    uint8_t funcResult[packetSize];

    TFTPPacket::createErrorPacket(funcResult, blockNum, data);

    uint8_t expected[packetSize];
    expected[0] = 0x00;
    expected[1] = 0x05;
    expected[2] = 0x00;
    expected[3] = 0x06;
    memcpy(expected+4, data.c_str(), data.length());
    expected[2+2+dataSize]=0x00;


    ASSERT_EQ(memcmp(funcResult, expected, packetSize), 0);
}

TEST(tftpTests, Test12){ 
    
    uint16_t blockNum = 0;
    std::string data = "Not defined";
    size_t dataSize = data.length();
    std::cout<<dataSize;
    int packetSize = 2 + 2 + dataSize + 1;
    uint8_t funcResult[packetSize];

    TFTPPacket::createErrorPacket(funcResult, blockNum, data);

    uint8_t expected[packetSize];
    expected[0] = 0x00;
    expected[1] = 0x00;
    expected[2] = 0x00;
    expected[3] = 0x06;
    memcpy(expected+4, data.c_str(), data.length());
    expected[2+2+dataSize]=0x00;


    ASSERT_NE(memcmp(funcResult, expected, packetSize), 0);
}

TEST(tftpTests, Test13){ 
    
    std::string data = "filename.txt";
    std::string mode = "octet";
    size_t dataSize = data.length();
    int packetSize = 2 + 2 + dataSize + mode.length();
    uint8_t funcResult[packetSize];

    TFTPPacket::createDeletePacket(funcResult, data);

 
    uint8_t expected[packetSize];
    expected[0] = 0x00;
    expected[1] = 0x06;
    memcpy(expected+2, data.c_str(), data.length());
    expected[2+data.length()]=0x00;
    memcpy(expected +2 + data.length()+1, mode.c_str(), mode.length());
    expected[2 + data.length()+ 1+ mode.length()]=0x00;


    ASSERT_EQ(memcmp(funcResult, expected, packetSize), 0);
}






