#ifndef FILEIO_C_
#define FILEIO_C_

int getSubString(char* src,char* dest, int from, int to);
char *editorRowsToString(int *buflen);
void editorOpen(char *filename);
void editorSave();
char *searchConfigFile(char *n);

#endif