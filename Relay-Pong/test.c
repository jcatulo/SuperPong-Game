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

typedef struct {
    int x;
    int y;
} PaddlePosition;

int main()
{
    PaddlePosition paddlePosition = {42, 77};

    // Serialize the PaddlePosition
    char buffer[sizeof(PaddlePosition)];
    memcpy(buffer, &paddlePosition, sizeof(PaddlePosition));

    char buffer[sizeof(PaddlePosition)];
    memcpy(buffer, &paddlePosition, sizeof(PaddlePosition));
    return 0;
}