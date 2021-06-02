#include <unistd.h>
#include <stdlib.h>
#include "ops.h"
#include "fileIO.h"
#include "command.h"
#include "CometTex.h"

void commandPrompt(char *n){
    editorInsertChar(n[0]);
    switch(n[0]){
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
    }
}