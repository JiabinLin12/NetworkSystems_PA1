CC = gcc
CFLAGS = -g -Wall -Werror

all: client server

client: client/udp_client.c
	$(CC) $(CFLAGS) client/udp_client.c -o client/udp_client
#	cd client && ./udp_client localhost 5001
	

server: server/udp_server.c
	$(CC) $(CFLAGS) server/udp_server.c -o server/udp_server
#	cd server && ./udp_server 5001
	
cleanclient:
	rm client/udp_client
cleanserver:
	rm server/udp_server
clean:
	rm server/udp_server client/udp_client

