/* 
 * udpserver.c - A simple UDP echo server 
 * usage: udpserver <port>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFSIZE 1024
#define PACKET_BUFF_SIZE  1024
#define ARG_MAX_SIZE 1024
#define MAX_REPEAT_TIMES 5
#define MAX_CMD_LENGHT 10
#define MAX_FILE_LENGTH 100

typedef struct packet_s {
  int seq_num;   
  char packet[PACKET_BUFF_SIZE];
  char command[MAX_CMD_LENGHT];
  char file_name[MAX_FILE_LENGTH];
}packet_t;

/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}

void handle_commands(int sockfd,uint32_t clientlen, struct sockaddr_in clientaddr){
  packet_t pkt;
  int n; /* message byte size */
  FILE *fd;
  n = recvfrom(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&clientaddr, &clientlen);
  if(n < 0){
    error("ERROR in recvfrom");
    return;
  }
 
  if((strncmp(pkt.command, "put", sizeof("put"))==0) && (pkt.file_name != NULL)){
    printf("put\n");
    fd = fopen(pkt.file_name, "a");
    n = fwrite(pkt.packet,1,strlen(pkt.packet), fd);
    printf("wrote %d", n);
    n = sendto(sockfd, &pkt.seq_num, sizeof(pkt.seq_num), 0, (struct sockaddr *) &clientaddr, clientlen);
    if (n < 0) 
      error("ERROR in sendto");
    fclose(fd);
  }else if((strncmp(pkt.command, "get", sizeof("get"))==0) && (pkt.file_name != NULL)){
    printf("get\n");
  }else if((strncmp(pkt.command, "ls", sizeof("ls"))==0) && (pkt.file_name == NULL)) {
    printf("ls\n");
  }else if ((strncmp(pkt.command, "exit", sizeof("exit"))==0) && (pkt.file_name == NULL)){
    printf("exit\n");
  }

  //printf("%s\n%s\n", pkt.command, pkt.file_name);
  
  
}


int main(int argc, char **argv) {
  int sockfd; /* socket */
  int portno; /* port to listen on */
  uint32_t clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  //struct hostent *hostp; /* client host info */
  //char buf[BUFSIZE]; /* message buf */
  //char *hostaddrp; /* dotted decimal host addr string */
  int optval; /* flag value for setsockopt */
  //int n; /* message byte size */

  /* 
   * check command line arguments 
   */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  portno = atoi(argv[1]);

  /* 
   * socket: create the parent socket 
   */
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) 
    error("ERROR opening socket");

  /* setsockopt: Handy debugging trick that lets 
   * us rerun the server immediately after we kill it; 
   * otherwise we have to wait about 20 secs. 
   * Eliminates "ERROR on binding: Address already in use" error. 
   */
  optval = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
	     (const void *)&optval , sizeof(int));

  /*
   * build the server's Internet address
   */
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)portno);

  /* 
   * bind: associate the parent socket with a port 
   */
  if (bind(sockfd, (struct sockaddr *) &serveraddr, 
	   sizeof(serveraddr)) < 0) 
    error("ERROR on binding");

  /* 
   * main loop: wait for a datagram, then echo it
   */
  clientlen = sizeof(clientaddr);
  while (1) {
    handle_commands(sockfd,clientlen, clientaddr);
    // /*
    //  * recvfrom: receive a UDP datagram from a client
    //  */
    // bzero(buf, BUFSIZE);
    // n = recvfrom(sockfd, buf, BUFSIZE, 0,
		//  (struct sockaddr *) &clientaddr, &clientlen);
    // if (n < 0)
    //   error("ERROR in recvfrom");

    // /* 
    //  * gethostbyaddr: determine who sent the datagram
    //  */
    // hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
		// 	  sizeof(clientaddr.sin_addr.s_addr), AF_INET);
    // if (hostp == NULL)
    //   error("ERROR on gethostbyaddr");
    // hostaddrp = inet_ntoa(clientaddr.sin_addr);
    // if (hostaddrp == NULL)
    //   error("ERROR on inet_ntoa\n");
    // printf("server received datagram from %s (%s)\n", 
	  //  hostp->h_name, hostaddrp);
    // printf("server received %d/%d bytes: %s\n", strlen(buf), n, buf);
    
    // /* 
    //  * sendto: echo the input back to the client 
    //  */
    // n = sendto(sockfd, buf, strlen(buf), 0, 
	  //      (struct sockaddr *) &clientaddr, clientlen);
    // if (n < 0) 
    //   error("ERROR in sendto");
  }
}
