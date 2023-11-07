#include "packets_tftp.h"

using namespace std;

TFTP_PACKET::TFTP_PACKET(){
  clearPacket();
}

int TFTP_PACKET::getSize(){
  return packet_size;
}

int TFTP_PACKET::setSize(int _varSize){
  if(_varSize <= TFTP_PACKET_MAX_SIZE){
    packet_size = _varSize;
  }
  return packet_size;
}

void TFTP_PACKET::clearPacket(){
  memset(data, packet_size = 0, TFTP_PACKET_MAX_SIZE); 
}

void TFTP_PACKET::printData(){
	cout << "\n~~~~~~~~~~~~~~~Data~~~~~~~~~~~~~~~\n";
	cout << "\tPacket Size: " << packet_size << endl;
	cout << "Opcode: " << (int)data[0] << (int)data[1] << endl;
  int opCode = getOpcode();

  if(opCode==5){
    cout << "Error Code: " << (int)data[2] << (int)data[3] << endl;
    int n = strlen((char*)getData(4));
    for(int i = 4; i < n; ++i)
      cout << (char)data[i];
  }else if(opCode==4){
    cout << "Block #: " << (int)data[2] << (int)data[3] << endl;
  }else if(opCode==3){
    cout << "Block #: " << (int)data[2] << (int)data[3] << endl;
    for(int i = 4; i < packet_size; ++i)
      cout << data[i];
  }
	cout << "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
}


int TFTP_PACKET::addByte(BYTE _byteToAdd){
	if(packet_size >= TFTP_PACKET_MAX_SIZE){
		cerr << "Max Packet Size Reached (" << packet_size << ")\n";
		return -1;
	}
	return (data[packet_size++] = (unsigned char)_byteToAdd);
}


int TFTP_PACKET::addWord(WORD _wordToAdd){
	if(packet_size + 2 >= TFTP_PACKET_MAX_SIZE){
		cerr << "Max Packet Size Reached (" << packet_size << ")\n";
		return -1;
	}
	data[packet_size++] |= (_wordToAdd>>8);
	data[packet_size++] |= _wordToAdd;
	return _wordToAdd;
}


int TFTP_PACKET::addString(char* _stringToAdd){
	int i = 0;
  for(i=0; _stringToAdd[i]; ++i)
		if(addByte(_stringToAdd[i]) < 0) return i;
	packet_size += i;
	return i;
}
int TFTP_PACKET::addString(const char* _s){
	int i = 0;
	for(; _s[i]; ++i)
		if(addByte(_s[i]) < 0) return i;
	packet_size += i;
	return i;
}


int TFTP_PACKET::addData(char* _buffer, int _length){
	if(packet_size + _length >= TFTP_PACKET_MAX_SIZE){
		cerr << "Packet Max Size Reached (" << packet_size + _length << ")\n";
		return 0;
	}
	memcpy(&(data[packet_size]),_buffer,_length);
	packet_size += _length;
	return _length;
}


BYTE TFTP_PACKET::getByte(int _offset){ 
  return (BYTE)data[_offset]; 
}


WORD TFTP_PACKET::getWord(int _offset){ 
  return ((getByte(_offset)<<8)|getByte(_offset+1)); 
}


BYTE TFTP_PACKET::getOpcode(){ 
  return (BYTE)data[1]; 
}


int TFTP_PACKET::getString(int _offset, char* _buf, int _len){
	if(_len < packet_size - _offset || _offset >= packet_size){
    return 0;
  }
	strcpy(_buf,(char*)&(data[_offset]));
	return packet_size - _offset;
}


WORD TFTP_PACKET::getBlockNumber() {
    if (this->isData() || this->isACK()) {
        // If the packet is a data packet or an ACK packet,
        // retrieve and return the block number from the packet.
        return this->getWord(2);
    } else {
        // If the packet is not a data or ACK packet, return 0 as the block number.
        return 0;
    }
}


unsigned char* TFTP_PACKET::getData(int _offset = 0){ 
  return &(data[_offset]); 
}


int TFTP_PACKET::getDataSize(){
	int n = 0;
  if(!isData()) {
    return 0;
  }
	BYTE* d = getData(TFTP_DATA_PKT_DATA_OFFSET);
	for(n = 0; d[n] && n < TFTP_PACKET_DATA_SIZE; ++n);
	return n;
}


int TFTP_PACKET::copyData(int _offset, char* _dest, int _len){
	if(_len < packet_size - _offset || _offset >= packet_size) return 0;
	memcpy(_dest, &(data[_offset]), packet_size - _offset);
	return packet_size - _offset;
}



/*
 *	Create RRQ Packet
 *
 *	2 bytes     string    1 byte   string    1 byte
 *	------------------------------------------------
 *	| Opcode |  Filename  |  0  |   Mode    |   0  |
 *	------------------------------------------------
 *
 */
int TFTP_PACKET::createRRQ(char* _filename){
	clearPacket();
	if(addWord(TFTP_OPCODE_RRQ) < 0) {
    return -1;
  }
	int n = addString(_filename);
	if(addByte(0) < 0) {
    return -1;
  }
	if(addString(TFTP_DEFAULT_TRANSFER_MODE) < strlen(TFTP_DEFAULT_TRANSFER_MODE)) {
    return -1;
  }
	if(addByte(0) < 0) {
    return -1;
  }
	return n;
}



/*
 *	Create WRQ Packet
 *
 *	2 bytes     string    1 byte   string    1 byte
 *	------------------------------------------------
 *	| Opcode |  Filename  |  0  |   Mode    |   0  |
 *	------------------------------------------------
 *
 *	
 */
int TFTP_PACKET::createWRQ(char* _filename){
	clearPacket();
	if(addWord(TFTP_OPCODE_WRQ) < 0) {
    return -1;
  }
	int n = 0;
	if((n = addString(_filename)) < strlen(_filename)){
		cerr << "Not all of Filename was written (" << n << " / ";
		cerr << strlen(_filename) << ")\n";
		return -1;
	}
	if(addByte(0) < 0) {
    return -1;
  }
	if(addString(TFTP_DEFAULT_TRANSFER_MODE) < strlen(TFTP_DEFAULT_TRANSFER_MODE)) {
    return -1;
  }
	if(addByte(0) < 0) {
    return -1;
  }
	return n;
}


/*
 *	Create ACK Packet
 *
 *	2 bytes    2 bytes
 *	---------------------
 *	| Opcode |  Block # |
 *	---------------------
 *
 *	
 */
int TFTP_PACKET::createACK(int _packet_num){
	clearPacket();
	if(addWord(TFTP_OPCODE_ACK) < 0) {
    return -1;
  }
	if(addWord(_packet_num) < 0) {
    return -1;
  }
	return _packet_num;
}


/*
 *	Create DATA Packet
 *
 *	2 bytes    2 bytes     n bytes
 *	---------------------------------
 *	| Opcode |  Block # |   Data    |
 *	---------------------------------
 *
 *	
 */
int TFTP_PACKET::createData(int _block, char* _data, int _data_size){
	clearPacket();
	if(addWord(TFTP_OPCODE_DATA) < 0) {
    return -1;
  }
	if(addWord(_block) < 0) {
    return -1;
  }
	return addData(_data,_data_size);
}


/*
 *	Create ERROR Packet
 *
 *	2 bytes    2 bytes     string    1 byte
 *	-----------------------------------------
 *	| Opcode | ErrorCode |  ErrMsg  |   0   |
 *	-----------------------------------------
 *
 *	
 */
int TFTP_PACKET::createError(int _error_code, char* _msg){
	clearPacket();
	if(addWord(TFTP_OPCODE_ERROR) < 0) {
    return -1;
  }
	if(addWord(_error_code) < 0) {
    return -1;
  }
	if(addString(_msg) < strlen(_msg)) {
    return -1;
  }
	if(addByte(0) < 0) {
    return -1;
  }
	return _error_code;
}



bool TFTP_PACKET::isRRQ(){ 
  return getOpcode() == TFTP_OPCODE_RRQ; 
}


bool TFTP_PACKET::isWRQ(){ 
  return getOpcode() == TFTP_OPCODE_WRQ; 
}


bool TFTP_PACKET::isACK()
{ 
  return getOpcode() == TFTP_OPCODE_ACK; 
}



bool TFTP_PACKET::isData()
{ 
  return getOpcode() == TFTP_OPCODE_DATA; 
}



bool TFTP_PACKET::isError()
{ 
  return getOpcode() == TFTP_OPCODE_ERROR; 
}


TFTP_PACKET::~TFTP_PACKET(){}