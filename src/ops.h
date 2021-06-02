#ifndef OPS_C_
#define OPS_C_
#include "CometTex.h"


int rowMxToRx(erow *row, int mx);

int rowRxtoMx(erow *row, int rx);

void editorUpdateRow(erow *row);

void editorInsertRow(int at, char *s, size_t len);

void editorFreeRow(erow *row);

void editorRowInsertChar(erow *row, int at, int c);

void editorRowAppendString(erow *row, char *s, size_t len);

void editorRowDelChar(erow *row, int at);

void editorDelChar();

void editorInsertNewLine();

void editorInsertChar(int c);

void editorDelRow(int at);

#endif