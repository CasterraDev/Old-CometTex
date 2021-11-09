#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "CometTex.h"

void disableRawMode(){
    if( tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orignal_termios) == -1) die("DisableRawMode() Failed");
}

void enableRawMode(){
    if (tcgetattr(STDIN_FILENO, &E.orignal_termios) == -1) die("EnableRawMode() tcgetattr Failed");
    //atexit is called when the program exits
    atexit(disableRawMode);

    struct termios raw = E.orignal_termios;
    //Turn off some flags
    raw.c_cflag |= (CS8);
    raw.c_oflag &= ~(OPOST); //OPOST turns off output processing
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP); //IXON disables ctrl-s and ctrl-q,ICRNL fixes ctrl-m
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN); //Turn off Echo,ICANON,ISIG disables ctrl-c, ctrl-z,IEXTEN disables ctrl-v

    raw.c_cc[VMIN] = 0; //Min amount of bytes read() needs before it can return
    raw.c_cc[VTIME] = 1; //Max amount of time read() waits before it returns

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("EnableRawMode() tcsetattr Failed");
}

int editorReadKey(){
    int nread;
    char c;
    while((nread = read(STDIN_FILENO, &c, 1)) != 1){
        if (nread == -1 && errno != EAGAIN) die("EditorReadKey() failed");
    }

    //If the character starts with an escape. It could be the start of on escape sequence
    if (c == '\x1b'){
        char seq[3];
        //Read the character after the escape key
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        //Read the character after the one above
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

        //If the escape key is the beginning of an esacape sequence
        if (seq[0] == '['){
            //Escape sequences can sometimes have a number, so check for that
            if (seq[1] >= '0' && seq[1] <= '9'){
                //If so read the next character to get the full escape sequence
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if (seq[2] == '~'){
                    switch(seq[1]){
                        //Seq would be \x1b1~ for HOME_KEY
                        case '1': return HOME_KEY;
                        case '3': return DEL_KEY;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                }
            }else{
                //If not just read the next character
                switch (seq[1]){
                    //Seq would be \x1bA for ARROW_UP
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                }
            }
        } else if (seq[0] == 'O'){
            switch(seq[1]){
                //Seq would be \x1b0H
                case 'H': return HOME_KEY;
                //Seq would be \x1b0F
                case 'F': return END_KEY;
            }
        }

        return '\x1b';
    }else{
        return c;
    }
}

int getCursorPos(int *rows,int *cols){
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n",4) != 4) return -1;

    while (i < sizeof(buf) - 1){
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

    return 0;
}

int getWindowSize(int *rows,int *cols){
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0){
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B",12) != 12) return -1;
        return getCursorPos(rows,cols);
    }else{
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}