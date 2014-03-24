#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/wait.h>
#include <netdb.h>
#include <errno.h>
#include <stdarg.h>
#include "shared.h"
#include "semaphore.h"
#include "protocol.h"

#define BACKLOG 10

//asgn 7 - player records
#define MEMORY_KEY 32500
#define MAX_RECORDS 10

typedef struct PlayerRecord {
   int playerID;
   char firstName[10];
   char lastName[10];
   int wins;
   int losses;
   int ties;
} Player;

void dprintf(const char *fmt, ...);

void load_records(char *filename, Player *records);
void save_records(char *filename, Player * records);

void send_record_msg(int socket, Player *record);

void print_records(Player *records);
void print_board(char board[][3]);
char checkWinner(char board[][3]);
char get_player_symbol(char player);
int get_player_index(int id, Player *records);
void send_game_over(int socket, char flag, char board[][3]);
void send_id_msg(int socket);
void send_record_msg(int socket, Player *record);
void send_inv_msg(int socket, char flag);
void send_turn_msg(int socket, char board[][3]);
void send_wait_msg(int socket);
void append_board(char msg[], char start_index, char board[][3]);

void reap_terminated_child(int status);              // reap subservers
void *get_in_addr(struct sockaddr * sa);             // get internet address
int get_server_socket(char *hostname, char *port);   // get a server socket
int start_server(int serv_socket, int backlog);      // start server's listening
int accept_client(int serv_sock);                    // accept a connection from client
void subserver(int client1_sock, int client2_sock, Player *records); // subserver - subserver
void print_ip( struct addrinfo *ai);                 // print IP info from getaddrinfo()

Semaphore mutex(1, MEMORY_KEY);

int debug = 0; //global variable to determine whether or not server is being run in "debug mode"

int main(int argc, char *argv[]) {
	int server_sock = 0;
	
	int client1_sock = 0;
	int client2_sock = 0;

	Shared<Player> records(MAX_RECORDS, MEMORY_KEY);

	if(argc == 3 && strcmp("-d", argv[2]) == 0) {
		printf("Running in debug mode.\n");
		debug = 1;
	}
	
	load_records(argv[1], records);

	print_records(records);

	signal(SIGCHLD, reap_terminated_child);

	//set up the server
	dprintf("Initializing server.\n");
	server_sock = get_server_socket(HOST, HTTPPORT);
	
	if(start_server(server_sock, BACKLOG) == -1) {
		printf("Error starting server: %s.\n", strerror(errno));
		exit(1);
	}

	while(1) {
		//client 1 has already connected to the server
		if(client1_sock != 0) {
			client2_sock = accept_client(server_sock);
			dprintf("Received connection from second client.\n");
			
			//fork for subserver
			if (!fork()) { // child process, so start the subserver
			
			   close(server_sock); //no longer needed in child process
			   dprintf("Preparing to play.\n");
			   subserver(client1_sock, client2_sock, records);
			   
			} else { //parent process
			
				//reset client sockets for more connections
			   close(client1_sock);
			   close(client2_sock);
			   client1_sock = 0;
			   client2_sock = 0;
			}
			
		} else { //client 1 has not connected yet
			client1_sock = accept_client(server_sock);
			dprintf("Received connection from first client.\n");

			char msg = P_WAIT; //tell the client just to wait
			if(send(client1_sock, &msg, sizeof(msg), 0) < 0) {
				printf("Unable to send: %s\n", strerror(errno));
				exit(1);
			}
		}
	}

	save_records(argv[1], records);

	mutex.remove();
	records.remove();

	exit(0);
}

/*
*	Load player records from the specified file
*/

void load_records(char *filename, Player *records) {
	int fd = open(filename, O_RDWR, S_IRWXU | S_IRWXO);
	int num = 0;
	int i = 0;
	
	num = read(fd, &records[i], sizeof(Player));
	while(num > 0) {
		i = i + 1;
		
		if(i == MAX_RECORDS) {
			printf("Max number of users reached: %d.\n", MAX_RECORDS);
		}
		
		num = read(fd, &records[i], sizeof(Player));
	}
	
	close(fd);
}

void save_records(char *filename, Player *records) {
	int fd = open(filename, O_WRONLY | O_CREAT, S_IRWXU | S_IRWXO);
	int num = 0;
	int i = 0;
	
	for(i = 0; i < MAX_RECORDS; i = i + 1) {
		num = write(fd, &records[i], sizeof(Player));
	}
		
	close(fd);
}


void print_records(Player *records) {
	int i;
	for(i = 0; i < MAX_RECORDS; i = i + 1) {
		printf("Player: id = %d, first name = %s, last name = %s\n", records[i].playerID, records[i].firstName, records[i].lastName);
	}
}

/*
*	Where child processes will communicate with clients and run the game.
*/
void subserver(int client1_sock, int client2_sock, Player *records) {

	//these will be used to control whose turn it is
	int current_sock;
	int waiting_sock;

	//player data
	int player1_index = -1;
	int player2_index = -1;

	//game data
	char board[3][3];
	char turn = 1; //tracks whose turn it is
	char turn_count = 0;
	char winner = 0;
	
	//networking data
	int read_count = -1;
	int BUFFERSIZE = 256;
	char buffer[BUFFERSIZE+1];

	char x, y;
	

	//get players to "login"
	int t_id = 0;
	dprintf("Getting player 1 user id...\n");
	while(player1_index == -1) {
		send_id_msg(client1_sock);

		//get user input from client
		read_count = recv(client1_sock, buffer, BUFFERSIZE, 0);
		buffer[read_count] = '\0';

		t_id = buffer[1];
		player1_index = get_player_index(t_id, records);
	}
	send_record_msg(client1_sock, &records[player1_index]);
	
	t_id = 0;
	dprintf("Getting player 2 user id...\n");
	while(player2_index == -1) {
		send_id_msg(client2_sock);

		//get user input from client
		read_count = recv(client2_sock, buffer, BUFFERSIZE, 0);
		buffer[read_count] = '\0';

		t_id = buffer[1];
		player2_index = get_player_index(t_id, records);
	}
	send_record_msg(client2_sock, &records[player2_index]);
	//both players are now logged in
	
	//initialize the board array to all 0's
	dprintf("Preparing game board...\n");
	for (x = 0; x < 3; x = x + 1) {
		for(y = 0; y < 3; y = y + 1) {
			board[x][y] = 0;
		}
	}

	//let the game begin!
	while(turn_count < 9) {
		
		//set up "reply" sockets based on whose turn it is
		if(turn == 1) {
			current_sock = client1_sock;
			waiting_sock = client2_sock;
		} else {
			current_sock = client2_sock;
			waiting_sock = client1_sock;
		}

		//tell idle player to wait
		send_wait_msg(waiting_sock);
		//alert current player it's her turn
		send_turn_msg(current_sock, board);

		//get user input from client
		read_count = recv(current_sock, buffer, BUFFERSIZE, 0);
		buffer[read_count] = '\0';

		if(buffer[0] == P_MOVE) {
			x = buffer[1];
			y = buffer[2];
			
			dprintf("Player input: %d %d\n", x, y);
			
			//first, make sure the input is valid
			if(x < 0 || x > 2 || y < 0 || y > 2) {
				dprintf("Input error: out of range\n");
				send_inv_msg(current_sock, Q_OUT_OF_RANGE);
				continue;
			} else if(board[x][y] > 0) {
				dprintf("Input error: location taken\n");
				send_inv_msg(current_sock, Q_LOC_TAKEN);
				continue;
			}
			
			//update the board
			board[x][y] = get_player_symbol(turn);
			
			if(debug > 0) {
				print_board(board);
			}
			
			turn_count = turn_count + 1;
			
			winner = checkWinner(board);
			if(winner != 0) {
				if(winner == 'X') {
					dprintf("Game over. Player 1 wins!");

					mutex.wait();
					//CRITICAL SECTION!!!!!!
					records[player1_index].wins++;
					records[player2_index].losses++;
					mutex.signal();

					send_game_over(client1_sock, Q_YOU_WON, board);
					send_game_over(client2_sock, Q_YOU_LOST, board);
					close(client1_sock);
					close(client2_sock);
					exit(0); 
				} else {
					dprintf("Game over. Player 2 wins!");

					mutex.wait();
					//CRITICAL SECTION!!!!!!
					records[player2_index].wins++;
					records[player1_index].losses++;
					mutex.signal();

					send_game_over(client2_sock, Q_YOU_WON, board);
					send_game_over(client1_sock, Q_YOU_LOST, board);
					close(client1_sock);
					close(client2_sock);
					exit(0);
				}
			}
		}

		//prepare for next turn
		if(turn == 1) {
			turn = 2;
		} else {
			turn = 1;
		}
	}
	
	//if we make it this far, the game was a draw
	send_game_over(client1_sock, Q_GAME_DRAW, board);
	send_game_over(client2_sock, Q_GAME_DRAW, board);

	records[player1_index].ties++;
	records[player2_index].ties++;

	//goodbye
	close(client1_sock);
	close(client2_sock);

	exit(0);
}

/*
*	Appends msg with a listing of the board to send to the client
*/
void append_board(char msg[], char start_index, char board[][3]) {
	char x, y, c;
	
	c = start_index;
	for (x = 0; x < 3; x = x + 1) {
		for (y = 0; y < 3; y = y + 1) {
			msg[c] = board[x][y];
			c = c + 1;
		}
	}

}

/*
*	Print the board for any users of the server
*/
void print_board(char board[][3]) {
	char x, y;
	
	for (x = 0; x < 3; x = x + 1) {
		for (y = 0; y < 3; y = y + 1) {
			if(board[x][y] == 0) {
				printf("_ ");
			} else {
				printf("%c ", board[x][y]);
			}
		}
		printf("\n");
	}
}

int get_player_index(int id, Player *records) {
	int i;
	mutex.wait();
	for(int i = 0; i < MAX_RECORDS; i++) {
		if(records[i].playerID == id) return i;
	}
	mutex.signal();
	return -1;
}

/*
*	Returns the specified player's symbol for the board
*/
char get_player_symbol(char player) {
	if(player == 1) {
		return 'X';
	} else {
		return 'O';
	}
}

/*
*	Returns the winning symbol for board, or 0 is there is none
*/
char checkWinner(char board[][3]) {
	char i;
	
	//check horizontally
	for(i = 0; i < 3; i=i+1) {
		if(board[i][0] == board[i][1] && board[i][1] == board[i][2]) {
			return board[i][0];
		}
	}
	
	//check vertically
	for(i = 0; i < 3; i=i+1) {
		if(board[0][i] == board[1][i] && board[1][i] == board[2][i]) {
			return board[0][i];
		}
	}
	
	//check diagonals
	if((board[0][0] == board[1][1] && board[1][1] == board[2][2]) ||
	   (board[0][2] == board[1][1] && board[1][1] == board[2][0])) {
		return board[1][1];
	}
	
	return 0;
}

/*
*	Sends the "id" message to the client specified by socket
*/
void send_id_msg(int socket) {
	char msg = P_UID;
	if(send(socket, &msg, sizeof(msg), 0) < 0) {
		perror("Error sending P_UID message to client.");
		exit(1);
	}
}

/*
*	Sends the "record" message to the client specified by socket
*/
void send_record_msg(int socket, Player *record) {
	char msg[26];
	msg[0] = P_RECORD;
	msg[1] = record->playerID;
	strcpy(&msg[2],record->firstName);
	strcpy(&msg[12],record->lastName);
	msg[23] = record->wins;
	msg[24] = record->losses;
	msg[25] = record->ties;

	if(send(socket, &msg, sizeof(msg), 0) < 0) {
		perror("Error sending P_RECORD message to client.");
		exit(1);
	}
}


/*
*	Sends the "wait" message to the client specified by socket
*/
void send_wait_msg(int socket) {
	char msg = P_WAIT;
	if(send(socket, &msg, sizeof(msg), 0) < 0) {
		perror("Error sending P_WAIT message to client.");
		exit(1);
	}
}

/*
*	Sends the "your turn" message to the client specified by socket
*/
void send_turn_msg(int socket, char board[][3]) {
	char msg[10];
	msg [0] = P_YOUR_TURN;
	append_board(msg, 1, board);

	if(send(socket, &msg, sizeof(msg), 0) < 0) {
		perror("Error sending P_YOUR_TURN message to client.");
		exit(1);
	}
}

/*
*	Sends the "invalid input" message to the client specified by socket
*/
void send_inv_msg(int socket, char flag) {
	char msg[2];
	msg[0] = P_INVALID;
	msg[1] = flag;

	if(send(socket, &msg, sizeof(msg), 0) < 0) {
		perror("Error sending P_INVALID message to client.");
		exit(1);
	}
}

/*
*	Sends the "game over" message to the client specified by socket.
*	flag is the Q_ code corresponding to the game's result
*/
void send_game_over(int socket, char flag, char board[][3]) {
	char msg[11];
	msg[0] = P_GAMEOVER;
	msg[1] = flag;
	
	append_board(msg, 2, board);

	if(send(socket, &msg, sizeof(msg), 0) < 0) {
		perror("Error sending P_GAME_OVER message to client.");
		exit(1);
	}
}

/*
*	This will print its arguments only if the global variable for debugging is set.
*/
void dprintf(const char *fmt, ...) {
	if(debug > 0) {
		va_list args;
		va_start(args, fmt);
		vfprintf(stdout, fmt, args);
		va_end(args);
	}
}

//Graciously written by Dr. Bi
void reap_terminated_child(int status) {
   while (waitpid(-1, NULL, WNOHANG) > 0);
}

int get_server_socket(char *hostname, char *port) {
	struct addrinfo hints, *servinfo, *p;
	int status;
	int server_socket;
	int yes = 1;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if ((status = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) {
	   printf("getaddrinfo: %s\n", gai_strerror(status));
	   exit(1);
	}

	for (p = servinfo; p != NULL; p = p ->ai_next) {
	   // step 1: create a socket
	   if ((server_socket = socket(p->ai_family, p->ai_socktype,
						   p->ai_protocol)) == -1) {
		   printf("socket socket \n");
		   continue;
	   }
	   // if the port is not released yet, reuse it.
	   if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		 printf("socket option\n");
		 continue;
	   }

	   // step 2: bind socket to an IP addr and port
	   if (bind(server_socket, p->ai_addr, p->ai_addrlen) == -1) {
		   printf("socket bind \n");
		   continue;
	   }
	   break;
	}
	print_ip(servinfo);
	freeaddrinfo(servinfo);   // servinfo structure is no longer needed. free it.

	return server_socket;
}

int start_server(int serv_socket, int backlog) {
	int status = 0;
	if ((status = listen(serv_socket, backlog)) == -1) {
		printf("socket listen error: %s\n", strerror(errno));
	}
	return status;
}

int accept_client(int serv_sock) {
	int reply_sock_fd = -1;
	socklen_t sin_size = sizeof(struct sockaddr_storage);
	struct sockaddr_storage client_addr;
	char client_printable_addr[INET6_ADDRSTRLEN];

	// accept a connection request from a client
	// the returned file descriptor from accept will be used
	// to communicate with this client.
	if ((reply_sock_fd = accept(serv_sock, 
	   (struct sockaddr *)&client_addr, &sin_size)) == -1) {
			printf("socket accept error\n");
	}
	else {
		// here is only info only, not really needed.
		inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr *)&client_addr), 
						  client_printable_addr, sizeof client_printable_addr);
		printf("server: connection from %s at port %d\n", client_printable_addr,
							((struct sockaddr_in*)&client_addr)->sin_port);
	}
	return reply_sock_fd;
}

/* the following is a function designed for testing.
   it prints the ip address and port returned from
   getaddrinfo() function */
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

void *get_in_addr(struct sockaddr * sa) {
   if (sa->sa_family == AF_INET) {
	  printf("ipv4\n");
	  return &(((struct sockaddr_in *)sa)->sin_addr);
   }
   else {
	  printf("ipv6\n");
	  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
   }
}
//end of Dr. Bi's code
