#ifndef FILEIO_C_
#define FILEIO_C_
#include "CometTex.h"

int getSubString(char* src,char* dest, int from, int to);
char *editorRowsToString(editorConfig *ce, int *buflen);
void editorOpen(editorConfig *ce, char *filename);
void editorSave(editorConfig *ce);
char *searchConfigFile(char *n);

#endif