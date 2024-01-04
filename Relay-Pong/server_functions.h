#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <sys/types.h> 
#include <sys/socket.h>
#include <pthread.h>
#include <ncurses.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8080 // has to be between 1024 and 49151

#define WINDOW_SIZE 20
#define PADDLE_SIZE 2


/* ****************************************************************************************** STRUCTS ****************************************************************************************** */

typedef struct ball_position_t{ // Stores ball position
    int x, y;
    int up_hor_down; //  -1 up, 0 horizontal, 1 down
    int left_ver_right; //  -1 left, 0 vertical,1 right
    char c;
} ball_position_t;

typedef struct paddle_position_t{ // Stores paddle position of one client
    int x, y;
    int length;
} paddle_position_t;

typedef struct ClientInfo { // Struct to store client information
    struct sockaddr_in addr;
    time_t startTime;
    time_t endTime;
    int isConnected;  // 1 if connected, 0 if not connected
    paddle_position_t paddle_pos;
    struct ClientInfo* next; // position of the client's paddle
}ClientInfo;

typedef struct ball_position_client{ // Stores ball position to send to clients
    int x, y;
    char c;
} ball_position_client;


typedef struct Board_Update{
    ball_position_client ball_pos;
    paddle_position_t client_paddle_pos;
    paddle_position_t other_paddle_pos;
}Board_Update;


/* ****************************************************************************************** GLOBAL VARIABLES ****************************************************************************************** */

ClientInfo* clientList = NULL;
pthread_rwlock_t clientListLock; //rwlock to synchronize access to list of groups
ball_position_t ball;



/* ****************************************************************************************** FUNCTIONS ****************************************************************************************** */

void addClient(struct sockaddr_in clientAddr){

    ClientInfo* newClient = (ClientInfo*)malloc(sizeof(ClientInfo));
    if (newClient == NULL) {
        perror("Error creating new client");
        exit(EXIT_FAILURE);
    }

    pthread_rwlock_wrlock(&clientListLock); // Lock for writing
    newClient->addr = clientAddr;
    newClient->startTime = time(NULL);
    newClient->endTime = 0;
    newClient->isConnected = 1;

    newClient->next = clientList;
    clientList = newClient;
    pthread_rwlock_unlock(&clientListLock); // Unlock after reading

}

int isClientInList(struct sockaddr_in addr) {

    pthread_rwlock_rdlock(&clientListLock); // Lock for reading
    ClientInfo* current = clientList;
    while (current != NULL) {
        if (memcmp(&(current->addr), &addr, sizeof(struct sockaddr_in)) == 0) {
            return 1;  // Client is already in the list
        }
        current = current->next;
    }
    pthread_rwlock_unlock(&clientListLock); // Unlock after reading

    return 0;  // Client is not in the list
}

void displayClients() {

    
    printf("\n-------------------- LIST OF CLIENTS ---------------------\n\n");

    pthread_rwlock_rdlock(&clientListLock); // Lock for reading
    ClientInfo* current = clientList;
    while (current != NULL) {
        printf("CLIENT %s:%d\n", inet_ntoa(current->addr.sin_addr), ntohs(current->addr.sin_port));
        if (current->isConnected) {
            printf("  . CONNECTED: \t\t%s", ctime(&(current->startTime)));
            printf("  . DISCONNECTED: \t-- \n");
            printf("  . DURATION: \t\t--\n\n");
        } else {
            time_t connectedTime = current->endTime - current->startTime;
            struct tm connectedTimeStruct;
            gmtime_r(&connectedTime, &connectedTimeStruct);

            printf("  . CONNECTED: \t\t%s", ctime(&(current->startTime)));
            printf("  . DISCONNECTED: \t%s", ctime(&(current->endTime)));
            printf("  . DURATION: \t\t%02dh %02dmin %02ds\n\n", connectedTimeStruct.tm_hour, connectedTimeStruct.tm_min, connectedTimeStruct.tm_sec);
        }

        current = current->next;
    }
    pthread_rwlock_unlock(&clientListLock); // Unlock after reading

    printf("---------------------- END OF LIST ----------------------\n");
    
    printf("\n");
}

// Function to disconnect a client
void disconnectClient(struct sockaddr_in clientAddr) {
    
    pthread_rwlock_wrlock(&clientListLock); // Lock for writing
    ClientInfo* current = clientList;
    ClientInfo* prev = NULL;

    while (current != NULL) {
        if (memcmp(&(current->addr), &clientAddr, sizeof(struct sockaddr_in)) == 0) {
            // Client found, update disconnect time 
            current->isConnected = 0;
            current->endTime = time(NULL);
            break;
        }

        prev = current;
        current = current->next;
    }
    pthread_rwlock_unlock(&clientListLock); // Unlock after writing

}


/*---------------------------------------------------------------------------------
    FUNCTION'S NAME:    create_malloc
    DESCRIPTION:        Allocates memory position and confirms success.
    ARGUMENTS:          size_t sz - size of memory position to be allocated
    RETURN VALUES:      pointer to allocated memory position, NULL if memory is full
---------------------------------------------------------------------------------*/
void *create_malloc (size_t sz){
    void *mem = malloc (sz);

    if (mem == NULL){ //if memory is full
        perror("ERROR in Malloc\n");
        exit(0); 
    }
    return mem;
}


/*---------------------------------------------------------------------------------
    FUNCTION'S NAME:    read_line
    DESCRIPTION:        Reads string from terminal (stdin) and stores it in allocated
                        memory. The resulting string has a number of characters 
                        specified by "size" plus the '/0'. '\n' is not included.
    ARGUMENTS:          int size - number of characters to be read from terminal
    RETURN VALUES:      pointer to string read from terminal
---------------------------------------------------------------------------------*/
char * read_line(int size){
    char *line = (char *)create_malloc (size+1); //+1 because of '\0'
    int i; //position in str read from terminal
    
    do{
        fgets(line, size+1, stdin); //+1 accounts for the '\0' char
        i = strlen(line)-1;
    
        if(line[i] != '\n') // if EOL was not read
            while(getchar()!='\n');  //clear terminal until EOL character is reached

        if(line[i]=='\n') //take '\n' out of string
            line[i]='\0';
    
        if (strcmp(line,"")==0) //ask for input until something is entered
            printf("\nINVALID INPUT. ENTER AGAIN\n\t> ");
    }while(strcmp(line,"")==0);

    return line;
}



void clean_list(){

    ClientInfo* aux = clientList;

    pthread_rwlock_wrlock(&clientListLock); // Lock for writing
    while (aux != NULL){
        clientList = clientList->next;

        /* Enter here code to close SOCKSTREAM sockets
        
        */

       free(aux);
        aux = clientList;
    }
    pthread_rwlock_unlock(&clientListLock); // Unlock after writing


}


void handleConnectionRequest(int sockfd, struct sockaddr_in clientAddr) {
     char response[40];

    // Check if the client is already in the list
    if (isClientInList(clientAddr)!= 1) {
        
        addClient(clientAddr);
        
        // Send a connection confirmation message to the client
        strcpy(response, "CONNECTION_ACCEPTED");

    } else {
        // Send a connection refused message to the client
        strcpy(response, "ALREADY_CONNECTED");
    }

    sendto(sockfd, response, strlen(response), 0, (struct sockaddr*)&clientAddr, sizeof(clientAddr));
}


/*---------------------------------------------------------------------------------
    FUNCTION'S NAME:    verify_message
    DESCRIPTION:        Verifies client message and returns flag accordingly.
    RETURN VALUES:      1 if connected successfully
                        2 if diconnected successfully
                        3 if is connected and is other message
                        -1 if connection denied 
                        0 if already connected
---------------------------------------------------------------------------------*/
int verify_message(int sockfd, struct sockaddr_in clientAddr, char *rcv_msg){
            
        if (strcmp(rcv_msg, "CONNECT") == 0) {
            handleConnectionRequest(sockfd, clientAddr);
            return 1;
        }
        else if (isClientInList(clientAddr) == 1) { // Only continues if client is authenticated
            // Check if the received message is 'q' or 'Q'
            if (strcasecmp(rcv_msg, "Q") == 0) {
                disconnectClient(clientAddr); // Disconnect the client if 'q' is received
                return 2;
            }
            return 3;
        }
        else { // If it is not a connection request and the client is not in the list of clients, refuse the connection
            sendto(sockfd, "CONNECTION_DENIED", strlen("CONNECTION_DENIED"), 0, (struct sockaddr*)&clientAddr, sizeof(clientAddr));
            return -1;
        }


}


/*---------------------------------------------------------------------------------
    FUNCTION'S NAME:    place_ball_random
    DESCRIPTION:        Initializes ball position randomly.
    RETURN VALUES:      ---
---------------------------------------------------------------------------------*/
void place_ball_random(){
    ball.x = rand() % WINDOW_SIZE ;
    ball.y = rand() % WINDOW_SIZE ;
    ball.c = 'o';
    ball.up_hor_down = rand() % 3 -1; //  -1 up, 1 - down
    ball.left_ver_right = rand() % 3 -1 ; // 0 vertical, -1 left, 1 right
}



void initialize_client_paddle(ClientInfo *current_client){

    // New clients are always on the top of the list, so initialize their positions
    current_client->paddle_pos.x = WINDOW_SIZE/2;
    current_client->paddle_pos.y = WINDOW_SIZE-2;
    current_client->paddle_pos.length = PADDLE_SIZE;

}



/*---------------------------------------------------------------------------------
    FUNCTION'S NAME:    initialize_client_paddle
    DESCRIPTION:        Function to search for a client in the list based on the client address
    RETURN VALUES:      Pointer to that client in the client list
                        Returns Null if client has not been found
---------------------------------------------------------------------------------*/
ClientInfo* search_for_client(struct sockaddr_in addr) {
    ClientInfo* current = clientList;

    while (current != NULL) {
        // Compare the client addresses
        if (memcmp(&(current->addr), &addr, sizeof(struct sockaddr_in)) == 0) {
            return current;  // Found the client
        }
        current = current->next;
    }

    return NULL;  // Client not found
}


/*---------------------------------------------------------------------------------
    FUNCTION'S NAME:    send_board_update
    DESCRIPTION:        Function that sends the board update for a specific client
    RETURN VALUES:      ---
---------------------------------------------------------------------------------*/
void send_board_update(int sockfd, ClientInfo *current_client){
    Board_Update board_update;

    // Update ball position
    board_update.ball_pos.c = ball.c;
    board_update.ball_pos.x = ball.x;
    board_update.ball_pos.x = ball.y;

    // Update client position 
    board_update.client_paddle_pos = current_client->paddle_pos;


}



/* ****************************************************************************************** THREADS ****************************************************************************************** */

// Thread function to handle user input
void* userInputThread(void* arg) {
    int option, n_keys, terminate = 0;
    char *line;

    while (terminate == 0){
        printf("\n< PRESS ENTER TO GO TO MENU >");
        getchar();
        system("clear");
        printf("\n\n************************* MENU **************************\n");
        printf("\nCHOOSE AN ACTION:\n\n\t1 - SHOW CLIENT LIST\n\t2 - EXIT\n\n(WAITING FOR CHOSEN OPTION FROM KEYBOARD)\n");
        printf("\n*********************************************************\n");
        do{  //asks for input until valid input is given
            line = read_line(2);    //read at least 2 characters to confirm input does not have 2 digits
            option = atoi(line); // convert to int
            free(line);
            if (option < 1 || option > 2)
                printf("\nINVALID INPUT. ENTER AGAIN\n\t> ");            
        }while(option < 1 || option > 2);

        switch (option){
            case 1:
                displayClients();
                break;
            case 2:
                terminate = 1;
                break;
        }
    }

    clean_list(); //clear memory before killing the process
    printf("\nSERVER WILL EXIT.\n");
    exit(0); //This causes the termination of all threads in the process.

}