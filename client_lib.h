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
#include <pthread.h>
#include <signal.h>
#include <semaphore.h>

#define SERVER_IP "127.0.0.1"  //if the server and client are on the same machine, use the loopback address 127.0.0.1 or localhost. If the server is running on a different machine, you need to know the IP address of that machine.
#define PORT 8080

/* ALTERAR - O SERVER É QUE DEVE ENVIAR ESTES VALORES */
#define WINDOW_SIZE 20
#define PADLE_SIZE 2
/* -------------------------------------------------- */


/* ****************************************************************************************** STRUCTS ****************************************************************************************** */

typedef struct ball_position_t{
    int x, y;
    //int up_hor_down; //  -1 up, 0 horizontal, 1 down
    //int left_ver_right; //  -1 left, 0 vertical,1 right
    char c;
} ball_position_t;

typedef struct paddle_position_t{
    int x, y;
    int length;
} paddle_position_t;

typedef struct Board_State{
    ball_position_t ball_pos;
    paddle_position_t client_paddle_pos;
    int nmr_other_clients;
    paddle_position_t * other_paddle_pos; // -1 because the paddle position of the client to send the message is stored in "client_paddle_pos"
}Board_State;

/* ****************************************************************************************** GLOBAL VARIABLES ****************************************************************************************** */

WINDOW * my_win; // Window
int window_semaphore; // This is used to simulate a semaphore - is used to only allow the client to receive user input after the gaming window being created
pthread_mutex_t window_semaphore_mux; // mutex to prevent racing conditions to the simulated semaphore window_semaphore

WINDOW * message_win; // Message window
int sockfd; // socket to communicate with server

/* ****************************************************************************************** FUNCTIONS ****************************************************************************************** */

/*---------------------------------------------------------------------------------
    FUNCTION'S NAME:    create_malloc
    DESCRIPTION:        Allocates memory position and confirms success.
    ARGUMENTS:          size_t sz - size of memory position to be allocated
    RETURN VALUES:      pointer to allocated memory position, NULL if memory is full
---------------------------------------------------------------------------------*/
void *create_malloc (size_t sz){
    void *mem = malloc (sz);

    if (mem == NULL){ //if memory is full
        perror("\rERROR in Malloc\n");
        exit(0); 
    }
    return mem;
}


/*---------------------------------------------------------------------------------
    FUNCTION'S NAME:    read_message
    DESCRIPTION:        Reads messages from socket, character by character until '\0'
                        is reached.
    ARGUMENTS:          int socket - file descriptor of socket from which to read
    RETURN VALUES:      address of memory position with first character of msg read 
---------------------------------------------------------------------------------*/
char *read_message(int socket){
    char *line =(char *)create_malloc(sizeof(char)); //+1 because of '\0'
    char c; //char read from socket
    int i = 0; //position of char in msg

    do{ //read chars one by one from socket
        if (read(socket, &c, 1) < 1){
            free(line);
            perror("\r\nERROR Reading from server ");
            return NULL;
        } 
        line[i] = c;
        if(c != '\0'){  //if "\0" not reached, add char to the string
            i++;
            line = realloc(line, i+1); //resize the memory block pointed to by line
            line[i]='\0';   //add new "\0" to the str
        }   
    } while (c!='\0'); //if "\0" is reached break the loop

    return line;
}


/*---------------------------------------------------------------------------------
    FUNCTION'S NAME:    confirm_connection
    DESCRIPTION:        After initiating the connection on a socket, verify if server 
                        accepted connection.
    RETURN VALUES:      1 if connection accepted
                        -1 if connection denied
---------------------------------------------------------------------------------*/
int confirm_connection(int sockfd, struct sockaddr_in serverAddr)
{
    int flag_connection;

    if(read(sockfd, &flag_connection, sizeof(int))<1){ //receive status from server (access granted or denied)
        perror("\r\nERROR Reading connectionn confirmation ");
        return -5;
    }

    // Check if connection was accepted or not
    if (flag_connection == 1) {
        return 1;
    } else {
        return -1;
    }

}


/*---------------------------------------------------------------------------------
    FUNCTION'S NAME:    handle_sudden_disconnection
    DESCRIPTION:        When user clicks CTRL+C, this function is called and a quit message 
                        is sent to the server.
    RETURN VALUES:      ---
---------------------------------------------------------------------------------*/
void handle_sudden_disconnection(int signum){
    int msg_quit = 81; // represents the char Q

    write(sockfd, &msg_quit, sizeof(int)); // it does not matter if the write gives an error

    //sem_destroy(&window_semaphore);

    close(sockfd);
    exit(0);
}

/*---------------------------------------------------------------------------------
    FUNCTION'S NAME:    initialize_window
    DESCRIPTION:        After connecting to server, initiates board.
    RETURN VALUES:      ----
---------------------------------------------------------------------------------*/
void initialize_window()
{
    initscr();
	cbreak();				/* Line buffering disabled	*/
    keypad(stdscr, TRUE);   /* We get F1, F2 etc..		*/
	noecho();			    /* Don't echo() while we do getch */

    /* creates a window and draws a border */
    my_win = newwin(WINDOW_SIZE, WINDOW_SIZE, 0, 0);
    box(my_win, 0 , 0);	
	wrefresh(my_win);
    keypad(my_win, true);

    /* creates a window and draws a border */
    message_win = newwin(5, WINDOW_SIZE+10, WINDOW_SIZE, 0);
    box(message_win, 0 , 0);	
	wrefresh(message_win);

    //return my_win;

}



/*---------------------------------------------------------------------------------
    FUNCTION'S NAME:    receive_board_update
    DESCRIPTION:        Function that receives board updates from server. The messages
                        received follow the following order:
                            1) struct with ball position
                            2) struct with the current client's position
                            3) integer with the number of active clients
                            4) cycle that sends the structs containing the other clients' positions
    RETURN VALUES:      1 if success
                        -1 if unexpected error 
---------------------------------------------------------------------------------*/
int receive_board_update(Board_State *boardState){

    // Read Ball position
    if(read(sockfd, &boardState->ball_pos, sizeof(ball_position_t))<sizeof(ball_position_t)){ 
        perror("\r\nERROR Reading ball position ");
        return -1;
    }

    // Read client's paddle position
    if(read(sockfd, &boardState->client_paddle_pos, sizeof(paddle_position_t))<sizeof(paddle_position_t)){ 
        perror("\r\nERROR Reading paddle position from server ");
        return -1;
    }

    // Read number of active clients
    if(read(sockfd, &boardState->nmr_other_clients, sizeof(int))<sizeof(int)){ 
        perror("\r\nERROR Reading number of active players ");
        return -1;
    }

    //create dynamic array to save other client's positions
    boardState->other_paddle_pos = (paddle_position_t *)malloc(boardState->nmr_other_clients * sizeof(paddle_position_t));

    for(int client_idx = 0; client_idx<boardState->nmr_other_clients; client_idx++){ 
        // Read other client's paddle position
        if(read(sockfd, &boardState->other_paddle_pos[client_idx], sizeof(paddle_position_t))<sizeof(paddle_position_t)){ 
            perror("\r\nERROR Reading other paddles ");
            free(boardState->other_paddle_pos);
            return -1;
        }
    }

    return 1;
}



/*---------------------------------------------------------------------------------
    FUNCTION'S NAME:    draw_paddle
    DESCRIPTION:        Draws paddles. It uses "=" to draw the client's paddle and it uses
                        '-' to draw the other clients' paddles
    RETURN VALUES:      ----
---------------------------------------------------------------------------------*/
void draw_paddles(paddle_position_t client_pos, paddle_position_t *other_clients_pos, int nmr_other_clients ,int delete){
    int ch_client, ch_other_clients;
    int start_x, end_x;
    
    if(delete){
        ch_client = '=';
        ch_other_clients = '_';
    }else{
        ch_client = ' ';
        ch_other_clients = ' ';
    }

    // draw current client's paddle
    start_x = client_pos.x - client_pos.length;
    end_x = client_pos.x + client_pos.length;
    for (int x = start_x; x <= end_x; x++){
        wmove(my_win, client_pos.y, x);
        waddch(my_win, ch_client);
    }

    // draw other clients' paddles
    for (int idx = 0; idx<nmr_other_clients; idx++){
        start_x = other_clients_pos[idx].x - other_clients_pos[idx].length;
        end_x = other_clients_pos[idx].x + other_clients_pos[idx].length;
        for (int x = start_x; x <= end_x; x++){
            wmove(my_win, other_clients_pos[idx].y, x);
            waddch(my_win, ch_other_clients);
        }

    }

    wrefresh(my_win);
}


/*---------------------------------------------------------------------------------
    FUNCTION'S NAME:    draw_ball
    DESCRIPTION:        Draws ball. The ball position is received from the server in receive_board_update
    RETURN VALUES:      ----
---------------------------------------------------------------------------------*/
void draw_ball(ball_position_t ball, int draw){
    int ch;
    if(draw){
        ch = ball.c;
    }else{
        ch = ' ';
    }
    wmove(my_win, ball.y, ball.x);
    waddch(my_win,ch);
    wrefresh(my_win);
}

/*---------------------------------------------------------------------------------
    FUNCTION'S NAME:    print_keys_message
    DESCRIPTION:        Prints key pressed by client.
    RETURN VALUES:      ----
---------------------------------------------------------------------------------*/
void print_keys_message(int key){
    // Wait for user input
    switch (key) {
        case KEY_LEFT:
            mvwprintw(message_win, 1,1,"LEFT ARROW key pressed");
            break;
        case KEY_RIGHT:
            mvwprintw(message_win, 1,1,"RIGHT ARROW key pressed");
            break;
        case KEY_DOWN:
            mvwprintw(message_win, 1,1,"DOWN ARROW key pressed");
            break;
        case KEY_UP:
            mvwprintw(message_win, 1,1,"UP ARROW key pressed");
            break;
        default:
            printw("Unknown key: %d", key);
            break;
        }
}