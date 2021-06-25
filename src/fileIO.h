#ifndef FILEIO_C_
#define FILEIO_C_

char *editorRowsToString(int *buflen);
void editorOpen(char *filename);
void editorSave();
char *searchConfigFile(char *n);

#endif