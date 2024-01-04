#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <sys/types.h> 
#include <ncurses.h>

#define SERVER_IP "127.0.0.1"  //if the server and client are on the same machine, use the loopback address 127.0.0.1 or localhost. If the server is running on a different machine, you need to know the IP address of that machine.
#define PORT 8080

/* ALTERAR - O SERVER É QUE DEVE ENVIAR ESTES VALORES */
#define WINDOW_SIZE 20
#define PADLE_SIZE 2
/* -------------------------------------------------- */


/* ****************************************************************************************** STRUCTS ****************************************************************************************** */

typedef struct ball_position_t{
    int x, y;
    int up_hor_down; //  -1 up, 0 horizontal, 1 down
    int left_ver_right; //  -1 left, 0 vertical,1 right
    char c;
} ball_position_t;

typedef struct paddle_position_t{
    int x, y;
    int length;
} paddle_position_t;

/* ****************************************************************************************** GLOBAL VARIABLES ****************************************************************************************** */

WINDOW * message_win;


/* ****************************************************************************************** FUNCTIONS ****************************************************************************************** */

int request_connection(int sockfd, struct sockaddr_in serverAddr)
{
    char message[1024], rcv_msg[1024];
    ssize_t bytesSent, bytesRead;


    // Send a connection request to the server
    strcpy(message, "CONNECT");
    bytesSent = sendto(sockfd, message, strlen(message), 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if (bytesSent == -1) {
        perror("Error sending data");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    bytesRead = recvfrom(sockfd, rcv_msg, sizeof(rcv_msg), 0, NULL, NULL);
    if (bytesRead == -1) {
        perror("Error receiving data");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    rcv_msg[bytesRead] = '\0';

    // Check if the message is a connection request
    if (strcmp(rcv_msg, "CONNECTION_ACCEPTED") == 0) {
        return 1;
    } else {
        return 0;
    }

}

/*---------------------------------------------------------------------------------
    FUNCTION'S NAME:    initialize_board
    DESCRIPTION:        After connecting to server, initiates board .
    RETURN VALUES:      ---
---------------------------------------------------------------------------------*/
void initialize_board()
{
    initscr();		    	/* Start curses mode 		*/
	cbreak();				/* Line buffering disabled	*/
    keypad(stdscr, TRUE);   /* We get F1, F2 etc..		*/
	noecho();			    /* Don't echo() while we do getch */

    /* creates a window and draws a border */
    WINDOW * my_win = newwin(WINDOW_SIZE, WINDOW_SIZE, 0, 0);
    box(my_win, 0 , 0);	
	wrefresh(my_win);
    keypad(my_win, true);
    /* creates a window and draws a border */
    message_win = newwin(5, WINDOW_SIZE+10, WINDOW_SIZE, 0);
    box(message_win, 0 , 0);	
	wrefresh(message_win);



}
