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
#include <signal.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8080 // has to be between 1024 and 49151

#define WINDOW_SIZE 20
#define PADDLE_SIZE 2
#define MAX_CLIENTS 5


/* ****************************************************************************************** STRUCTS ****************************************************************************************** */

typedef struct Client_Info_to_Thread{ // structure to pass de client information into the client thread
    int client_sock;
    struct sockaddr_in client_addr;
}Client_Info_to_Thread;

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
    int sock_client;
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


typedef struct Board_Update_msg{
    ball_position_client ball_pos;
    paddle_position_t client_paddle_pos;
    paddle_position_t other_paddle_pos[MAX_CLIENTS-1]; // -1 because the paddle position of the client to send the message is stored in "client_paddle_pos"
}Board_Update_msg;


/* ****************************************************************************************** GLOBAL VARIABLES ****************************************************************************************** */

ClientInfo* clientList = NULL;
pthread_rwlock_t clientListLock; //rwlock to synchronize access to list of groups

ClientInfo* disconnected_ClientList = NULL;
pthread_rwlock_t disconnected_ClientListLock; //rwlock to synchronize access to list of disconnected

// int activePlayers = 0;
// pthread_mutex_t activePlayersLock = PTHREAD_MUTEX_INITIALIZER; //mutex to lock variable activePlayers

ball_position_t ball; // ball position 
pthread_rwlock_t ballLock; //rwlock to synchronize access to ball

pthread_mutex_t board_updates_mux; // mutex to prevent racing conditions in sending messages to clients

int server_socket;

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
        perror("ERROR in Malloc\n");
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
            perror("\nERROR Reading client message ");
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
    FUNCTION'S NAME:    wait_for_available_thread
    DESCRIPTION:        Function that, before accepting a new client, verifies if there is
                        any available thread
    RETURN VALUES:      index of the available thread_id
---------------------------------------------------------------------------------*/
int wait_for_available_thread(){
    int i=0;

    // do{ 
    //     for(i = 0; i<MAX_CLIENTS; i++){
    //         if(t_id[i] != -1){
    //             if(pthread_tryjoin_np(t_id[i], NULL) == 0){ // confirms client thread is still alive
    //                 t_id[i] = -1; //if client thread exited make thread id available for another client to connect
    //                 break;
    //             }
    //         }else{
    //             break;
    //         }
    //     }
    //     sleep(1);
    // }while(i == MAX_CLIENTS);
    
    return i;
}


/*---------------------------------------------------------------------------------
    FUNCTION'S NAME:    verify_paddle_position
    DESCRIPTION:        Verifies if paddle position is already occupied.
    SYNC NOTES:         There is a lock where this function is called (addClient->initialize_client_paddle and moove_paddle)
    RETURN VALUES:      1 if it is not occupied
                        -1 if it is occupied
---------------------------------------------------------------------------------*/
int verify_paddle_position(paddle_position_t proposed_pos, ClientInfo * requesting_client) {
    int client_paddle_start_x, client_paddle_end_x;
    int prop_paddle_start_x, prop_paddle_end_x;

    prop_paddle_start_x = proposed_pos.x - proposed_pos.length;
    prop_paddle_end_x = proposed_pos.x + proposed_pos.length;

    ClientInfo *current_client = clientList;

    for( ;current_client != NULL; current_client = current_client->next){
        
        if(current_client == requesting_client) continue;
        
        if(current_client->paddle_pos.y != proposed_pos.y) continue;

        client_paddle_start_x = current_client->paddle_pos.x - current_client->paddle_pos.length;
        client_paddle_end_x = current_client->paddle_pos.x + current_client->paddle_pos.length;

        if( client_paddle_start_x <= prop_paddle_start_x && prop_paddle_start_x <= client_paddle_end_x){ 
            return -1;
        }

        if( client_paddle_start_x <= prop_paddle_end_x && prop_paddle_end_x <= client_paddle_end_x){ 
            return -1;
        }

        // if(current_client->paddle_pos.y == proposed_pos.y && current_client->paddle_pos.x == proposed_pos.x){ // Assuming that all paddles have the same length
        //     return -1;
        // }
        
    }

    return 1; // Position is not occupied
}

/*---------------------------------------------------------------------------------
    FUNCTION'S NAME:    initialize_client_paddle
    DESCRIPTION:        Initializes client paddle randomly.
    SYNC NOTES:         There is no need to lock client list, because, when this function is called, 
                        the new client is still not on the list 
    RETURN VALUES:      ---
---------------------------------------------------------------------------------*/
void initialize_client_paddle(ClientInfo *new_client){

    new_client->paddle_pos.length = PADDLE_SIZE;
    
    do {
        new_client->paddle_pos.x = rand() % (WINDOW_SIZE - 1);
        new_client->paddle_pos.y = rand() % (WINDOW_SIZE - 1);
    } while (verify_paddle_position( new_client->paddle_pos, new_client) == -1); // While proposed position is already occupied


}


/*---------------------------------------------------------------------------------
    FUNCTION'S NAME:    count_active_clients
    DESCRIPTION:        Counts number of active clients.
    SYNC NOTES:         ---
    RETURN VALUES:      ---
---------------------------------------------------------------------------------*/
int count_active_clients() {
    int count = 0;

    pthread_rwlock_rdlock(&clientListLock); // Lock for reading
    ClientInfo* current = clientList;
    while (current != NULL) {
        count++;
        current = current->next;
    }
    pthread_rwlock_unlock(&clientListLock); // Unlock after reading

    return count;  
}


/*---------------------------------------------------------------------------------
    FUNCTION'S NAME:    send_board_update
    DESCRIPTION:        sends board update to a specific client. For each client, the messages 
                        follow the following structure:
                            1) struct with ball position
                            2) struct with the current client's position
                            3) integer with the number of active clients
                            4) cycle that sends the structs containing the other clients' positions
    SYNC NOTES:         It is necessary to lock client list OUTSIDE this function 
                        the new client is still not on the list or is called inside a lock (broadcast_board_update)
    RETURN VALUES:      ---
---------------------------------------------------------------------------------*/
void send_board_update(ClientInfo* client){

    Board_Update_msg board_msg;

    // Fill ball position
    pthread_rwlock_rdlock(&ballLock); // Lock ball for reading
    board_msg.ball_pos.x = ball.x;
    board_msg.ball_pos.y = ball.y;
    board_msg.ball_pos.c = ball.c;
    pthread_rwlock_unlock(&ballLock); // Lock ball for reading

    // THERE IS NO NEED TO LOCK CLIENT LIST, BECAUSE WHEN THIS FUNCTION IS CALLED, THE NEW CLIENT IS STILL NOT ON THE LIST
    //      OR THIS FUNCTIONN IS CALLED INSIDE A LOCK
    
    // Sends the ball position 
    if(write(client->sock_client, &board_msg.ball_pos , sizeof(ball_position_client)) < sizeof(ball_position_client)){ 
        perror("\nERROR Writing to client (Game Thread) ");
    }

    // Fill current client's paddle
    board_msg.client_paddle_pos = client->paddle_pos;

    // Sends current client's position
    if(write(client->sock_client, &board_msg.client_paddle_pos , sizeof(paddle_position_t)) < sizeof(paddle_position_t)){ 
        perror("\nERROR Writing to client (Game Thread) ");
    }
    
    // Sends number of active players
    // pthread_mutex_lock(&activePlayersLock);
    int otherPlayers = count_active_clients() -1; // We must subtract one beacause we only want to send the number of other active players, not counting with the current client

    if(write(client->sock_client, &otherPlayers , sizeof(int)) < sizeof(int)){ 
        perror("\nERROR Writing to client (Game Thread) ");
    }
    // pthread_mutex_unlock(&activePlayersLock);

    // Fill other paddle positions
    if (otherPlayers != 0){
        ClientInfo *otherClient = clientList;
        for( ;otherClient != NULL; otherClient = otherClient->next){

            if(otherClient != client && otherClient->isConnected == 1){ // if other client is not the client that we will send the update and if it is active
                // Sends other client's position
                if(write(client->sock_client, &otherClient->paddle_pos , sizeof(paddle_position_t)) < sizeof(paddle_position_t)){ 
                    perror("\nERROR Writing to client (Game Thread) ");
                }
            }

        }
    }

}


/*---------------------------------------------------------------------------------
    FUNCTION'S NAME:    addClient
    DESCRIPTION:        Adds client on the top of the list of clients
    RETURN VALUES:      Pointer to the struct of the new client, in the client List
---------------------------------------------------------------------------------*/
ClientInfo* addClient(int sockfd ,struct sockaddr_in clientAddr){

    ClientInfo* newClient = (ClientInfo*)malloc(sizeof(ClientInfo));
    if (newClient == NULL) {
        perror("Error creating new client");
        exit(EXIT_FAILURE);
    }

    pthread_rwlock_rdlock(&clientListLock); // Lock for reading
    initialize_client_paddle(newClient); // initializes client paddle
    pthread_rwlock_unlock(&clientListLock); // Lock for reading
        
    newClient->sock_client = sockfd;
    newClient->addr = clientAddr;
    newClient->startTime = time(NULL);
    newClient->endTime = 0;
    newClient->isConnected = 1;

    pthread_rwlock_wrlock(&clientListLock); // Lock for writing
    newClient->next = clientList;
    clientList = newClient;
    pthread_rwlock_unlock(&clientListLock); // Unlock after reading

    pthread_rwlock_rdlock(&clientListLock);
    send_board_update(newClient); // After accepting the client and initializing its paddle, send him the board info
    pthread_rwlock_unlock(&clientListLock);

    // pthread_mutex_lock(&activePlayersLock);
    // activePlayers = activePlayers + 1;
    // pthread_mutex_unlock(&activePlayersLock);
    
    return newClient;
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

    int activePlayers = count_active_clients();
    printf("\n---------- LIST OF CLIENTS (%d CLIENT(S) ACTIVE) ---------- \n\n", activePlayers);

    // PRINT LIST OF ACTIVE CLIENTS
    pthread_rwlock_rdlock(&clientListLock); // Lock for reading
    ClientInfo* current = clientList;
    while (current != NULL) {
        printf("CLIENT %s:%d\n", inet_ntoa(current->addr.sin_addr), ntohs(current->addr.sin_port));
        
        printf("  . CONNECTED: \t\t%s", ctime(&(current->startTime)));
        printf("  . DISCONNECTED: \t-- \n");
        printf("  . DURATION: \t\t--\n\n");
         
        current = current->next;
    }
    pthread_rwlock_unlock(&clientListLock); // Unlock after reading

    // PRINT LIST OF DISCONNECTED CLIENTS
    pthread_rwlock_rdlock(&disconnected_ClientListLock); // Lock for reading
    current = disconnected_ClientList;
    while (current != NULL) {
        printf("CLIENT %s:%d\n", inet_ntoa(current->addr.sin_addr), ntohs(current->addr.sin_port));

        time_t connectedTime = current->endTime - current->startTime;
        struct tm connectedTimeStruct;
        gmtime_r(&connectedTime, &connectedTimeStruct);

        printf("  . CONNECTED: \t\t%s", ctime(&(current->startTime)));
        printf("  . DISCONNECTED: \t%s", ctime(&(current->endTime)));
        printf("  . DURATION: \t\t%02dh %02dmin %02ds\n\n", connectedTimeStruct.tm_hour, connectedTimeStruct.tm_min, connectedTimeStruct.tm_sec);


        current = current->next;
    }
    pthread_rwlock_unlock(&disconnected_ClientListLock); // Unlock after reading

    printf("--------------- END OF LIST ---------------\n");
    
    printf("\n");
}

/*---------------------------------------------------------------------------------
    FUNCTION'S NAME:    disconnectClient
    DESCRIPTION:        Function that removes client from client List
    RETURN VALUES:      ---
---------------------------------------------------------------------------------*/
void disconnectClient(struct sockaddr_in clientAddr) {

    pthread_rwlock_wrlock(&clientListLock); // Lock for writing
    ClientInfo* current = clientList;
    ClientInfo* prev = NULL;

    while (current != NULL) {
        if (memcmp(&(current->addr), &clientAddr, sizeof(struct sockaddr_in)) == 0) {
            // Client found, update disconnect time 
            current->isConnected = 0;
            current->endTime = time(NULL);

            if (current == clientList) {
                clientList = clientList->next;
            }
            else{
                prev->next = current->next;
            }

            // Add to disconnected list of clients
            pthread_rwlock_wrlock(&disconnected_ClientListLock);
            current->next = disconnected_ClientList;
            disconnected_ClientList = current;
            pthread_rwlock_unlock(&disconnected_ClientListLock);

            break;
        }

        prev = current;
        current = current->next;
    }
    pthread_rwlock_unlock(&clientListLock); // Unlock after writing

    // pthread_mutex_lock(&activePlayersLock);
    // activePlayers = activePlayers - 1;
    // pthread_mutex_unlock(&activePlayersLock);

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


/*---------------------------------------------------------------------------------
    FUNCTION'S NAME:    clean_list
    DESCRIPTION:        When server exits, this function cleans clients lists (cleans active 
                        and disconnected clients).
    ARGUMENTS:          int size - number of characters to be read from terminal
    RETURN VALUES:      pointer to string read from terminal
---------------------------------------------------------------------------------*/
void clean_list(){

    pthread_rwlock_wrlock(&clientListLock); // Lock for reading
    ClientInfo* aux = clientList;

    while (aux != NULL){
        clientList = clientList->next;

        close(aux->sock_client);
        free(aux);

        aux = clientList;
    }
    pthread_rwlock_unlock(&clientListLock); // Unlock after writing

    pthread_rwlock_wrlock(&disconnected_ClientListLock); // Lock for reading
    aux = disconnected_ClientList;

    while (aux != NULL){
        disconnected_ClientList = disconnected_ClientList->next;

        free(aux);

        aux = disconnected_ClientList;
    }
    pthread_rwlock_unlock(&disconnected_ClientListLock); // Unlock after writing

}


/*---------------------------------------------------------------------------------
    FUNCTION'S NAME:    handle_sudden_server_disconnection
    DESCRIPTION:        When user clicks CTRL+C, this function is called and a quit message 
                        is sent to the server.
    RETURN VALUES:      ---
---------------------------------------------------------------------------------*/
void handle_sudden_server_disconnection(int signum){

    clean_list(); //clear memory before killing the process

    close(server_socket);

    exit(0);
}



/*---------------------------------------------------------------------------------
    FUNCTION'S NAME:    handleConnectionRequest
    DESCRIPTION:        Verifies if client is already connected. Verifies if can Accept connection
    FLAGS TO CLIENT:    1 if connection accepted
                        -1 if connection not accepted
    RETURN VALUES:      1 if connection can be accepted
                        -1 if connection can not accepted
                        -2 if error in writing to client
---------------------------------------------------------------------------------*/
int handleConnectionRequest(int sockfd, struct sockaddr_in clientAddr) {
     char response[40];
     int flag_connection;

    // Check if the client is already in the list
    if (isClientInList(clientAddr)!= 1) {
        
        // Send a connection confirmation message to the client
        flag_connection = 1;
        if(write(sockfd, &flag_connection , sizeof(int)) < 1){ 
            perror("\nERROR Writing to client ");
            return -2;
        }

        return 1;

    } else {

        // Send a connection confirmation message to the client
        flag_connection = -1;
        if(write(sockfd, &flag_connection , sizeof(int)) < 1){ 
            perror("\nERROR Writing to client ");
            return -2;
        }

        return -1;
    }

}


/*---------------------------------------------------------------------------------
    FUNCTION'S NAME:    initialize_client_paddle
    DESCRIPTION:        Function to search for a client in the list based on the client address
    RETURN VALUES:      Pointer to that client in the client list
                        Returns Null if client has not been found
---------------------------------------------------------------------------------*/
ClientInfo* search_for_client(struct sockaddr_in addr) {

    pthread_rwlock_rdlock(&clientListLock); // Lock for reading
    ClientInfo* current = clientList;

    while (current != NULL) {
        // Compare the client addresses
        if (memcmp(&(current->addr), &addr, sizeof(struct sockaddr_in)) == 0) {
            return current;  // Found the client
        }
        current = current->next;
    }
    pthread_rwlock_unlock(&clientListLock); // Lock for reading

    return NULL;  // Client not found
}



/*---------------------------------------------------------------------------------
    FUNCTION'S NAME:    place_ball_random
    DESCRIPTION:        Initializes ball position randomly.
    RETURN VALUES:      ---
---------------------------------------------------------------------------------*/
void place_ball_random(){
    pthread_rwlock_wrlock(&ballLock); // Lock for writing
    ball.x = rand() % WINDOW_SIZE ;
    ball.y = rand() % WINDOW_SIZE ;
    ball.c = 'o';
    ball.up_hor_down = rand() % 3 -1; //  -1 up, 1 - down
    ball.left_ver_right = rand() % 3 -1 ; // 0 vertical, -1 left, 1 right
    pthread_rwlock_unlock(&ballLock); // Unlock ball
}



/*---------------------------------------------------------------------------------
    FUNCTION'S NAME:    verify_paddle_collision
    DESCRIPTION:        verifies if ball is colliding with any of the paddles
    RETURN VALUES:      .1 if is colliding
                        .0 if is colliding
---------------------------------------------------------------------------------*/
int verify_paddle_collision(){

    int paddle_start_x, paddle_end_x;

    pthread_rwlock_rdlock(&clientListLock); // Lock for reading
    ClientInfo* current_client = clientList;
    
    while(current_client != NULL){

        paddle_start_x = current_client->paddle_pos.x - current_client->paddle_pos.length;
        paddle_end_x = current_client->paddle_pos.x + current_client->paddle_pos.length;

        // NO NEED TO LOCK BALL POSITION BECAUSE IT IS LOCKED WHERE THIS FUNCTION IS CALLED
        if( paddle_start_x <= ball.x && ball.x <= paddle_end_x && ball.y == current_client->paddle_pos.y ){
            return 1;
        }

        current_client = current_client->next;
    }
    pthread_rwlock_unlock(&clientListLock); 

    return 0;
}


/*---------------------------------------------------------------------------------
    FUNCTION'S NAME:    moove_ball
    DESCRIPTION:        Computes ball position
    RETURN VALUES:      ---
---------------------------------------------------------------------------------*/
void moove_ball(){

    pthread_rwlock_wrlock(&ballLock); // Lock ball for writing
    int next_x = ball.x + ball.left_ver_right;
    if( next_x == 0 || next_x == WINDOW_SIZE-1){
        ball.left_ver_right *= -1;
        ball.x += ball.left_ver_right;
     }else{
        ball.x = next_x;
    }

    int paddle_collision = 0;
    paddle_collision = verify_paddle_collision();

    int next_y = ball.y + ball.up_hor_down;
    if( next_y == 0 || next_y == WINDOW_SIZE-1 || paddle_collision == 1 ){
        ball.up_hor_down *= -1;
        ball.y += ball.up_hor_down;
    }else{
        ball . y = next_y;
    }
    pthread_rwlock_unlock(&ballLock); // Lock ball for writing

}




/*---------------------------------------------------------------------------------
    FUNCTION'S NAME:    broadcast_board_update
    DESCRIPTION:        sends board update to all active clients
    RETURN VALUES:      ---
---------------------------------------------------------------------------------*/
void broadcast_board_update(){
    
    pthread_rwlock_rdlock(&clientListLock); // Lock for reading
    ClientInfo* current_client = clientList;

    for( ;current_client != NULL; current_client = current_client->next){
        send_board_update(current_client);
    }
    pthread_rwlock_unlock(&clientListLock); 

}

/*---------------------------------------------------------------------------------
    FUNCTION'S NAME:    moove_paddle
    DESCRIPTION:        Function that receives the paddle movement of a specific client
                        and updates the paddle position of that client
    RETURN VALUES:      ---
---------------------------------------------------------------------------------*/
void moove_paddle (ClientInfo* client, int direction){

    paddle_position_t proposed_paddle_position = client->paddle_pos; // used to verify if the movement can be performed

    // pthread_rwlock_wrlock(&clientListLock); 
    // if (direction == KEY_UP){
    //     if (client->paddle_pos.y  != 1){
    //         proposed_paddle_position.y --;
    //         if(verify_paddle_position(proposed_paddle_position) == 1) client->paddle_pos.y --;
    //     }
    // }

    // else if (direction == KEY_DOWN){
    //     if (client->paddle_pos.y  != WINDOW_SIZE-2){
    //         proposed_paddle_position.y ++;
    //         if(verify_paddle_position(proposed_paddle_position) == 1) client->paddle_pos.y ++;
    //     }
    // }

    // else if (direction == KEY_LEFT){
    //     if (client->paddle_pos.x - client->paddle_pos.length != 1){
    //         proposed_paddle_position.x --;
    //         if(verify_paddle_position(proposed_paddle_position) == 1) client->paddle_pos.x --;
    //     }
    // }

    // else if (direction == KEY_RIGHT)
    //     if (client->paddle_pos.x + client->paddle_pos.length != WINDOW_SIZE-2){
    //         proposed_paddle_position.x ++;
    //         if(verify_paddle_position(proposed_paddle_position) == 1) client->paddle_pos.x ++;
    // }
    // pthread_rwlock_wrlock(&clientListLock); 

    pthread_rwlock_wrlock(&clientListLock);
    switch (direction) {
        case KEY_LEFT:
            if (client->paddle_pos.x - client->paddle_pos.length != 1){
                proposed_paddle_position.x --;
                if(verify_paddle_position(proposed_paddle_position, client) == 1) client->paddle_pos.x --;
            }
            break;
        case KEY_RIGHT:
            if (client->paddle_pos.x + client->paddle_pos.length != WINDOW_SIZE-2){
                proposed_paddle_position.x ++;
                if(verify_paddle_position(proposed_paddle_position, client) == 1) client->paddle_pos.x ++;
            }
            break;
        case KEY_DOWN:
            if (client->paddle_pos.y  != WINDOW_SIZE-2){
                proposed_paddle_position.y ++;
                if(verify_paddle_position(proposed_paddle_position, client) == 1) client->paddle_pos.y ++;
            }
            break;
        case KEY_UP:
            if (client->paddle_pos.y  != 1){
                proposed_paddle_position.y --;
                if(verify_paddle_position(proposed_paddle_position, client) == 1) client->paddle_pos.y --;
            }
            break;
        default:
            break;
        }
    pthread_rwlock_unlock(&clientListLock);
}