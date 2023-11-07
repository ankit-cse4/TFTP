#include "packets_tftp.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string>
#include <stdlib.h>
#include <sstream>

// Number of client this server can handle at one time
#define MAX_CLIENTS 15	
#define TFTP_DEFAULT_PORT 69
#define MAX_PATH_LENGTH 256

#define REQUEST_UNDEFINED 0
#define REQUEST_READ 1
#define REQUEST_WRITE 2

#define NOT_CONNECTED 0
#define CONNECTED 1

#define ACK_WAITING 0
#define ACK_OK 1

/* Error Codes */
#define ERROR_NOT_DEFINED 0
#define ERROR_FILE_NOT_FOUND 1
#define ERROR_ACCESS_VIOLATION 2
#define ERROR_DISK_FULL 3
#define ERROR_ILLEGAL_TFTP_OPERATION 4
#define ERROR_UNKNOWN_TID 5
#define ERROR_FILE_ALREADY_EXISTS 6
#define ERROR_NO_SUCH_USER 7

#define DIRECTORY_LIST_SIZE 2048

using namespace std;

struct Client{
	int connection;
	int request_type;
	int block;			//
	int temp;
	int acknowledged;
	int dirPost;		// Marker for Directory List Sending
	
	char dirBuf[DIRECTORY_LIST_SIZE];
	
	char* ip;
	
	fd_set set;
	
	int client_socket;
	struct sockaddr_in address;
	struct sockaddr_storage addr;
	
	ifstream* read_file;
	ofstream* write_file;
	
	int disconnect_after_send;
	
	TFTP_PACKET receive_packet;
	TFTP_PACKET send_packet;
	
	Client(){
		request_type = REQUEST_UNDEFINED;
		connection = NOT_CONNECTED;
		acknowledged = ACK_WAITING;
		block = 0;
		disconnect_after_send = 0;
		client_socket = -1;
		ip = (char*)"";
		dirPost = 0;
	}
	
	~Client(){
		if(read_file) delete read_file;
		if(write_file) delete write_file;
	}
};


class TFTP_SERVER{
private:
	int server_port;
	char rootdir[MAX_PATH_LENGTH];
	
	int server_socketfd;
	struct sockaddr_in server_addr;
	int listener;
	
	int getFileOffset(char* f){
		char at[] = "@";
		int off = strcspn(f,at);
		if(off == strlen(f)) return 0;
		char offset[16];
		strncpy(offset,f+off+1,strlen(f) - off);
		off = atoi(offset);
		return off;
	}
	
	int ls(char*, char*);
	
public:
	Client clients[MAX_CLIENTS];
	
	TFTP_SERVER(int, char*);
	
	int run(int);
	
	/* Packet Received */
	int receivePacket(Client*);
	int processClient(Client*);
	
	/* RRQ */
	int getReadFile(Client*);
	int createReadPacket(Client*);
	
	/* WRQ */
	int createWriteFile(Client*);
	int writeData(Client*);
	
	int createDirPacket(Client*, char*);
	
	int sendPacket(TFTP_PACKET*, Client*);
	
	int sendError(Client*, int, char*);
	
	int disconnect(Client*);
	
	int closeServer();
	
	~TFTP_SERVER();
	
};

class TFTPServerException{
private:
	char* err;
public:
	TFTPServerException(char* msg){
		stringstream s;
		s << "TFTP Server Exception: " << msg;
		s >> err;
	}
	void print(ostream &out)
	{ out << err; }
	
	friend ostream& operator<< (ostream &out, TFTPServerException &e){
		e.print(out);
		return out;
	}
};
