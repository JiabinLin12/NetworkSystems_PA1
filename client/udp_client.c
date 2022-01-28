/* 
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <string.h>

#define PACKET_BUFF_SIZE  1024
#define ARG_MAX_SIZE 1024
#define MAX_REPEAT_TIMES 5
#define MAX_CMD_LENGHT 10
#define MAX_FILE_LENGTH 100

#define REWIND_OG 0

typedef struct packet_s {
  int seq_num;   
  char packet[PACKET_BUFF_SIZE];
  char command[MAX_CMD_LENGHT];
  char file_name[MAX_FILE_LENGTH];
}packet_t;

void usage(){
    printf("Usage: \n put [file]\n get [file] \n ls\n exit \n");
}

int recvfrom_wait(int sockfd,uint32_t serverlen, struct sockaddr_in serveraddr){
  int rp_num = MAX_REPEAT_TIMES;
  int rec_seq_num=-1;
  while ((recvfrom(sockfd, &rec_seq_num, sizeof(rec_seq_num), 0, (struct sockaddr *)&serveraddr, &serverlen)<0) 
          && (rp_num > 0)){
    rp_num--;
  }
  if(rp_num<=0){
    printf("error: recvfrom connection timeout");
    return -1;
  }
  return rec_seq_num;
}

void handle_commands(packet_t *pkt, int sockfd,int serverlen, struct sockaddr_in serveraddr){
  struct timeval timeout = {0,800000};  
  size_t file_size = 0;
  int packet_num = 0, seq_num_check = 0;
  FILE * fd;
  
  if((strcmp((*pkt).command,"put")==0) && ((*pkt).file_name != NULL)){
    fd = fopen((*pkt).file_name, "r+b");
    if(fd == NULL){
      perror("Error fopen");
      return; 
    }

    /*set receive timeout from the server*/
    setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeout,sizeof(struct timeval)); 
    /*get file size*/
    fseek(fd, 0,SEEK_END);
    file_size = ftell(fd);
    fseek(fd,0,SEEK_SET);
    /*get packet number*/
    packet_num = (file_size / PACKET_BUFF_SIZE) + 1; 
    printf("total pkts %d\n", packet_num);
    //send dataput
    for(int i = 0; i < packet_num;i++){
      (*pkt).seq_num = i;
      memset((*pkt).packet,0,sizeof((*pkt).packet));
      fread((*pkt).packet, PACKET_BUFF_SIZE, 1, fd);
      sendto(sockfd,pkt,sizeof(*pkt),0, (struct sockaddr *)&serveraddr, serverlen);
      seq_num_check = recvfrom_wait(sockfd,serverlen, serveraddr);
      if(seq_num_check == -1)
        return;
      printf("receive packets number %d, seq_num_check %d\n",i,seq_num_check);
    }
  }else if (strcmp((*pkt).command,"get")==0){

  }else if (strcmp((*pkt).command,"ls")==0){
    
  }else if (strcmp((*pkt).command,"exit")==0){
    
  }

}

/* 
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
}

int main(int argc, char **argv) {
    int sockfd, portno;
    uint32_t serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char argopt[ARG_MAX_SIZE];

    char *cmd, *filename; 
    packet_t pkt;

    /* check command line arguments */
    if (argc != 3) {
       fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
       exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
	  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    /* get a message from the user */
    bzero(argopt, ARG_MAX_SIZE);
    usage();
    fgets(argopt, ARG_MAX_SIZE, stdin);
#if (REWIND_OG == 0)
    /*check user arguments with string token*/
    memset(&pkt,0,sizeof(pkt));
    cmd = strtok(argopt, " \n\r\0");
    filename = strtok(NULL, " \n\r\0");
    strcpy(pkt.command, cmd);
    strcpy(pkt.file_name, filename);

    serverlen = sizeof(serveraddr);
    printf("%s\n",pkt.command );
    /*Handle command*/
    handle_commands(&pkt,sockfd, serverlen, serveraddr);
#else
    /* send the message to the server */
    serverlen = sizeof(serveraddr);
    n = sendto(sockfd, argopt, strlen(argopt), 0, &serveraddr, serverlen);
    if (n < 0) 
      error("ERROR in sendto");
    
    /* print the server's reply */
    n = recvfrom(sockfd, argopt, strlen(argopt), 0, &serveraddr, &serverlen);
    if (n < 0) 
      error("ERROR in recvfrom");
    printf("Echo from server: %s", argopt);
    return 0;
#endif
}

