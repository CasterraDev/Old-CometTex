#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define COMETTEX_VERSION "0.0.1"
#define COMETTEX_TAB_STOP 8
#define CTRL_KEY(c) ((c) & 0x1f)

typedef struct erow {
    int size;
    int rsize;
    char *chars;
    char *render;
} erow;

struct editorConfig{
    int mx,my;
    int colOffset;
    int rowOffset;
    int screenRow;
    int screenCol;
    int numRows;
    erow *row;
    struct termios orignal_termios;
};

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

void editorScroll(){
    //Vertical Scrolling
    if (E.my < E.rowOffset){
        E.rowOffset = E.my;
    }
    if (E.my >= E.rowOffset + E.screenRow){
        E.rowOffset = E.my - E.screenRow + 1;
    }
    //Horizontal Scrolling
    if (E.mx < E.colOffset){
        E.colOffset = E.mx;
    }
    if (E.mx >= E.colOffset + E.screenCol){
        E.colOffset = E.mx - E.screenCol + 1;
    }
}

void editorDrawRow(struct abuf *ab){
    for(int i = 0;i<E.screenRow;i++){
        int fileRow = i + E.rowOffset;
        if (fileRow >= E.numRows){
            if (E.numRows == 0 && i == E.screenRow / 3){
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
        }else{
            int len = E.row[fileRow].rsize - E.colOffset;
            if (len < 0) len = 0;
            if (len > E.screenCol) len = E.screenCol;
            abAppend(ab, &E.row[fileRow].render[E.colOffset], len);
        }

        abAppend(ab, "\x1b[K", 3);
        if (i < E.screenRow - 1){
            abAppend(ab, "\r\n", 2);
        }
    }
}

void editorRefreshScreen(){
    editorScroll();

    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRow(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.my - E.rowOffset) + 1, (E.mx - E.colOffset) + 1);
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
    erow *row = (E.my >= E.numRows) ? NULL : &E.row[E.my];

    switch(key){
        case ARROW_LEFT:
            if (E.mx != 0){
                E.mx--;
            }else if (E.my > 0){
                E.my--;
                E.mx = E.row[E.my].size;
            }
            break;
        case ARROW_RIGHT:
            if (row && E.mx < row->size){
                E.mx++;
            }else if (row && E.mx == row->size){
                E.my++;
                E.mx = 0;
            }
            break;
        case ARROW_UP:
            if (E.my != 0){
                E.my--;
            }
            break;
        case ARROW_DOWN:
            if (E.my < E.numRows){
                E.my++;
            }
            break;
    }

    row = (E.my >= E.numRows) ? NULL : &E.row[E.my];
    int rowlen = row ? row->size : 0;
    if (E.mx > rowlen){
        E.mx = rowlen;
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

void editorUpdateRow(erow *row){
    int tabs = 0;
    for (int i = 0;i<row->size;i++){
        if (row->chars[i] == '\t') tabs++;
    }

    free(row->render);
    row->render = malloc(row->size + tabs*(COMETTEX_TAB_STOP - 1) + 1);

    int idx = 0;
    for (int i = 0;i<row->size;i++){
        if (row->chars[i] == '\t'){
            row->render[idx++] = ' ';
            while (idx % COMETTEX_TAB_STOP != 0) row->render[idx++] = ' ';
        }else{
            row->render[idx++] = row->chars[i];
        }
    }
    row->render[idx] = '\0';
    row->rsize = idx;
}

void editorAppendRow(char *s, size_t len){
    E.row = realloc(E.row, sizeof(erow) * (E.numRows + 1));

    int at = E.numRows;
    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';

    E.row[at].rsize = 0;
    E.row[at].render = NULL;
    editorUpdateRow(&E.row[at]);

    E.numRows++;
}

void editorOpen(char *filename){
    FILE *fp = fopen(filename, "r");
    if (!fp) die("fopen");

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while ((linelen = getline(&line, &linecap, fp)) != -1){
        while(linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r')){
            linelen--;
        }
        editorAppendRow(line, linelen);
    }
    free(line);
    fclose(fp);
}

void initEditor(){
    E.mx = 0;
    E.my = 0;
    E.rowOffset = 0;
    E.colOffset = 0;
    E.numRows = 0;
    E.row = NULL;

    if (getWindowSize(&E.screenRow, &E.screenCol) == -1) die("getWindowSize");
}

int main(int argc, char *argv[]){
    enableRawMode();
    initEditor();
    if (argc >= 2){
        editorOpen(argv[1]);
    }

    while (1){
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}