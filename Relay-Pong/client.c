#include "client_lib.h"

int main(){

    int sockfd;
    struct sockaddr_in serverAddr;
    char input[1024];
    ssize_t bytesSent, bytesRead;
    
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    inet_aton(SERVER_IP, &(serverAddr.sin_addr));

    int conection_flag = request_connection(sockfd, serverAddr); // returns 1 if success, or -1 if unsuccessful

    if (conection_flag == 0){
        printf("Server Recused Connection. Process wil exit.\n");
        exit(0);
    }

    initialize_window_and_paddle();

    while (1) {

        printf("Enter a message to send to the server (type 'q' to quit): ");
        fgets(input, sizeof(input), stdin);

        // Remove the newline character from the input
        input[strcspn(input, "\n")] = '\0';
        
        // Check if the user wants to quit
        if (strcasecmp(input, "q") == 0) {
            printf("Exiting the client...\n");
            
            strcpy(input, "Q");
            bytesSent = sendto(sockfd, input, strlen(input), 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
            if (bytesSent == -1) {
                perror("Error sending data");
                close(sockfd);
                exit(EXIT_FAILURE);
            }
            break;
        }

        printf("Message sent to the server: %s\n\n", input);
    }


    close(sockfd);
    return 0;
}