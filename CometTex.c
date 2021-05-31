#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define COMETTEX_VERSION "0.0.1"
#define CTRL_KEY(c) ((c) & 0x1f)

enum editorKey{
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};

struct editorConfig{
    int mx,my;
    int screenRow;
    int screenCol;
    struct termios orignal_termios;
};

struct editorConfig E;

struct abuf{
    char *b;
    int len;
};

#define ABUF_INIT {NULL,0}

void abAppend(struct abuf *ab,const char *s,int len){
    char *new = realloc(ab->b,ab->len + len);

    if(new == NULL) return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void abFree(struct abuf *ab){
    free(ab->b);
}

void clearScreen(){
    //Clear the entire screen
    write(STDOUT_FILENO, "\x1b[2H", 4);
    //Reposition the cursor to the top left
    write(STDOUT_FILENO, "\x1b[H",3);
}

void die(const char *s){
    clearScreen();
    perror(s);
    exit(1);
}

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

void editorDrawRow(struct abuf *ab){
    for(int i = 0;i<E.screenRow;i++){
        if (i == E.screenRow / 3){
            char welcome[124];
            int welcomeLen = snprintf(welcome, sizeof(welcome), "CometTex Editor -- Version %s", COMETTEX_VERSION);
            if (welcomeLen > E.screenCol){
                welcomeLen = E.screenCol;
            }
            int padding = (E.screenCol - welcomeLen)/2;
            if (padding){
                abAppend(ab,"~",1);
                padding--;
            }
            while(padding--) abAppend(ab, " ", 1);
            abAppend(ab,welcome, welcomeLen);
        }else{
            abAppend(ab, "~", 1);
        }

        abAppend(ab, "\x1b[K", 3);
        if (i < E.screenRow - 1){
            abAppend(ab, "\r\n", 2);
        }
    }
}

void editorRefreshScreen(){
    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRow(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.my + 1, E.mx + 1);
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

int editorReadKey(){
    int nread;
    char c;
    while((nread = read(STDIN_FILENO, &c, 1)) != 1){
        if (nread == 1 && errno != EAGAIN) die("read");
    }

    if (c == '\x1b'){
        char seq[3];

        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

        if (seq[0] == '['){
            if (seq[1] >= '0' && seq[1] <= '9'){
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if (seq[2] == '~'){
                    switch(seq[1]){
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
                switch (seq[1]){
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
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }

        return '\x1b';
    }else{
        return c;
    }
}

void editorMoveCursor(int key){
    switch(key){
        case ARROW_LEFT:
            if (E.mx != 0){
                E.mx--;
            }
            break;
        case ARROW_RIGHT:
            if (E.mx != E.screenCol - 1){
                E.mx++;
            }
            break;
        case ARROW_UP:
            if (E.my != 0){
                E.my--;
            }
            break;
        case ARROW_DOWN:
            if (E.my != E.screenRow - 1){
                E.my++;
            }
            break;
    }
}

void editorProcessKeypress(){
    int c = editorReadKey();

    switch(c){
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H",3);
            exit(0);
            break;
        
        case HOME_KEY:
            E.mx = 0;
            break;
        case END_KEY:
            E.mx = E.screenCol - 1;
            break;
        
        case PAGE_UP:
        case PAGE_DOWN:
            {
                int times = E.screenRow;
                while(times--){
                    editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
                }
            }
            break;
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            editorMoveCursor(c);
            break;
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



void initEditor(){
    E.mx = 0;
    E.my = 0;

    if (getWindowSize(&E.screenRow, &E.screenCol) == -1) die("getWindowSize");
}

int main(){
    enableRawMode();
    initEditor();

    while (1){
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}