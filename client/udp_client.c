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
#include <errno.h>
#define PACKET_BUFF_SIZE  1024
#define ARG_MAX_SIZE 1024
#define MAX_REPEAT_TIMES 5
#define MAX_CMD_LENGHT 10
#define MAX_FILE_LENGTH 100

#define REWIND_OG 0
#define ACK 21111112
typedef struct packet_s {
  int pkt_num;
  int seq_num;   
  char packet[PACKET_BUFF_SIZE];
  char command[MAX_CMD_LENGHT];
  char file_name[MAX_FILE_LENGTH];
  int err_msg;
}packet_t;

void usage(){
    printf("\nUsage: \n put [file]\n get [file] \n delete [file]\n ls\n exit \n");
}

/* 
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
}


void sendto_waitforack(void * rc_buf, packet_t * st_buf, int sockfd, uint32_t serverlen, struct sockaddr_in serveraddr){
  int rp_num = MAX_REPEAT_TIMES;
  /**buffer is empty then have nothing to sent**/
  if(st_buf == NULL)
    return;
  /*send the packets*/
  if(sendto(sockfd,st_buf,sizeof(*st_buf),0, (struct sockaddr *)&serveraddr, serverlen)<0){
    error("error in sendto");
  }
  /*not receiving ack, then try to send it several times untill it fails all*/
  while ((recvfrom(sockfd, rc_buf, sizeof(rc_buf), 0, (struct sockaddr *)&serveraddr, &serverlen)<0) 
          && (rp_num > 0)){
    if(sendto(sockfd,st_buf,sizeof(*st_buf),0, (struct sockaddr *)&serveraddr, serverlen)<0){
      error("error in sendto");
    }
    rp_num--;
    printf("send attemp: %d\n", (MAX_REPEAT_TIMES - rp_num));
  }
  if(rp_num<=0){
    error("error: recvfrom connection timeout\n");
  }
  //printf("0.pkt.packet=%s\npkt.command=%s\npkt.file_name=%s\npkt.seq_num%d\n",(*(packet_t*)rc_buf).packet, (*(packet_t*)rc_buf).command, (*(packet_t*)rc_buf).file_name,(*(packet_t*)rc_buf).seq_num); 
}
 

void handle_commands(packet_t *pkt_st, int sockfd,uint32_t serverlen, struct sockaddr_in serveraddr){
  struct timeval timeout = {0,800000};  
  size_t file_size = 0;
  int seq_num_check = -1;
  FILE * fd_put, *fd_get;
  int n = 0;
  packet_t pkt_rc;
  (*pkt_st).err_msg = 0;
  pkt_rc.err_msg = 0;
  memset(&pkt_rc,0,sizeof(pkt_rc));
  printf("set rc to 0\n");
  /*set receive timeout from the server*/
  setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeout,sizeof(struct timeval)); 

  if((strcmp((*pkt_st).command,"put")==0) && ((*pkt_st).file_name[0] != '\0')){
    fd_put = fopen((*pkt_st).file_name, "r");
    if(fd_put == NULL)
      error("Error fopen"); 
     
    
    /*get file size*/
    fseek(fd_put, 0,SEEK_END);
    file_size = ftell(fd_put);  
    fseek(fd_put,0,SEEK_SET);  
    /*get packet number*/ 
    pkt_st->pkt_num = (file_size / PACKET_BUFF_SIZE) + 1;
    pkt_st->seq_num = ACK;
    sendto_waitforack(&seq_num_check, pkt_st, sockfd,serverlen, serveraddr);
    if(seq_num_check != ACK){
      printf("wrong ack value %d",seq_num_check);
      return; 
    }  
     
    //send dataput  
    for(int i = 0; i < (pkt_st->pkt_num);i++){ 
      (*pkt_st).seq_num = i; 
      memset((*pkt_st).packet,0,sizeof((*pkt_st).packet));
      
      fread((*pkt_st).packet, PACKET_BUFF_SIZE, 1, fd_put);
      sendto_waitforack(&seq_num_check, pkt_st, sockfd,serverlen, serveraddr);
      if(seq_num_check != i)
        error("ack value not expected");
      printf("client: finish sending %d/%d packets\n",(*pkt_st).seq_num+1, (*pkt_st).pkt_num);
    }
  }else if (strcmp((*pkt_st).command,"get")==0 && ((*pkt_st).file_name[0] != '\0')){
      //send get 
      memset((*pkt_st).packet,0,sizeof((*pkt_st).packet));
      (*pkt_st).seq_num = ACK;
      sendto_waitforack(&pkt_rc, pkt_st, sockfd,serverlen, serveraddr);
      if(pkt_rc.seq_num != ACK){
        printf("wrong ack value %d",seq_num_check);
        return; 
      }
      /*set receive timeout from the server*/
      for( int i = 0; i < (pkt_rc).pkt_num; i++){
        n = recvfrom(sockfd, &pkt_rc, sizeof(pkt_rc), 0, (struct sockaddr *)&serveraddr, &serverlen);
        if(n < 0){
          printf("wrong ack value %d",seq_num_check);
          return; 
        }
        (*pkt_st).seq_num = i;
        n = sendto(sockfd, &(*pkt_st).seq_num, sizeof((*pkt_st).seq_num), 0, (struct sockaddr *)&serveraddr, serverlen);
        if (n < 0){ 
          error("error in sendto");
          return;
        }  
        fd_get = fopen((pkt_rc).file_name, "a");
        if(fd_get == NULL){
          error("error in fopen");
          return; 
        }
        n = fwrite((pkt_rc).packet,1,sizeof((pkt_rc).packet), fd_get);
        printf("client: finish writing %d/%d packets\n",(pkt_rc).seq_num+1, (pkt_rc).pkt_num);
        fclose(fd_get); 
      }
  }else if(strcmp((*pkt_st).command,"delete")==0 && ((*pkt_st).file_name[0] != '\0')){
      memset((*pkt_st).packet,0,sizeof((*pkt_st).packet));
      (*pkt_st).seq_num = 0;
      sendto_waitforack(&pkt_rc, pkt_st, sockfd,serverlen, serveraddr);
      if(pkt_rc.seq_num < 0){
        printf("file does not exist\n");
      }else{
        printf("%s removed succesfully\n",(*pkt_st).file_name);
      }
      
  }else if (strcmp((*pkt_st).command,"ls")==0){
      memset((*pkt_st).packet,0,sizeof((*pkt_st).packet));
      (*pkt_st).seq_num = 0;
      sendto_waitforack(&pkt_rc, pkt_st, sockfd,serverlen, serveraddr);
      if(pkt_rc.seq_num != 0)
        return; 
      
      n = recvfrom(sockfd, &pkt_rc, sizeof(pkt_rc), 0, (struct sockaddr *)&serveraddr, &serverlen);
      if(n < 0){
        error("error in recvfrom");
      } 
      (*pkt_st).seq_num = pkt_rc.seq_num;
      n = sendto(sockfd, &(*pkt_st).seq_num, sizeof((*pkt_st).seq_num), 0, (struct sockaddr *)&serveraddr, serverlen);
      if (n < 0)
        error("error in sendto");
      
      printf("%s",pkt_rc.packet);
  }else if (strcmp((*pkt_st).command,"exit")==0){
      (*pkt_st).seq_num = ACK;
      sendto_waitforack(&pkt_rc, pkt_st, sockfd,serverlen, serveraddr);
      if(pkt_rc.seq_num != ACK)
        error("error in sendto_waitforack");
      
      printf("client: server exited\n");
  }else{
    sendto_waitforack(&pkt_rc.command, pkt_st, sockfd,serverlen, serveraddr);
    printf("command was not understood: %s\n", pkt_rc.command);
  }    
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
        error("error opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"error, no such host as %s\n", hostname);
        exit(0);
    }
 
    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
	  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);
#if (REWIND_OG == 0)
  while(1){
    memset(&pkt,0,sizeof(pkt));
    /* get a message from the user */  
    bzero(argopt, ARG_MAX_SIZE);
    usage(); 
    fgets(argopt, ARG_MAX_SIZE, stdin);
      /*check user arguments with string token*/
    memset(&pkt,0,sizeof(pkt));
    cmd = strtok(argopt, " \n\r\0");
    filename = strtok(NULL, " \n\r\0");
    if(cmd == NULL){ 
      printf("error: no input command\n");
      return 0;
    } 
    strcpy(pkt.command, cmd);
    if(filename!=NULL)
      strcpy(pkt.file_name, filename);
    else
      pkt.file_name[0] = '\0'; 

    serverlen = sizeof(serveraddr); 
    /*Handle command*/
    handle_commands(&pkt,sockfd, serverlen, serveraddr);
  }  
    return 0; 
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

