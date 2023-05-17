#ifndef RAWMODE_C_
#define RAWMODE_C_
#include "CometTex.h"

void disableRawMode(editorConfig *ce);
void enableRawMode(editorConfig *ce);
int editorReadKey();
int getCursorPos(int *rows,int *cols);
int getWindowSize(int *rows,int *cols);

#endif