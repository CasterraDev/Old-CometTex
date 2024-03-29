#ifndef COMETTEX_C_
#define COMETTEX_C_

#include <termios.h>
#include <time.h>

#define COMETTEX_VERSION "0.0.1"
#define COMETTEX_QUIT_TIMES 3;
#define COMETTEX_TAB_STOP 4
#define CTRL_KEY(c) ((c) & 0x1f)

typedef struct erow {
    int idx;
    int size;
    int rsize;
    char *chars;
    char *render;
    unsigned char *hl;
    int hlOpenComment;
} erow;

typedef struct editorConfig{
    int mx,my;
    int rx;
    int colOffset;
    int rowOffset;
    int screenRow;
    int screenCol;
    int numRows;
    erow *row;
    int mode;
    int dirty;
    char *filename;
    char statusMsg[80];
    time_t statusMsg_time;
    struct editorSyntax *syntax;
    struct termios orignal_termios;
} editorConfig;

enum editorMode {
    MODE_INSERT = 0,
    MODE_NORMAL
};

enum editorKey{
    BACKSPACE = 127,
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

static editorConfig E;

void die(const char *s);
char *editorPrompt(char *prompt, void (*callback)(char *, int));
void editorSetStatusMessage(const char *fmt, ...);
void editorFind();
void initEditor();

#endif