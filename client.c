#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "protocol.h"

#define BUFFERSIZE 256

void game_over(char *buffer, int len);
void do_turn(int socket);
void print_board(char *buffer);

void invalid_turn(int socket, char *buffer, int len);
int get_server_connection(char *hostname, char *port);
void compose_http_request(char *http_request, char *filename);
void web_browser(int http_conn, char *http_request);
void print_ip( struct addrinfo *ai);

int main(int argc, char *argv[])
{
    int socket;  
    char buffer[BUFFERSIZE];
	int numbytes = 0;

    //get a connection to server
    if ((socket = get_server_connection(HOST, HTTPPORT)) == -1) {
       printf("connection error\n");
       exit(1);
    }

	//receive from the server and act upon the command received
	while((numbytes=recv(socket, buffer, sizeof(buffer),  0)) > 0) {
	   if(numbytes < 0) {
		  perror("recv");
		  exit(1);
		}
		buffer[numbytes] = '\0';

		switch(buffer[0]) {
		case P_WAIT:
			printf("Waiting for other player...\n");
			break;

		case P_BOARD:
			print_board(buffer);
			break;

		case P_YOUR_TURN:
			print_board(&buffer[1]);
			printf("\nEnter the location for your next move: ");
			do_turn(socket);
			break;

		case P_INVALID:
			invalid_turn(socket, buffer, sizeof(buffer));
			continue;

		case P_GAMEOVER:
			game_over(buffer, sizeof(buffer));
			print_board(&buffer[2]);
			close(socket);
			exit(0);
		}
	   
	}
    
}

//prints the board to stdout
void print_board(char *buffer) {
	char i;
	for (i = 0; i < 9; i++) {
		if(buffer[i] == 0) {
			printf("_ ");
		} else {
			printf("%c ", buffer[i]);
		}
		if( (i + 1) % 3 == 0) {
			printf("\n");
		}
	}
}

//handles the user taking his turn
void do_turn(int socket) {
	char msg[3];
	msg[0] = P_MOVE;

	scanf("%d %d", &msg[1], &msg[2]);

	if(send(socket, msg, sizeof(msg), 0) < 0) {
		perror("could not send.");
		exit(1);
	}
}

//the server said the user did not put in valid input
void invalid_turn(int socket, char *buffer, int len) {
	char msg[3];
	msg[0] = P_MOVE;

	switch(buffer[1]) {
	case Q_OUT_OF_RANGE:
		printf("Location out of range.\n");
		break;
	case Q_LOC_TAKEN:
		printf("Location taken.\n");
	}
}

//the game is over
void game_over(char *buffer, int len) {
	switch(buffer[1]) {
	case Q_GAME_DRAW:
		printf("\nThe game ended in a tie.\n");
		break;
	case Q_YOU_WON:
		printf("\nYou won!\n");
		break;
	case Q_YOU_LOST:
		printf("\nYou lose!\n");
		break;
	}
}

//by dr. bi
int get_server_connection(char *hostname, char *port) {
    int serverfd;
    struct addrinfo hints, *servinfo, *p;
    int status;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

   if ((status = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) {
       printf("getaddrinfo: %s\n", gai_strerror(status));
       return -1;
    }

    print_ip(servinfo);
    for (p = servinfo; p != NULL; p = p ->ai_next) {
       if ((serverfd = socket(p->ai_family, p->ai_socktype,
                           p->ai_protocol)) == -1) {
           printf("socket socket \n");
           continue;
       }

       if (connect(serverfd, p->ai_addr, p->ai_addrlen) == -1) {
           close(serverfd);
           printf("socket connect \n");
           continue;
       }
       break;
    }

    freeaddrinfo(servinfo);
   
    return serverfd;
}

void  compose_http_request(char *http_request, char *filename) {
    strcpy(http_request, "GET /");
    strcpy(&http_request[5], filename);
    strcpy(&http_request[5+strlen(filename)], " ");
}

void web_browser(int http_conn, char *http_request) {
    int numbytes = 0;
    char buf[256];
    // step 4.1: send the HTTP request
    send(http_conn, http_request, strlen(http_request), 0);

    // step 4.2: receive message from server
    while ((numbytes=recv(http_conn, buf, sizeof(buf),  0)) > 0) {
       if ( numbytes < 0)  {
          perror("recv");
          exit(1);
        }

       // step 4.3: the received may not end with a '\0' 
       buf[numbytes] = '\0';
       printf("%s",buf);
    }
}

void print_ip( struct addrinfo *ai) {
   struct addrinfo *p;
   void *addr;
   char *ipver;
   char ipstr[INET6_ADDRSTRLEN];
   struct sockaddr_in *ipv4;
   struct sockaddr_in6 *ipv6;
   short port = 0;

   for (p = ai; p !=  NULL; p = p->ai_next) {
      if (p->ai_family == AF_INET) {
         ipv4 = (struct sockaddr_in *)p->ai_addr;
         addr = &(ipv4->sin_addr);
         port = ipv4->sin_port;
         ipver = "IPV4";
      }
      else {
         ipv6= (struct sockaddr_in6 *)p->ai_addr;
         addr = &(ipv6->sin6_addr);
         port = ipv4->sin_port;
         ipver = "IPV6";
      }
      inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
      printf("serv ip info: %s - %s @%d\n", ipstr, ipver, ntohs(port));
   }
}
