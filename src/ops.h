#ifndef OPS_C_
#define OPS_C_
#include "CometTex.h"


int rowMxToRx(erow *row, int mx);

int rowRxtoMx(erow *row, int rx);

void editorUpdateRow(editorConfig *ce, erow *row);

void editorInsertRow(editorConfig *ce, int at, char *s, size_t len);

void editorFreeRow(erow *row);

void editorRowInsertChar(editorConfig *ce, erow *row, int at, int c);

void editorRowAppendString(editorConfig *ce, erow *row, char *s, size_t len);

void editorRowDelChar(editorConfig *ce, erow *row, int at);

void editorDelChar(editorConfig *ce);

void editorInsertNewLine(editorConfig *ce);

void editorInsertChar(editorConfig *ce, int c);

void editorDelRow(editorConfig *ce, int at);

#endif