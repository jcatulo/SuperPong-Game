#include "client_lib.h"


/* ****************************************************************************************** THREADS ****************************************************************************************** */


/*---------------------------------------------------------------------------------
    THREAD'S NAME:      game_thread
    DESCRIPTION:        Thread that runs the ping pong game. It receives the position of 
                        the ball and paddles every second.
    ARGUMENTS:          ---
    RETURN VALUES:      --- 
---------------------------------------------------------------------------------*/
void* game_thread(void* arg) {

    Board_State boardState;
    Board_State previouseBoardState; // is used to save ball and paddle positios in order to clean the board

    // my_win = initialize_window();
    initialize_window();

    if( receive_board_update(&boardState) == -1 ){ // means that there is an error with the server, so client will exit
        printf("\rClient will exit.\n");
        close(sockfd);
        exit(0);
    }

    draw_paddles(boardState.client_paddle_pos, boardState.other_paddle_pos, boardState.nmr_other_clients, true); // draw paddles
    draw_ball(boardState.ball_pos, true); // draw ball

    pthread_mutex_lock(&window_semaphore_mux);
    window_semaphore = 1; // after gaming window being created and the paddle initialized, the other thread is allowed to receive user input
    pthread_mutex_unlock(&window_semaphore_mux);

    while(1){

        previouseBoardState = boardState; // save old board state (client is going to receive an update)

        if( receive_board_update(&boardState) == -1 ){ // receive new board state
            printf("\rClient will exit.\n");
            close(sockfd);
            exit(0);
        }

        draw_paddles(previouseBoardState.client_paddle_pos, previouseBoardState.other_paddle_pos, previouseBoardState.nmr_other_clients, false); // delete previous paddles
        draw_ball(previouseBoardState.ball_pos, false); // delete previous ball

        draw_paddles(boardState.client_paddle_pos, boardState.other_paddle_pos, boardState.nmr_other_clients, true); // draw new paddles
        draw_ball(boardState.ball_pos, true); // draw new ball

        //printf("\rBall_x: %d, Ball_y: %d\n", boardState.ball_pos.x, boardState.ball_pos.y);
        //printf("\r\rPaddle_x: %d, Paddle_y: %d\n", boardState.client_paddle_pos.x, boardState.client_paddle_pos.y);

        free(previouseBoardState.other_paddle_pos); // free dynamically allocated memory (allocated in receive_board_update)

        pthread_mutex_lock(&window_semaphore_mux);
        if(window_semaphore == 0) break; // If main thread wants to close the game, break the cycle
        pthread_mutex_unlock(&window_semaphore_mux);
    }

    free(boardState.other_paddle_pos); // free dynamically allocated memory (allocated in receive_board_update)
    pthread_exit(NULL);
}



/* ****************************************************************************************** MAIN ****************************************************************************************** */


int main(){

    struct sockaddr_in serverAddr;
    pthread_t game_tid;
    int key = -1; // save user input

    window_semaphore = 0;  // Initializes window semaphore to 0

    // Clear Terminal
    system("clear");
    
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("\rError creating socket");
        exit(EXIT_FAILURE);
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    inet_aton(SERVER_IP, &(serverAddr.sin_addr));


    // Connect to the server
    if (connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1) {
        perror("\rConnection failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (confirm_connection(sockfd, serverAddr) == -1){ // returns 1 if success, or -1 if unsuccessful
        printf("\rServer Recused Connection. Process wil exit.\n");
        exit(0);
    }

    // Set up a signal handler for Ctrl+C (SIGINT)
    if (signal(SIGINT, handle_sudden_disconnection) == SIG_ERR) {
        perror("\rUnable to set up signal handler");
        return 1;
    }

    // After the connection confirmation, create a thread that handles the game
    if (pthread_create(&game_tid, NULL, &game_thread, NULL) != 0){
        perror("\r\nERROR Creating Game thread ");
        exit(-1);
    }

    while(1){ 
        pthread_mutex_lock(&window_semaphore_mux);
        if (window_semaphore == 1) break; // Wait until other thread created gaming window and received the board status
        pthread_mutex_unlock(&window_semaphore_mux);

        usleep(200000); // Sleep for 0.2 seconds 
    }


    while(key != 81 && key != 113){ // represents 'Q' or 'q' (while user does not want to quit)
        key = wgetch(my_win);
        // printf("%d\n", key);
        // printf("%c\n", key);
        if (key == KEY_LEFT || key == KEY_RIGHT || key == KEY_DOWN || key == KEY_UP){
            if( write(sockfd, &key, sizeof(int)) <  sizeof(int)) {
                perror("\r\nERROR writing to server ");
                break;
            }
            print_keys_message(key);
            wrefresh(message_win); // refreshes message window
        }

    }

    pthread_mutex_lock(&window_semaphore_mux);
    window_semaphore = 0; // This signals the gaming thread that is necessary to close the game and free the memory
    pthread_mutex_unlock(&window_semaphore_mux);


    // Wait for the gaming to finish
    if (pthread_join(game_tid, NULL) != 0) {
        perror("\r\nError joining thread ");
        close(sockfd);
        exit(0);
    }

    key = 81;
    write(sockfd, &key, sizeof(int)); // send quit message to server

    close(sockfd);
    exit(0);
}

