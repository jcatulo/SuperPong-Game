#include "server_functions.h"



int main() {
    int sockfd;
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t addrLen = sizeof(clientAddr);
    ssize_t bytesRead;
    char rcv_msg[1024];
    int msg_flag = 0;
    ClientInfo *current_client;

    // Clear Terminal
    system("clear");

    // Create thread to receive user input
    pthread_t userInputThreadId; 
    //pthread_create(&userInputThreadId, NULL, userInputThread, NULL);


    // Create a socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Initialize server address structure
    memset(&serverAddr, 0, sizeof(serverAddr)); 
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = inet_addr(SERVER_IP);

    // Initialize client address structure
    memset(&clientAddr, 0, sizeof(serverAddr)); 

    if (bind(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        perror("Error binding socket");
        close(sockfd);
        exit(EXIT_FAILURE); 
    }

    // Places ball randomly
    place_ball_random();
    
    while(1){
        memset(rcv_msg, 0, sizeof(rcv_msg));
        bytesRead = recvfrom(sockfd, rcv_msg, sizeof(rcv_msg), 0, (struct sockaddr*)&clientAddr, &addrLen);
        if (bytesRead == -1) {
            perror("Error receiving data");
            continue;
        }
        rcv_msg[strcspn(rcv_msg, "\n")] = '\0';  // Null-terminate the received data

        msg_flag = verify_message(sockfd, clientAddr, rcv_msg);
        if (msg_flag == -1 || msg_flag == 0 || msg_flag == 2) continue;   

        current_client = search_for_client(clientAddr);

        if (msg_flag == 1){
            initialize_client_paddle(current_client);
            send_board_update(sockfd, current_client);
            continue;
        }


    }


    close(sockfd);
    return(0);
}