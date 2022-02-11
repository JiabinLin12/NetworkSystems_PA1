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
  int seq_num;   
  char packet[PACKET_BUFF_SIZE];
  char command[MAX_CMD_LENGHT];
  char file_name[MAX_FILE_LENGTH];
  int err_msg;
}packet_t;

/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}


void sendto_waitforack(void * rc_buf, packet_t * st_buf, int sockfd, uint32_t clientlen, struct sockaddr_in clientaddr){
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
    //							fseek(fp, -MAXBUFSIZE, SEEK_CUR);
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


// printf("pkt.packet=%s\npkt.command=%s\npkt.file_name=%s\npkt.seq_num%d\n", pkt.packet, pkt.command, pkt.file_name,pkt.seq_num); 
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
  n = recvfrom(sockfd, &pkt_rc, sizeof(pkt_rc), 0, (struct sockaddr *)&clientaddr, &clientlen);
  if(n < 0){
    if((errno == EAGAIN) || (errno == EWOULDBLOCK)){
      return;
    }  
    error("error in recvfrom");
  }
  // printf("time = s=%ld,us=%ld",timeout.tv_sec, timeout.tv_usec);
  if((strncmp(pkt_rc.command, "put", sizeof("put"))==0) && (pkt_rc.file_name[0] != '\0')){
    fd_put = fopen(pkt_rc.file_name, "ab");
    if(fd_put == NULL)
      error("error opening file");
    pkt_st.seq_num = pkt_rc.seq_num;
    n = sendto(sockfd, &pkt_st.seq_num, sizeof(pkt_st.seq_num), 0, (struct sockaddr *) &clientaddr, clientlen);
    if (n < 0) 
      error("error in sendto");

    for(int j = 0; j < pkt_rc.pkt_num; j++){
      n = recvfrom(sockfd, &pkt_rc, sizeof(pkt_rc), 0, (struct sockaddr *)&clientaddr, &clientlen);
      if(n < 0)
        error("error in recvfrom");
      
      fwrite(pkt_rc.packet,1,sizeof(pkt_rc.packet), fd_put);
      pkt_st.seq_num = j;
      n = sendto(sockfd, &pkt_st.seq_num, sizeof(pkt_st.seq_num), 0, (struct sockaddr *) &clientaddr, clientlen);
      if (n < 0) 
        error("error in sendto");
      printf("server: finish writing %d/%d packets\n",pkt_rc.seq_num+1, pkt_rc.pkt_num);
    }
    fclose(fd_put); 

    }else if((strncmp(pkt_rc.command, "delete", sizeof("delete"))==0) && (pkt_rc.file_name[0] != '\0')){
      n = remove(pkt_rc.file_name);
      pkt_st.seq_num = pkt_rc.seq_num+n;
      n = sendto(sockfd, &pkt_st, sizeof(pkt_st), 0, (struct sockaddr *) &clientaddr, clientlen);
      if (n < 0) 
        error("error in sendto");
    }else if((strncmp(pkt_rc.command, "get", sizeof("get"))==0) && (pkt_rc.file_name[0] != '\0')){
    fd_get = fopen(pkt_rc.file_name, "r");
    if(fd_get == NULL)
      error("error in fopen"); 
    
   
    /*get file size*/ 
    fseek(fd_get, 0,SEEK_END);
    file_size = ftell(fd_get); 
    fseek(fd_get,0,SEEK_SET);
  
    /*get packet number*/
    pkt_st.pkt_num = (file_size / PACKET_BUFF_SIZE) + 1; 
    pkt_st.seq_num = pkt_rc.seq_num;
    n = sendto(sockfd, &pkt_st, sizeof(pkt_st), 0, (struct sockaddr *) &clientaddr, clientlen);
    if (n < 0) 
      error("error in sendto");
     
    strcpy(pkt_st.file_name, pkt_rc.file_name);
    /*set receive timeout from the server*/
 
    for (int i = 0; i < pkt_st.pkt_num;i++){
      pkt_st.seq_num = i; 
      n = fread(pkt_st.packet,PACKET_BUFF_SIZE,1, fd_get);
      if (n < 0)  
        error("error in fread");
      
      sendto_waitforack(&seq_check, &pkt_st, sockfd,clientlen, clientaddr);
      if (seq_check != i) 
        error("error in sendto_waitforack");
      
      printf("server: finish sending %d/%d packets\n",pkt_st.seq_num+1, pkt_st.pkt_num);
    }
    fclose(fd_get); 
  }else if((strncmp(pkt_rc.command, "ls", sizeof("ls"))==0) && (pkt_rc.file_name[0] == '\0')) {    
    
    pkt_offset = 0;
    memset((pkt_st).packet,0,sizeof((pkt_st).packet));
    pkt_st.seq_num = pkt_rc.seq_num;
    n = sendto(sockfd, &pkt_st, sizeof(pkt_st), 0, (struct sockaddr *) &clientaddr, clientlen);
    if (n < 0) 
      error("error in sendto");
    cur_dir = opendir(".");
    while((list = readdir(cur_dir))!=NULL){
      if(pkt_offset  < PACKET_BUFF_SIZE){
        pkt_offset += sprintf(pkt_st.packet + pkt_offset,"%s\n", list->d_name);
      } 
    } 
    printf("%s\n", pkt_st.packet);
    pkt_st.seq_num = ACK;
    sendto_waitforack(&seq_check, &pkt_st, sockfd,clientlen, clientaddr);
    if (seq_check != ACK)
       error("error in sendto_waitforack");
    closedir(cur_dir);
  }else if ((strncmp(pkt_rc.command, "exit", sizeof("exit"))==0) && (pkt_rc.file_name[0] == '\0')){
    pkt_st.seq_num = pkt_rc.seq_num;
    n = sendto(sockfd, &pkt_st, sizeof(pkt_st), 0, (struct sockaddr *) &clientaddr, clientlen);
    if (n < 0)
      error("error in sendto");
    exit(0);
  }else{
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
  //struct hostent *hostp; /* client host info */
  //char buf[BUFSIZE]; /* message buf */
  //char *hostaddrp; /* dotted decimal host addr string */
  int optval; /* flag value for setsockopt */
  struct timeval timeout = {0,800000};  
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
  setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeout,sizeof(struct timeval)); 

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
  return 0;
}
