#include <stdint.h>
#include <iostream>
#include <string.h>
#include <errno.h>
#include <fstream>
#include <sstream>
#include <ostream>

#define		TFTP_OPCODE_RRQ		1
#define		TFTP_OPCODE_WRQ		2
#define		TFTP_OPCODE_DATA	3
#define		TFTP_OPCODE_ACK		4
#define		TFTP_OPCODE_ERROR	5

#define		TFTP_DEFAULT_TRANSFER_MODE		"octet"

#define		TFTP_PACKET_DATA_SIZE	512
#define		TFTP_PACKET_MAX_SIZE	1024

#define		TFTP_DATA_PKT_DATA_OFFSET	4

typedef uint8_t BYTE;
typedef uint16_t WORD;

/* Packet Outlines
 
[RRQ/WRQ Packet]
2 bytes     string    1 byte   string    1 byte
------------------------------------------------
| Opcode |  Filename  |  0  |   Mode    |   0  |
------------------------------------------------

[DATA Packet]
2 bytes    2 bytes     n bytes
---------------------------------
| Opcode |  Block # |   Data    |
---------------------------------

[ACK Packet]
2 bytes    2 bytes
---------------------
| Opcode |  Block # |
---------------------

[ERROR Packet]
2 bytes    2 bytes     string    1 byte
-----------------------------------------
| Opcode | ErrorCode |  ErrMsg  |   0   |
-----------------------------------------
*/

class TFTP_PACKET{
private:
	int packet_size;					
	unsigned char data[TFTP_PACKET_MAX_SIZE];	

public:
	TFTP_PACKET();
	
	int getSize();
	int setSize(int size);
	
	void clearPacket();
	void printData();
	
	int addByte(BYTE b);
	int addString(char* s);
	int addString(const char* s);
	int addData(char* buf, int len);
	int addWord(WORD w);
	
	BYTE getByte(int offset);
	WORD getWord(int offset);
	BYTE getOpcode();
	int getString(int offset, char* buf, int len);
	WORD getBlockNumber();
	unsigned char* getData(int offset);
	int getDataSize();
	int copyData(int offset, char* dest, int len);
	
	int createRRQ(char* filename);
	int createWRQ(char* filename);
	int createACK(int packet_num);
	int createData(int block, char* data, int data_size);
	int createError(int error_code, char* msg);
	
	int sendPacket(TFTP_PACKET*);
	
	bool isRRQ();
	bool isWRQ();
	bool isACK();
	bool isData();
	bool isError();

	friend std::ostream& operator<< (std::ostream &out, TFTP_PACKET &p){
		out << (int)p.data[0] << (int)p.data[1];
		if(p.getOpcode()==5){
      out << (int)p.data[2] << (int)p.data[3];
      int n = strlen((char*)p.getData(4));
      for(int i = 4; i < n; ++i)
        out << (char)p.data[i];
    } else if(p.getOpcode()==4){
      out << (int)p.data[2] << (int)p.data[3];
    }else if(p.getOpcode()==3){
      out << (int)p.data[2] << (int)p.data[3];
      for(int i = 4; i < p.packet_size; ++i)
        out << p.data[i];
    }
		return out;
	}
	
	~TFTP_PACKET();
};
