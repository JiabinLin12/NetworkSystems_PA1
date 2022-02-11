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
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#define BUFSIZE 1024
#define PACKET_BUFF_SIZE  1024
#define ARG_MAX_SIZE 1024
#define MAX_REPEAT_TIMES 5
#define MAX_CMD_LENGHT 10 
#define MAX_FILE_LENGTH 100
#define ACK 21111112

typedef struct packet_s {
  int pkt_num;
  int pkt_len;
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

/*
 * send packet and wait for buffer to be received, resend if nothing is received
 */
void sendto_then_recv(void * rc_buf, packet_t * st_buf, int sockfd, uint32_t clientlen, struct sockaddr_in clientaddr){
  int rp_num = MAX_REPEAT_TIMES;
  int err = -1;
  /**buffer is empty then have nothing to sent**/
  if(st_buf == NULL)
    return;
  /*send the packets*/
  err = sendto(sockfd,st_buf,sizeof(*st_buf),0, (struct sockaddr *)&clientaddr, clientlen);
  if(err < 0){
    error("error in sendto");
  }
  /*not receiving ack, then try to send it several times untill it fails all*/
  while ((recvfrom(sockfd, rc_buf, sizeof(rc_buf), 0, (struct sockaddr *)&clientaddr, &clientlen)<0) 
          && (rp_num > 0)){
    if(sendto(sockfd,st_buf,sizeof(*st_buf),0, (struct sockaddr *)&clientaddr, clientlen)<0){
      error("error in sendto");
    }
    rp_num--;
    printf("send attemp: %d\n", (MAX_REPEAT_TIMES - rp_num));
  }
  if(rp_num<=0){
    error("error: recvfrom connection timeout\n");
  }
}

/*
 * This function is handling commands send from client
 * it can handles [ls, delete, put, exit, get] and echo back unrecognize commands
 */
void handle_commands(int sockfd,uint32_t clientlen, struct sockaddr_in clientaddr){
  packet_t pkt_st, pkt_rc; 
  int n; /* message byte size */ 
  FILE *fd_put, *fd_get;
  size_t file_size = 0;
  int seq_check = -1;
  DIR *cur_dir;
  struct dirent *list;
  int pkt_offset; 
  memset(&pkt_st,0,sizeof(pkt_st));
  memset(&pkt_rc,0,sizeof(pkt_rc));
  /*waiting packet from client*/
  n = recvfrom(sockfd, &pkt_rc, sizeof(pkt_rc), 0, (struct sockaddr *)&clientaddr, &clientlen);
  if(n < 0){
    if((errno == EAGAIN) || (errno == EWOULDBLOCK)){
      return;
    }  
    error("error in recvfrom");
  }
    /*put command*/
  if((strncmp(pkt_rc.command, "put", sizeof("put"))==0) && (pkt_rc.file_name[0] != '\0')){
    /*open a file*/
    fd_put = fopen(pkt_rc.file_name, "ab");
    if(fd_put == NULL) 
      error("error opening file");
    
    /*set sequence and ack back*/
    pkt_st.seq_num = pkt_rc.seq_num;
    n = sendto(sockfd, &pkt_st.seq_num, sizeof(pkt_st.seq_num), 0, (struct sockaddr *) &clientaddr, clientlen);
    if (n < 0)  
      error("error in sendto");
    /*write file down to server machine*/
    for(int j = 0; j < pkt_rc.pkt_num; j++){
      /*receive file content from client*/
      n = recvfrom(sockfd, &pkt_rc, sizeof(pkt_rc), 0, (struct sockaddr *)&clientaddr, &clientlen);
      if(n == -1)
        error("error in recvfrom");
      
      /*write file*/
      n = fwrite(pkt_rc.packet,1,pkt_rc.pkt_len, fd_put);
      if(n != pkt_rc.pkt_len)
        error("error in fwrite");
      
      /*send ack back to client*/
      pkt_st.seq_num = j;
      n = sendto(sockfd, &pkt_st.seq_num, sizeof(pkt_st.seq_num), 0, (struct sockaddr *) &clientaddr, clientlen);
      if (n < 0) 
        error("error in sendto");
      printf("server: finish writing %d/%d packets\n",pkt_rc.seq_num+1, pkt_rc.pkt_num);
    }
    fclose(fd_put);

    /*get command*/
    }else if((strncmp(pkt_rc.command, "get", sizeof("get"))==0) && (pkt_rc.file_name[0] != '\0')){
      /*open file to read*/
      fd_get = fopen(pkt_rc.file_name, "r");
      if(fd_get == NULL)
        error("error in fopen"); 
    
      /*get file size*/ 
      fseek(fd_get, 0,SEEK_END);
      file_size = ftell(fd_get); 
      fseek(fd_get,0,SEEK_SET);
    
      /*get packet number*/
      pkt_st.pkt_num = (file_size / PACKET_BUFF_SIZE);
      pkt_st.pkt_num = ((file_size%PACKET_BUFF_SIZE) == 0)? pkt_st.pkt_num : pkt_st.pkt_num+1; 

      /*send ack to client*/
      pkt_st.seq_num = pkt_rc.seq_num;
      n = sendto(sockfd, &pkt_st, sizeof(pkt_st), 0, (struct sockaddr *) &clientaddr, clientlen);
      if (n < 0) 
        error("error in sendto");
      
      strcpy(pkt_st.file_name, pkt_rc.file_name);
      /*sending packet*/  
      for (int i = 0; i < pkt_st.pkt_num;i++){
        /*padding packet with sequence number, packet length and file content*/
        pkt_st.seq_num = i; 
        pkt_st.pkt_len = fread(pkt_st.packet,1,PACKET_BUFF_SIZE, fd_get);
        if (pkt_st.pkt_len < 0)  
          error("error in fread");
        /*send and receive ack*/
        sendto_then_recv(&seq_check, &pkt_st, sockfd,clientlen, clientaddr);
        if (seq_check != i) 
          error("error in sendto_then_recv");
        
        printf("server: finish sending %d/%d packets\n",pkt_st.seq_num+1, pkt_st.pkt_num);
      }
      fclose(fd_get); 
    /*delete command*/
  }else if((strncmp(pkt_rc.command, "delete", sizeof("delete"))==0) && (pkt_rc.file_name[0] != '\0')){
      /*remove file n is negative if fails*/
      n = remove(pkt_rc.file_name);
      /*seq number would be negative if it fails. Check on the other side*/
      pkt_st.seq_num = pkt_rc.seq_num+n;
      n = sendto(sockfd, &pkt_st, sizeof(pkt_st), 0, (struct sockaddr *) &clientaddr, clientlen);
      if (n < 0) 
        error("error in sendto");
    /*ls command*/
  }else if((strncmp(pkt_rc.command, "ls", sizeof("ls"))==0) && (pkt_rc.file_name[0] == '\0')) {  
    pkt_offset = 0;
    /*setting sequence number to ack*/
    pkt_st.seq_num = pkt_rc.seq_num;
    n = sendto(sockfd, &pkt_st, sizeof(pkt_st), 0, (struct sockaddr *) &clientaddr, clientlen);
    if (n < 0) 
      error("error in sendto");

    /*add list to packet content*/
    cur_dir = opendir(".");
    while((list = readdir(cur_dir))!=NULL){
      if(pkt_offset  < PACKET_BUFF_SIZE){
        pkt_offset += sprintf(pkt_st.packet + pkt_offset,"%s\n", list->d_name);
      } 
    }
    printf("\n**************ls**************\n"); 
    printf("%s\n", pkt_st.packet);
    /*send and wait for ack back from client*/
    pkt_st.seq_num = ACK;
    sendto_then_recv(&seq_check, &pkt_st, sockfd,clientlen, clientaddr);
    if (seq_check != ACK)
       error("error in sendto_then_recv");
    closedir(cur_dir);
    /*exit command*/
  }else if ((strncmp(pkt_rc.command, "exit", sizeof("exit"))==0) && (pkt_rc.file_name[0] == '\0')){
    /*set and send ack back to client*/
    pkt_st.seq_num = pkt_rc.seq_num;
    n = sendto(sockfd, &pkt_st.seq_num, sizeof(pkt_st.seq_num), 0, (struct sockaddr *) &clientaddr, clientlen);
    if (n == -1)
      error("error in sendto");
    exit(0);
    /*other commands*/
  }else{
    /*echo back*/
    memcpy(pkt_st.command,pkt_rc.command,sizeof(pkt_rc));
    n = sendto(sockfd, pkt_st.command, sizeof(pkt_st.command), 0, (struct sockaddr *) &clientaddr, clientlen);
    if(n == -1)
      error("error in sendto");
    printf("command was not understood: %s\n", pkt_rc.command);
  }
}   
 

int main(int argc, char **argv) {
  int sockfd; /* socket */
  int portno; /* port to listen on */
  uint32_t clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  int optval; /* flag value for setsockopt */
  struct timeval timeout = {0,800000};  
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
  setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeout,sizeof(struct timeval)); 

  /* 
   * main loop: wait for a datagram, then echo it
   */
  clientlen = sizeof(clientaddr); 
  while (1) { 
    handle_commands(sockfd,clientlen, clientaddr);
  }
  return 0;
}
