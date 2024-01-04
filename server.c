#include "server_functions.h"


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

/*---------------------------------------------------------------------------------
    THREAD'S NAME:      client_thread
    DESCRIPTION:        Thread assigned to each client (specified by arg) after connection
                        is established. It comunicates with client through the designated
                        socket and executes the necessary functions (specified by flags).
    ARGUMENTS:          void *arg - file descriptor of socket used to comunicate with client
    FLAG FROM CLIENT:  -2 something unexpected happened to client (raised by the server)
                       -1 client will disconnect
    FLAGS:              0 server acknowledges that client disconnecting
    RETURN VALUES:      - 
---------------------------------------------------------------------------------*/
void *client_thread(void *arg){

    int client_msg;
    int msg_flag;
    ClientInfo* client_ptr;

    Client_Info_to_Thread* aux = (Client_Info_to_Thread*) arg; // arg is pointing to the structure client_info
    int client_sock = aux->client_sock;
    struct sockaddr_in client_addr = aux->client_addr;

    int flag_connection = handleConnectionRequest(client_sock, client_addr);
    if (flag_connection == -2 || flag_connection == -1 ){ // if connection was rejected
        close(client_sock);
        pthread_exit(NULL);
    }

    client_ptr = addClient(client_sock, client_addr); // adds client to client list

    while (1){

        if (read(client_sock, &client_msg, sizeof(int)) < sizeof(int)){
            perror("\nERROR Reading client paddle movement ");
            break; 
        } 

        if ( client_msg == 81 || client_msg == 113 ) break; // client sends 'Q' or 'q' (client wants to quit)

        moove_paddle(client_ptr, client_msg);

        pthread_mutex_lock(&board_updates_mux);
        broadcast_board_update();
        pthread_mutex_unlock(&board_updates_mux);

    }

    disconnectClient(client_addr);
    close(client_sock);
    pthread_exit(NULL);

}



/*---------------------------------------------------------------------------------
    THREAD'S NAME:      game_thread
    DESCRIPTION:        Thread that runs the ping pong game. It computes the position of 
                        the ball every second, verifies collisions and sends updates to clients
    ARGUMENTS:          ---
    RETURN VALUES:      --- 
---------------------------------------------------------------------------------*/
void* game_thread(void* arg) {

    place_ball_random(); // Places ball randomly 

    while(1){
        pthread_mutex_lock(&board_updates_mux);
        broadcast_board_update();
        pthread_mutex_unlock(&board_updates_mux);
        
        
        // pthread_rwlock_rdlock(&ballLock); // Lock ball for writing
        // printf("Ball x: %d, Ball y: %d\n", ball.x, ball.y);
        // pthread_rwlock_unlock(&ballLock); // Lock ball for writing

        moove_ball();

        //sleep(1);
        usleep(300000); // Sleep for 0.5 seconds (500,000 microseconds)
    }

    pthread_exit(NULL);
}


/* ****************************************************************************************** MAIN ****************************************************************************************** */

int main() {
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t addr_size = sizeof(clientAddr);
    Client_Info_to_Thread client_info;
    //pthread_t t_id[MAX_CLIENTS], ui_tid;
    pthread_t t_id, ui_tid, game_tid;

    // Set up a signal handler for Ctrl+C (SIGINT)
    if (signal(SIGINT, handle_sudden_server_disconnection) == SIG_ERR) {
        perror("Unable to set up signal handler");
        return 1;
    }

    // Clear Terminal
    system("clear");

    //Creation of thread responsible for user interface
    if (pthread_create(&ui_tid, NULL, userInputThread, NULL) != 0){
        perror("\nERROR Creating UI thread ");
        exit(-1);
    }


    // Create a socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    unlink(SERVER_IP);

    // Initialize server address structure
    memset(&serverAddr, 0, sizeof(serverAddr)); 
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = inet_addr(SERVER_IP);

    // Initialize client address structure
    memset(&clientAddr, 0, sizeof(serverAddr)); 

    if (bind(server_socket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        perror("Error binding socket");
        close(server_socket);
        exit(EXIT_FAILURE); 
    }

    // Listen for incoming connections
    if (listen(server_socket, 5) == -1) {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // for(int i = 0; i<MAX_CLIENTS; i++){ 
    //     memset(&t_id[i], -1, sizeof(t_id[i])); 
    //     //t_id[i] = -1; //sets all thread id as not used
    // }

    // Starts Game
    if (pthread_create(&game_tid, NULL, &game_thread, NULL) != 0){
        perror("\nERROR Creating Game thread ");
        exit(-1);
    }

    int i = 0;
    while(1){

        //i = wait_for_available_thread(t_id);

        client_info.client_sock = accept(server_socket, (struct sockaddr *)&client_info.client_addr, &addr_size);

        if (pthread_create(&t_id, NULL, &client_thread, &client_info) != 0){
            perror("\nERROR Creating Client thread ");
            exit(-1);
        }

        pthread_detach(t_id); //  If tid has not terminated, pthread_detach() does not cause the thread to terminate.

    }


    close(server_socket);
    return(0);
}