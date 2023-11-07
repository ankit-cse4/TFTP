#include "server_tftp.h"

TFTP_SERVER::TFTP_SERVER(int _port, char* _dir){
	server_port = _port;
	strcpy(rootdir,_dir);
	
	if((server_socketfd = socket(AF_INET, SOCK_DGRAM,0)) < 0){
		throw TFTPServerException((char*)"Socket Error");
	}
	
	server_addr.sin_family = AF_UNSPEC;			// host byte order
	server_addr.sin_port = htons(server_port);	// short, network byte order
	server_addr.sin_addr.s_addr = INADDR_ANY;	// auto-fill with my IP
	memset(&(server_addr.sin_zero),0,8);		// zero the rest of the struct
	
	if(bind(server_socketfd,(struct sockaddr*)&server_addr,sizeof(struct sockaddr)) < 0){
		close(server_socketfd);
		throw TFTPServerException((char*)"Bind Error"); }
}


int TFTP_SERVER::run(int max_clients){
	while(true){
		int n = receivePacket(&(clients[0]));
		if(n == -1){
			closeServer();
			return 0;
		}
		else if(n < -1){
			return 0;
		}
		if(n == 0) continue;
		if(processClient(&(clients[0])) == 0){
			disconnect(&clients[0]);
		}
	}
}


int TFTP_SERVER::receivePacket(Client* client){
	int bytes_recv = 0;
	struct pollfd ufd;
	ufd.fd = server_socketfd;
	ufd.events = POLLIN;
	int rv = poll(&ufd,1,4000); // 4 second timeout
	if(rv < 0){
		return -1;
		/* Throw Exception */ }
	else if(rv == 0){
		return 0;
		/* Timeout */ }
	else{// Or client->client_socket
		client->receive_packet.clearPacket();
		client->send_packet.clearPacket();
		socklen_t len = sizeof(client->address);
		bytes_recv = recvfrom(server_socketfd,					//Socket fd
							  client->receive_packet.getData(0),//buffer
							  TFTP_PACKET_MAX_SIZE,				//Size of buffer
							  0,
							  (struct sockaddr*)&(client->address),
							  &len);
		client->receive_packet.setSize(bytes_recv);
		if(bytes_recv < 0){
			return -2;
		}
		return bytes_recv;
	}
	return 0;
}



int TFTP_SERVER::processClient(Client* client){
	client->ip = (char *)inet_ntoa(client->address.sin_addr);
	switch(client->receive_packet.getOpcode()){
		case TFTP_OPCODE_RRQ:{
			/* Find the read file and create a Read Packet to send back */
			client->request_type = REQUEST_READ;
			if((client->client_socket = socket(AF_INET, SOCK_DGRAM,0)) < 0){
				return -1; // Throw Exception
			}
			/* Determine if a dir request or file request */
			char RRQ_filename[MAX_PATH_LENGTH];
			if(client->receive_packet.getString(2,RRQ_filename,MAX_PATH_LENGTH) == 0){
        return 0;
			}
			cout << "RRQ_FILENAME[0] = " << RRQ_filename[0] << endl;
			if(RRQ_filename[0] == '?'){
				if(strlen(RRQ_filename) > 1){
					if(createDirPacket(client,&(RRQ_filename[1])) < 0){
						cout << "TFTP_SERVER::processClient() - Error finding Directory\n";
						return 0;
					}
				}
				else
					if(createDirPacket(client,(char*)".") < 0){
						return 0;
					}
			} else{
				if(getReadFile(client) < 0){
					return 0;
				}
				createReadPacket(client);
			}
			if(client->disconnect_after_send){ 
        disconnect(client); 
        return 0; 
      }
			return TFTP_OPCODE_RRQ;
		}
		case TFTP_OPCODE_WRQ:{
			/* Create the write file and an ACK Packet to send back */
			client->request_type = REQUEST_WRITE;
			if((client->client_socket = socket(AF_INET, SOCK_DGRAM,0)) < 0){
				return 0; // Throw Exception
			}
			createWriteFile(client); // << CHANGE THIS LATER
			
			/* Send ACK Back */
			TFTP_PACKET* ack_packet_WRQ = new TFTP_PACKET();
			ack_packet_WRQ->createACK(client->block);
			delete ack_packet_WRQ;
			/* ~~~~~~~~~~~~~ */
			return TFTP_OPCODE_WRQ;
		}
		case TFTP_OPCODE_DATA:{
			/* Write Packet data to file */
			writeData(client);
			/* Send ACK Back */
			TFTP_PACKET* ack_packet_DATA = new TFTP_PACKET();
			ack_packet_DATA->createACK(client->block);
			delete ack_packet_DATA;
			/* ~~~~~~~~~~~~~ */
			
			if(client->disconnect_after_send){
				cout << "TFTP::processClient() - DATA - Disconnecting...\n" << endl;
				return 0; 
      }
			
			return TFTP_OPCODE_DATA;
		}
		case TFTP_OPCODE_ACK:{
			/* Prepare to send next Read Packet */
			createReadPacket(client);
			if(client->disconnect_after_send){
				cout << "TFTP::processClient() - ACK - Disconnecting...\n" << endl;
				return 0; 
      }
			return TFTP_OPCODE_ACK;
		}
		case TFTP_OPCODE_ERROR:{
			/* Something went wrong, quit */
			return TFTP_OPCODE_ERROR;
		}
		default:{
			/* Unknown Packet received, send back Error packet */
			return -1;
		}
	}
	return -1;
}




int TFTP_SERVER::getReadFile(Client* client){
	char* filename = new char[TFTP_PACKET_MAX_SIZE];
	char actual_file[TFTP_PACKET_MAX_SIZE];
	
	strcpy(filename,rootdir);
	
	client->receive_packet.getString(2,(filename + strlen(filename)),
									client->receive_packet.getSize());
	char at[] = "@";
	strncpy(actual_file,filename,strcspn(filename,at)+1);
	
	client->read_file = new ifstream(filename,ios::binary | ios::in | ios::ate);
	
	if(!client->read_file->is_open() || !client->read_file->good()){
		sendError(client,ERROR_FILE_NOT_FOUND,(char*)"File Not Found");
		disconnect(client);
		return -1;
	}
	client->read_file->seekg(getFileOffset(filename),ios::beg);
	
	delete[] filename;
	return 0;
}




int TFTP_SERVER::createWriteFile(Client* client){
	char* filename = new char[TFTP_PACKET_MAX_SIZE];
	char actual_file[TFTP_PACKET_MAX_SIZE];
	
	strcpy(filename,rootdir);
	
	client->receive_packet.getString(2,(filename + strlen(filename)),
									 client->receive_packet.getSize());
	char at[] = "@";
	strncpy(actual_file,filename,strcspn(filename,at));
	
	client->write_file = new ofstream(actual_file, ios::binary);
	
	client->write_file->seekp(getFileOffset(filename),ios::beg);
	return 0;
}



int TFTP_SERVER::createReadPacket(Client* client){
	char _data[TFTP_PACKET_DATA_SIZE];
	client->read_file->read(_data,TFTP_PACKET_DATA_SIZE);
	
	if(client->read_file->eof()){
		client->disconnect_after_send = true;
	}
	client->send_packet.createData(++client->block,(char*)_data,
								   client->read_file->gcount());
	return 0;
}



int TFTP_SERVER::writeData(Client* client){
	if(++client->block == client->receive_packet.getBlockNumber()){
		
		char _data[TFTP_PACKET_DATA_SIZE];

		int bytes_written = (client->receive_packet.getSize() - 4);

		client->receive_packet.copyData(4,_data,bytes_written);
		
		client->write_file->write(_data,bytes_written);
		
		if(client->receive_packet.getSize() < TFTP_PACKET_DATA_SIZE + 4){
			client->write_file->close();
			client->disconnect_after_send = true;
			return bytes_written;
		}
		return bytes_written;
	}
	return -1;
}



int TFTP_SERVER::createDirPacket(Client* client, char* dir){
	int _data_size = 0;
	if(client->dirPost == 0){
		if(ls(dir,client->dirBuf) < 0){
			sendError(client,ERROR_FILE_NOT_FOUND,(char*)"Directory Not Found");
			disconnect(client);
			return -1;
		}
	}
	(strlen(&(client->dirBuf[client->dirPost])) > 512) ?
		_data_size = 512 :
		_data_size = strlen(&(client->dirBuf[client->dirPost]));
	client->send_packet.createData(++client->block,&(client->dirBuf[client->dirPost])
									,_data_size);
	client->dirPost += _data_size;
	if(strlen(&(client->dirBuf[client->dirPost])) <= 512)
		client->disconnect_after_send = 1;
	return 0;
}



int TFTP_SERVER::sendPacket(TFTP_PACKET* _packet, Client* client){
	socklen_t len = sizeof(client->address);
	int n = sendto(client->client_socket, _packet->getData(0),
				   _packet->getSize(), 0,
				   (struct sockaddr*)&(client->address), len);
	return n;
}


int TFTP_SERVER::sendError(Client* client, int error_code, char* msg){
	TFTP_PACKET* error_packet = new TFTP_PACKET();
	error_packet->createError(error_code, msg);
	sendPacket(error_packet, client);
	delete error_packet;
	return 0;
}


int TFTP_SERVER::disconnect(Client* client){
	if(!client) return 0;
	client->receive_packet.clearPacket();
	client->send_packet.clearPacket();
	client->ip = (char*)"";
	memset(client->dirBuf,0,DIRECTORY_LIST_SIZE);
	client->connection = NOT_CONNECTED;
	client->dirPost = 0;
	client->block = 0;
	client->request_type = REQUEST_UNDEFINED;
	client->temp = 0;
	client->disconnect_after_send = false;
	if(client->client_socket > 0) close(client->client_socket);
	return 0;
}


int TFTP_SERVER::closeServer(){
	if(server_socketfd > 0) {
    close(server_socketfd);
  }
	return 0;
}

int TFTP_SERVER::ls(char* dirName, char* b) {
    int slash_at_end = 0;
    size_t dirNameLen = strlen(dirName);
    
    if (dirNameLen > 0 && dirName[dirNameLen - 1] == '/')
        slash_at_end = 1;

    DIR* dirp;
    if (!(dirp = opendir(dirName))) {
        // Handle directory open failure
        perror("opendir");
        return -1;
    }

    struct dirent* entry;
    string dirlist = "";
    struct stat buf;

    while ((entry = readdir(dirp)) != NULL) {
        if (entry->d_name[0] == '.')
            continue;

        string temp = dirName;
        if (!slash_at_end)
            temp += "/";

        temp += entry->d_name;

        if (entry->d_type == DT_DIR) {
            dirlist += entry->d_name;
            dirlist += "/\n";
        } else {
            dirlist += entry->d_name;
            stat(temp.c_str(), &buf);
            stringstream ss;
            ss << buf.st_size;
            dirlist += "|" + ss.str() + "\n";
        }
    }

    closedir(dirp);
    
    // Copy directory list to the output parameter b
    strcpy(b, dirlist.c_str());

    return 0;
}

TFTP_SERVER::~TFTP_SERVER(){}
