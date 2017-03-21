#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <netdb.h>
#include<iostream>
#include <fstream>
#include <ctime>
#include <time.h>
#include <sys/select.h>
#include <sys/unistd.h>
#include <sys/fcntl.h>
#include <errno.h>


#include "packet.h"
#include "packet.cpp"



using namespace std;

int main (int argc, char ** argv){
		
    //creates a UDP socket
  int client_to_emulator,emulator_to_client,emulatorport,clientport;
	client_to_emulator=socket(AF_INET, SOCK_DGRAM,0);			
       	//fcntl(client_to_emulator,F_SETFL,O_NONBLOCK); // set socket to non-blocking 
	struct sockaddr_in client;
	memset((char *) &client, 0, sizeof (client));
	client.sin_family= AF_INET;
	clientport=atoi(argv[3]);//UDP port number used by the client to receive ACKS from emulator
	client.sin_port=htons(clientport);
	client.sin_addr.s_addr=htonl(INADDR_ANY);
	

	//get address for emulator
	struct hostent *em;
	em= gethostbyname(argv[1]);
	if(em == NULL)
	  {
	    printf("Error gethostbyname(): %d\n",h_errno);
	  }
	//get port for emulator
	emulator_to_client = socket(AF_INET,SOCK_DGRAM,0);
	//fcntl(emulator_to_client,F_SETFL,O_NONBLOCK); // set socket to non-blocking 
	struct sockaddr_in emulator;
	memset((char *) &emulator, 0, sizeof (emulator));
	emulator.sin_family= AF_INET;
	emulatorport=atoi(argv[2]);//UDP port used by the emulator to recevie data from client
	emulator.sin_port= htons(emulatorport);
	bcopy((char*)em->h_addr,
		 (char*)&emulator.sin_addr.s_addr,
		 em->h_length);
	


	bind(emulator_to_client,(struct sockaddr *)&client,sizeof(client));
	
		//binds  to new socket
	bind(client_to_emulator,(struct sockaddr *)&emulator,sizeof(emulator));

	
	socklen_t emulatorlen=sizeof(emulator);
	socklen_t clientlen = sizeof(client);
	
	char buffer[37];
	char fbuffer[30];
	char data[37]; 
	
	int type;
	int seqnum=0;
	int ackseq=-1;
	int window=0;
	int send_base = 0;
	int location=0;
	int timeout;
	int interrupt;
	int next_seq=0;
	
	fd_set readfds;
	
	ifstream file;
	file.open(argv[4]);
	
	ofstream ackfile;
	ackfile.open("ack.log.txt");	
	
	ofstream seqnumfile;
	seqnumfile.open("seqnum.log.txt");

	FD_ZERO(&readfds);
	FD_SET(client_to_emulator, &readfds);

	
	
	struct timeval tv;
	tv.tv_sec=2;
	tv.tv_usec=0;
	if(setsockopt(emulator_to_client,SOL_SOCKET,SO_RCVTIMEO,(char*)&tv,sizeof(tv)))
	{
		printf("Setsockopt error\n Error Number: %d\n",errno);
	}
	
	#define SOCKET_ERROR -1
	#define TIMEOUT 0
	
	printf("Host name: %s\n IP address: %s\n",argv[1],inet_ntoa(emulator.sin_addr));
	printf("-------------------------------------------------------------\n");

	while (true)
	 {	

	   
		memset ((char*)&buffer,0,sizeof(buffer));
		memset ((char*)&fbuffer,0,sizeof(fbuffer));
		memset ((char*)&data,0,sizeof(data));

		if ( file.eof() && window==0)
		  {
			
			
		    //Makes and sends data EOF packet to server
		    type=3;	
			printf("Sending EOT packet to server\n");
		    packet endpacket(type,seqnum,0,0);
		    endpacket.serialize((char*) buffer);
		    sendto(client_to_emulator,buffer,sizeof(buffer),0,(struct sockaddr *)&emulator, sizeof(emulator));
		    endpacket.printContents();		
		    seqnumfile<<seqnum;
		    seqnumfile<<"\n";
			
		     
		    //Recieves EOF ack from server
			
		 	if(recvfrom(emulator_to_client, data,sizeof(data),0,(struct sockaddr *)&client, &clientlen)<0)
			{
				printf("Error in recv\n");
			}
		    packet endackpacket(0,0,0,0); 		
		    endackpacket.deserialize((char*)data);
		    endackpacket.printContents();
		    ackseq=endackpacket.getSeqNum();
		    ackfile<<ackseq;
		    ackfile<<"\n";
			
			printf("EOT packet recived. Exiting-----------------------\n");
		    break;
			

		  }
		if (!file.eof() && window!=7)
		  {
		    file.read(fbuffer,30);
			type=1;
			
			if (seqnum==8)
			  {
				seqnum=0;
			  }
		
			//Makes and sends data packet to server
			packet datapacket(type,seqnum,sizeof(fbuffer),fbuffer);
			datapacket.serialize((char*) buffer);
			sendto(client_to_emulator,buffer,sizeof(buffer),0,(struct sockaddr *)&emulator, sizeof(emulator));
			
			datapacket.printContents();
			seqnumfile<<seqnum;
			seqnumfile<<"\n";
			next_seq++;	
			
			
			seqnum++;
			window++;
			if (next_seq==8)
			  {
				next_seq=0;
			  }	
			
			printf("SB: %d\n",send_base);
			printf("NS: %d\n",next_seq);
			printf("Number of outstanding packets: %d\n",window);
			printf("-------------------------------------------------------------\n");

		  }
	
	   	 	
		if (window==7 || file.eof())
			 {
			   
				 	if(recvfrom(emulator_to_client, data,sizeof(data),0,(struct sockaddr *)&client, &clientlen)<-1)
					{
						printf("Error in recv\n");
		  	//			location= (-window) * 30;
		  	//			file.seekg(location,ios::cur);
		  	//			seqnum=send_base;
			//			next_seq=0;
		 	//			window=0;
					}
				     packet ackpacket(0,0,0,0);
				     ackpacket.deserialize((char*)data);
				     ackpacket.printContents();
				     ackseq=ackpacket.getSeqNum();
				     ackfile<<ackseq;
					
				     ackfile<<"\n";
	    
					 if(send_base == ackseq)
					   {
						 send_base++;
	  					 window--;
	  		 			if (send_base==8)
	  		 			  {
	  		 				send_base=0;
	  		 			  }	
						
				       } 
					   
					  else
						  {
					
			  				location= (-window) * 30;
			  				file.seekg(location,ios::cur);
			  				seqnum=send_base;
							next_seq=0;
			 				window=0;
							  
						  }  
					 
		 			printf("SB: %d\n",send_base);
		 			printf("NS: %d\n",next_seq);
		 			printf("Number of outstanding packets: %d\n",window);
					printf("-------------------------------------------------------------\n");
				 
				  	if(setsockopt(emulator_to_client,SOL_SOCKET,SO_RCVTIMEO,(char*)&tv,sizeof(tv)))
				  	{
				  		printf("Setsockopt error\n Error Number: %d\n",errno);
				  	}

			  }	
	
		}		
	


	file.close();
	ackfile.close();
	seqnumfile.close();
	close (client_to_emulator);
	close(emulator_to_client);
}
