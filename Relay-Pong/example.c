#include <stdlib.h>
#include <ncurses.h>


#include "pong.h"

WINDOW * message_win;

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

void new_paddle (paddle_position_t * paddle, int legth){
    paddle->x = WINDOW_SIZE/2;
    paddle->y = WINDOW_SIZE-2;
    paddle->length = legth;
}

void draw_paddle(WINDOW *win, paddle_position_t * paddle, int delete){
    int ch;
    if(delete){
        ch = '_';
    }else{
        ch = ' ';
    }
    int start_x = paddle->x - paddle->length;
    int end_x = paddle->x + paddle->length;
    for (int x = start_x; x <= end_x; x++){
        wmove(win, paddle->y, x);
        waddch(win,ch);
    }
    wrefresh(win);
}

void moove_paddle (paddle_position_t * paddle, int direction){
    if (direction == KEY_UP){
        if (paddle->y  != 1){
            paddle->y --;
        }
    }
    if (direction == KEY_DOWN){
        if (paddle->y  != WINDOW_SIZE-2){
            paddle->y ++;
        }
    }
    

    if (direction == KEY_LEFT){
        if (paddle->x - paddle->length != 1){
            paddle->x --;
        }
    }
    if (direction == KEY_RIGHT)
        if (paddle->x + paddle->length != WINDOW_SIZE-2){
            paddle->x ++;
    }
}

void place_ball_random(ball_position_t * ball){
    ball->x = rand() % WINDOW_SIZE ;
    ball->y = rand() % WINDOW_SIZE ;
    ball->c = 'o';
    ball->up_hor_down = rand() % 3 -1; //  -1 up, 1 - down
    ball->left_ver_right = rand() % 3 -1 ; // 0 vertical, -1 left, 1 right
}


void moove_ball(ball_position_t * ball, paddle_position_t paddle){
    
    int next_x = ball->x + ball->left_ver_right;
    if( next_x == 0 || next_x == WINDOW_SIZE-1){
        ball->left_ver_right *= -1;
        ball->x += ball->left_ver_right;
        mvwprintw(message_win, 2,1,"left right win");
        wrefresh(message_win);
     }else{
        ball->x = next_x;
    }

    
    int next_y = ball->y + ball->up_hor_down;
    int paddle_start_x = paddle.x - paddle.length;
    int paddle_end_x = paddle.x + paddle.length;
    if( next_y == 0 || next_y == WINDOW_SIZE-1 || (paddle_start_x <= ball->x && ball->x <= paddle_end_x && ball->y == paddle.y) ){
        ball->up_hor_down *= -1;
        ball->y += ball->up_hor_down;
        mvwprintw(message_win, 2,1,"bottom top win");
        //mvwprintw(message_win, 2,1,"Ball x: %d, Ball y: %d", ball->x, ball->y);
        //mvwprintw(message_win, 3,1,"Paddle x: %d, Paddle y: %d", paddle.x, paddle.y);
        wrefresh(message_win);
    }else{
        ball -> y = next_y;
    }

}

void draw_ball(WINDOW *win, ball_position_t * ball, int draw){
    int ch;
    if(draw){
        ch = ball->c;
    }else{
        ch = ' ';
    }
    wmove(win, ball->y, ball->x);
    waddch(win,ch);
    wrefresh(win);
}

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

paddle_position_t paddle;
ball_position_t ball;

int main(){
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


    new_paddle(&paddle, PADLE_SIZE);
    draw_paddle(my_win, &paddle, true);

    place_ball_random(&ball);
    draw_ball(my_win, &ball, true);

    int key = -1;
    while(key != 27){
        key = wgetch(my_win);		
        if (key == KEY_LEFT || key == KEY_RIGHT || key == KEY_UP || key == KEY_DOWN){
            // mvwprintw(message_win, 3,1,"Ball x: %d, Ball y: %d", ball.x, ball.y);
            // mvwprintw(message_win, 4,1,"Hor Move: %d, Ver Move: %d", ball.up_hor_down, ball.left_ver_right);
            // wmove(message_win, 2, 1);
            // wclrtoeol(message_win);
            // wrefresh(message_win);  // Refresh the window to clear the line

            draw_paddle(my_win, &paddle, false);
            moove_paddle (&paddle, key);
            draw_paddle(my_win, &paddle, true);

            draw_ball(my_win, &ball, false);
            moove_ball(&ball, paddle);
            draw_ball(my_win, &ball, true);
        }

        print_keys_message(key);
        wrefresh(message_win);	
    }

    exit(0);
}