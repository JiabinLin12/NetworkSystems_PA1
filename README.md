# NetworkSystems_PA1

This is the code for Network Systems PA1
* This program handles commad: [put],[get],[delete],[ls],[exit] through UDP
  * [put] is to transfer file and write it from client to server
  * [get] is to transfer file and write it from server to client
  * [delete] is to delete file from server
  * [ls] list all the files in server directory
  * [exit] exits the server proecess gracefully 
* UDP realiable transfer  
  * adding sequence number 
  * retransfer the packet up to 5 times if the packet is lost. 
  * connection time out and ends the process if maximum retransfer attempts is tried
* Verify packet transfer outside of the C program using md5sum in terminal
  * use commad: md5sum file   
* Both programs ends by pressing [CTRL+C] or the program will ends if error happens

Instruction to run the program:
* clone this directory 
* cd NetworkSystems_PA1
* For client
* * run [make]
* * run [cd client]
* * run [./udp_client <ip> <port>]
* * Interface should prompt user with its usage
* For server
* * run [make]
* * run [cd server]
* * run [./udp_server <port>]

  

