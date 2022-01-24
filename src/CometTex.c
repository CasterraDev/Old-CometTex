#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <sys/ioctl.h>
#include "CometTex.h"
#include "appendBuffer.h"
#include "rawmode.h"
#include "ops.h"
#include "fileIO.h"
#include "command.h"
#include "syntaxHighlighting.h"

void die(const char *s){
    //Clear the entire screen
    write(STDOUT_FILENO, "\x1b[2J", 4);
    //Reposition the cursor to the top left
    write(STDOUT_FILENO, "\x1b[H",3);
    perror(s);
    exit(1);
}

void editorScroll(){
    //Editor's render X is 0
    E.rx = 0;
    
    if (E.my < E.numRows){
        E.rx = rowMxToRx(&E.row[E.my], E.mx);
    }

    //Vertical Scrolling
    if (E.my < E.rowOffset){
        E.rowOffset = E.my;
    }
    if (E.my >= E.rowOffset + E.screenRow){
        E.rowOffset = E.my - E.screenRow + 1;
    }
    //Horizontal Scrolling
    if (E.rx < E.colOffset){
        E.colOffset = E.mx;
    }
    if (E.rx >= E.colOffset + E.screenCol){
        E.colOffset = E.rx - E.screenCol + 1;
    }
}

void editorDrawRow(struct abuf *ab){
    for(int i = 0;i<E.screenRow;i++){
        int fileRow = i + E.rowOffset;
        if (fileRow >= E.numRows){
            if (E.numRows == 0 && i == E.screenRow / 3){
                char welcome[124];
                int welcomeLen = snprintf(welcome, sizeof(welcome), "CometTex Editor -- Version %s", "0.0.1");
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
            char *c = &E.row[fileRow].render[E.colOffset];
            unsigned char *hl = &E.row[fileRow].hl[E.colOffset];
            int curColor = -1;
            for (int i = 0;i<len;i++){
                if (iscntrl(c[i])){
                    char s = (c[i] <= 26) ? '@' + c[i] : '?';
                    abAppend(ab, "\x1b[7m", 4);//Invert the colors
                    abAppend(ab, &s, 1);
                    abAppend(ab, "\x1b[m", 3);//Invert the colors back to normal
                    //"\x1b[m" turns off all text formatting even text colors
                    //So let's change it to the current color again
                    if (curColor != -1){
                        char buf[16];
                        int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", curColor);
                        abAppend(ab, buf, clen);
                    }

                }else if (hl[i] == HL_NORMAL){
                    if (curColor != -1){
                        abAppend(ab, "\x1b[39m", 5);
                        curColor = -1;
                    }
                    abAppend(ab, &c[i], 1);
                }else{
                    int color = editorSyntaxToColor(hl[i]);
                    if (color != curColor){
                        curColor = color;
                        char buf[16];
                        int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
                        abAppend(ab, buf, clen);
                    }
                    abAppend(ab, &c[i], 1);
                }
            }
            abAppend(ab, "\x1b[39m", 5);
        }

        abAppend(ab, "\x1b[K", 3);
        abAppend(ab, "\r\n", 2);
    }
}

void editorDrawStatusBar(struct abuf *ab){
    abAppend(ab, "\x1b[7m", 4);
    char status[80], rstatus[80];

    int len = snprintf(status, sizeof(status), "%.20s - %d lines %s", E.filename ? E.filename : "[No Name]", E.numRows, E.dirty ? "(modified)" : "");
    int rlen = snprintf(rstatus, sizeof(rstatus), "%s | %d, %d",E.syntax ? E.syntax->fileType : "no ft", E.my + 1, E.rx);

    if (len > E.screenCol) len = E.screenCol;
    abAppend(ab, status, len);
    while (len < E.screenCol){
        if (E.screenCol - len == rlen){
            abAppend(ab, rstatus, rlen);
            break;
        }else{
            abAppend(ab, " ", 1);
            len++;
        }
    }
    abAppend(ab, "\x1b[m", 3);
    abAppend(ab, "\r\n", 2);
}

void editorDrawMessageBar(struct abuf *ab){
    abAppend(ab, "\x1b[K", 3);
    int msgLen = strlen(E.statusMsg);
    if (msgLen > E.screenCol) msgLen = E.screenCol;
    if (msgLen && time(NULL) - E.statusMsg_time < 5){
        abAppend(ab, E.statusMsg, msgLen);
    }
}

void editorRefreshScreen(){
    editorScroll();

    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRow(&ab);
    editorDrawStatusBar(&ab);
    editorDrawMessageBar(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.my - E.rowOffset) + 1, (E.rx - E.colOffset) + 1);
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

void editorSetStatusMessage(const char *fmt, ...){
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.statusMsg, sizeof(E.statusMsg), fmt, ap);
    va_end(ap);
    E.statusMsg_time = time(NULL);
}

char *editorPrompt(char *prompt, void (*callback)(char *, int)){
    size_t bufSize = 128;
    char *buf = malloc(bufSize);

    size_t bufLen = 0;
    buf[0] = '\0';

    while (1){
        editorSetStatusMessage(prompt, buf);
        editorRefreshScreen();

        int c = editorReadKey();
        if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE){
            if (bufLen != 0) buf[--bufLen] = '\0';
        }else if (c == '\x1b'){
            editorSetStatusMessage("");
            if (callback) callback(buf, c);
            free(buf);
            return NULL;
        }else if (c == '\r'){
            if (bufLen != 0){
                editorSetStatusMessage("");
                if (callback) callback(buf, c);
                return buf;
            }
        }else if (!iscntrl(c) && c < 128){
            if (bufLen == bufSize - 1){
                bufSize *= 2;
                buf = realloc(buf, bufSize);
            }
            buf[bufLen++] = c;
            buf[bufLen] = '\0';
        }

        if (callback) callback(buf, c);
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

void editorFindCallback(char *query, int key){
    static int last_match = -1;
    static int direction = 1;

    static int saved_hl_line;
    static char *saved_hl = NULL;

    if (saved_hl){
        memcpy(E.row[saved_hl_line].hl, saved_hl, E.row[saved_hl_line].rsize);
        free(saved_hl);
        saved_hl = NULL;
    }

    if (key == '\r' || key == '\x1b'){
        last_match = -1;
        direction = 1;
        return;
    }else if (key == ARROW_RIGHT || key == ARROW_DOWN){
        direction = 1;
    }else if (key == ARROW_LEFT || key == ARROW_UP){
        direction = -1;
    }else{
        last_match = -1;
        direction = 1;
    }

    if (last_match == -1) direction = 1;
    int cur = last_match;

    for (int i = 0;i<E.numRows;i++){
        cur += direction;
        if (cur == -1) cur = E.numRows - 1;
        else if (cur == E.numRows) cur = 0;

        erow *row = &E.row[cur];
        char *match = strstr(row->render, query);
        if (match){
            last_match = cur;
            E.my = cur;
            E.mx = rowRxtoMx(row, match - row->render);
            E.rowOffset = E.numRows;

            saved_hl_line = cur;
            saved_hl = malloc(row->rsize);
            memcpy(saved_hl, row->hl, row->rsize);
            //Highlight the matches
            memset(&row->hl[match - row->render], HL_MATCH, strlen(query));
            break;
        }
    }
}

void editorFind(){
    int saved_mx = E.mx;
    int saved_my = E.my;
    int saved_colOff = E.colOffset;
    int saved_rowOff = E.rowOffset;

    char *query = editorPrompt("Search: %s (ESC to cancel)", editorFindCallback);

    if (query){
        free(query);
    }else{
        E.mx = saved_mx;
        E.my = saved_my;
        E.colOffset = saved_colOff;
        E.rowOffset = saved_rowOff;
    }
}

void enterInsertMode(int key){
    switch (key) {
        case 'i':
            E.mode = 0;
            break;
        case 'I': 
            E.mx = 0;
            E.mode = 0;
            break;
        case 'a': 
            editorMoveCursor(ARROW_RIGHT);
            E.mode = 0;
            break;
        case 'A': 
            if (E.my < E.numRows) {
                E.mx = E.row[E.my].size;
            }
            E.mode = 0;
            break;
        case 'o':
            if (E.my < E.numRows) {
                E.mx = E.row[E.my].size;
            }
            editorInsertNewLine();
            E.mode = 0;
            break;
        case 'O':
            E.mx = 0;
            editorInsertNewLine();
            editorMoveCursor(ARROW_UP);
            E.mode = 0;
            break;
    }
}

void processKeypressNormal(){
    static int quit_times = COMETTEX_QUIT_TIMES;

    int c = editorReadKey();

    switch(c){
        case CTRL_KEY('q'):
            if (E.dirty && quit_times > 0){
                editorSetStatusMessage("WARNING!! File modified. Press Ctrl+Q %d more times to quit.", quit_times);
                quit_times--;
                return;
            }
            //Clear the entire screen
            write(STDOUT_FILENO, "\x1b[2J", 4);
            //Reposition the cursor to the top left
            write(STDOUT_FILENO, "\x1b[H",3);
            exit(0);
            break;
        
        case CTRL_KEY('s'):
            editorSave();
            break;

        case CTRL_KEY('f'):
            editorFind();
            break;

        case CTRL_KEY('x'):
            editorSave();
            //Clear the entire screen
            write(STDOUT_FILENO, "\x1b[2J", 4);
            //Reposition the cursor to the top left
            write(STDOUT_FILENO, "\x1b[H",3);
            exit(0);
            break;
        
        case HOME_KEY:
            //Bring the cursor(mouse) to the beginning of the line
            E.mx = 0;
            break;
        case END_KEY:
            //Bring cursor to end of line
            if (E.my < E.numRows){
                E.mx = E.row[E.my].size;
            }
            break;

        case BACKSPACE:
            editorMoveCursor(ARROW_LEFT);
            break;
        case DEL_KEY:
            editorMoveCursor(ARROW_RIGHT);
            break;
        
        case PAGE_UP:
        case PAGE_DOWN:
            {
                if (c == PAGE_UP){
                    //Bring cursor to top of the screen. Keeping it's x value if the line length is the same/bigger
                    E.my = E.rowOffset;
                }else if (c == PAGE_DOWN){
                    //Bring cursor to bottom of screen. Keeping it's x value if the line length is the same/bigger
                    E.my = E.rowOffset + E.screenRow - 1;
                    if (E.my > E.numRows) E.my = E.numRows;
                }
                int t = E.screenRow;
                while(t--){
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
        case 'i':
        case 'I':
        case 'a':
        case 'A':
        case 'o':
        case 'O':
            enterInsertMode(c);
            break;
        default:

            break;
    }

    quit_times = COMETTEX_QUIT_TIMES;
}

void ProcessKeypressInsert(){
    static int quit_times = COMETTEX_QUIT_TIMES;

    int c = editorReadKey();

    switch(c){
        case '\r':
            editorInsertNewLine();
            break;
        case CTRL_KEY('q'):
            if (E.dirty && quit_times > 0){
                editorSetStatusMessage("WARNING!! File modified. Press Ctrl+Q %d more times to quit.", quit_times);
                quit_times--;
                return;
            }
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H",3);
            exit(0);
            break;
        
        case CTRL_KEY('s'):
            editorSave();
            break;

        case CTRL_KEY('f'):
            editorFind();
            break;

        case CTRL_KEY('x'):
            editorSave();
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H",3);
            exit(0);
            break;
        
        case HOME_KEY:
            E.mx = 0;
            break;
        case END_KEY:
            if (E.my < E.numRows){
                E.mx = E.row[E.my].size;
            }
            break;

        case BACKSPACE:
        case CTRL_KEY('h'):
        case DEL_KEY:
            if (c == DEL_KEY) editorMoveCursor(ARROW_RIGHT);
            editorDelChar();
            break;
        
        case PAGE_UP:
        case PAGE_DOWN:
            {
                if (c == PAGE_UP){
                    E.my = E.rowOffset;
                }else if (c == PAGE_DOWN){
                    E.my = E.rowOffset + E.screenRow - 1;
                    if (E.my > E.numRows) E.my = E.numRows;
                }
                int t = E.screenRow;
                while(t--){
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
        
        case CTRL_KEY('l'):
        case '\x1b':
            E.mode = 1;
            break;
        default:
            editorInsertChar(c);
            break;
    }

    quit_times = COMETTEX_QUIT_TIMES;
}

void initEditor(){
    E.mx = 0;
    E.my = 0;
    E.rx = 0;
    E.rowOffset = 0;
    E.colOffset = 0;
    E.numRows = 0;
    E.row = NULL;
    E.mode = 1;
    E.dirty = 0;
    E.filename = NULL;
    E.statusMsg[0] = '\0';
    E.statusMsg_time = 0;
    E.syntax = NULL;

    if (getWindowSize(&E.screenRow, &E.screenCol) == -1) die("getWindowSize");
    E.screenRow -= 2;
}

int main(int argc, char *argv[]){
    enableRawMode();
    //Set all variables needed to the default
    initEditor();
    //If they gave a file name open the file
    if (argc >= 2){
        editorOpen(argv[1]);
    }

    editorSetStatusMessage("HELP: Ctrl+S = save | CTRL+F find | Ctrl+Q = quit");

    while (1){
        //Refresh the screen every frame
        editorRefreshScreen();
        if (E.mode == MODE_NORMAL) {
            processKeypressNormal();
        } else {
            ProcessKeypressInsert();
        }
    }
    return 0;
}
