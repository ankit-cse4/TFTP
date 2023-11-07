#include "server_tftp.h"
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>

using namespace std;

TFTP_SERVER* tftpServer;

void closeTFTPServer(int sNum, siginfo_t* sInfo, void* ptr)
{
  char response = '0';
  shutdown_prompt:
    cout<<"tftp> do you want to shutdown the tftp server? (y/n)";
    cin >> response;
    if(response == 'n' || response == 'N')
    {
      return;
    }else if(response == 'y' || response == 'Y')
    {
      // break;
    }
    else
    {
      goto shutdown_prompt;
    }
    tftpServer->closeServer();
    for(int i = 0; i < MAX_CLIENTS; ++i)
    {
      tftpServer->disconnect(&(tftpServer->clients[i]));
    }
    delete tftpServer;
    kill(getpid(), SIGTERM);
}

int main(int argc, char* argv[]){
  int portNumber = TFTP_DEFAULT_PORT;
  char* rootDir = (char*)"./";
  struct sigaction act;
  memset(&act, 0, sizeof(act));
  act.sa_flags = SA_SIGINFO;
  act.sa_handler = SIG_IGN;
  act.sa_sigaction = closeTFTPServer;
  sigaction(SIGINT, &act, NULL);

  if(argc == 3){
    rootDir = argv[2];
    portNumber = atoi(argv[1]);
  } else if(argc == 2){
    portNumber= atoi(argv[1]);
  } else {
    cerr << "tftp: Too many arguments\n";
    cout << "tftp [port [rootDir]]\n";
    return 0;
  }

  try{
    while(1){
      tftpServer = new TFTP_SERVER(portNumber, rootDir);
      tftpServer->run(MAX_CLIENTS);
      delete tftpServer;
    } 
  }
  catch(TFTPServerException e){
    cout << "TFTPServerException caught: " << e << endl;
    delete tftpServer;
  }
}