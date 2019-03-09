#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

#define CANBUFSIZE 106
#define MSGBUFSIZE 256
#define TIMEBUFSIZE 128

char msgbuf[MSGBUFSIZE];
char canarybuf[CANBUFSIZE];

void get_time(char* format, char* retstr, unsigned received)
{
  // memory for our local copy of the timestring
  char timebuf[TIMEBUFSIZE];
  time_t curtime;
  struct tm *loctime;

  // Get the current time.
  curtime = time (NULL);

  // Convert it to local time representation.
  loctime = localtime (&curtime);

  // if the format string esceeds our local buffer ...
  if(strlen(format) > TIMEBUFSIZE)
  {
    strcpy(retstr,"Process Error.");
    return;
  }

  memcpy(timebuf,format,received);

  // convert the format string to the real timestring
  strftime(retstr,TIMEBUFSIZE,format,loctime);

  return;
}


int main(int argc, char** argv)
{
  int port; // the portnumber of our service
  int sd; // the socketdescriptor
  struct sockaddr_in addr; // address of our service
  struct sockaddr_in addr_from; //address of the client
  int addrlen = sizeof(addr_from);
  int pid; // our process id
  int sid; // our session id
  unsigned received; // number of bytes received from network

  if(argc != 2)
  {
    printf("Usage: timeservice <portnum>\n");
    return 1;
  }
  
  port = atoi(argv[1]);
  
  if ((port < 1024) || (port > 65535))
  {
    printf("Portrange has to be between 1024 and 65535.\n");
    return 1;
  }

  // forking to background
  pid = fork();

  if(pid < 0)
  {
    printf("fork() failed\n");
    return 1;
  }

  // we are parent
  else if(pid > 0)
  {
    return 0;
  }

  /*
   * we are the child process
   * because of the termination of our parent, we need a new session id,
   * else we are zombie
   */
  sid = setsid();
  if (sid < 0) {
    return 1;
  }

  /*
   * since we are a system service we have to close all standard file 
   * descriptors
   */
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  // create an udp socket
  if((sd = socket(PF_INET,SOCK_DGRAM,IPPROTO_UDP)) < 0)
  {
    return 1;
  }

  // clear the memory of our addr struct
  memset(&addr,0,sizeof(addr));

  // Protocol Family = IPv4
  addr.sin_family = PF_INET; 

  // listen on any incoming interface
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);

  // bind to the udp socket
  if(bind(sd,(struct sockaddr*)&addr,sizeof(addr)) != 0)
  {
    return 1;
  }

  for(;;)
  {
    received = recvfrom(sd,msgbuf,MSGBUFSIZE,MSG_WAITALL,
      (struct sockaddr*)&addr_from,(socklen_t*) &addrlen);

    // fork a new child
    pid = fork();

    // we are parent
    if (pid > 0)
    {
      // wait for the child to finish
      waitpid(pid,NULL,0);
    }
    else
    {
      /*
       * we are inside the child process
       */

      // reserve some memory for our response
      char * returnstr = malloc(TIMEBUFSIZE);

      // analyse the client request and format the time string
      get_time(msgbuf, returnstr, received);

      // send our response to the client
      sendto(sd,returnstr,strlen(returnstr)+1,MSG_DONTWAIT,
        (struct sockaddr *) &addr_from, addrlen);

      free(returnstr);
      return EXIT_SUCCESS;
    }
  }

  close(sd);

  return 0;
}