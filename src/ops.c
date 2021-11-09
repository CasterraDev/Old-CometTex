#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "CometTex.h"
#include "syntaxHighlighting.h"

//Row MouseX to RowX
int rowMxToRx(erow *row, int mx){
    int rx = 0;
    for (int i = 0;i<mx;i++){
        if (row->chars[i] == '\t'){
            rx += (COMETTEX_TAB_STOP - 1) - (rx % COMETTEX_TAB_STOP);
        }
        rx++;
    }
    return rx;
}

//Row RowX to MouseX
int rowRxtoMx(erow *row, int rx){
    int cur_rx = 0;
    int mx;
    for (mx = 0;mx < row->size;mx++){
        if (row->chars[mx] == '\t'){
            cur_rx += (COMETTEX_TAB_STOP - 1) - (cur_rx % COMETTEX_TAB_STOP);
        }
        cur_rx++;

        if (cur_rx > rx) return mx;
    }
    return mx;
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

    editorUpdateSyntax(row);
}

void editorInsertRow(int at, char *s, size_t len){
    if (at < 0 || at > E.numRows) return;

    E.row = realloc(E.row, sizeof(erow) * (E.numRows + 1));
    memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numRows - at));
    //Increment the below rows by one
    for (int j = at + 1; j <= E.numRows;j++) E.row[j].idx++;

    E.row[at].idx = at;

    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';

    E.row[at].rsize = 0;
    E.row[at].render = NULL;
    E.row[at].hl = NULL;
    E.row[at].hlOpenComment = 0;
    editorUpdateRow(&E.row[at]);

    E.numRows++;
    E.dirty++;
}

void editorFreeRow(erow *row){
    free(row->render);
    free(row->chars);
    free(row->hl);
}

void editorDelRow(int at){
    if (at < 0 || at >= E.numRows) return;
    editorFreeRow(&E.row[at]);
    memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numRows - at - 1));
    //Decrement the below rows by one
    for (int j = at; j < E.numRows - 1;j++) E.row[j].idx--;
    E.numRows--;
    E.dirty++;
}

void editorRowInsertChar(erow *row, int at, int c){
    if (at < 0 || at > row->size) at = row->size;
    row->chars = realloc(row->chars, row->size + 2);
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
    row->size++;
    row->chars[at] = c;
    editorUpdateRow(row);
    E.dirty++;
}

void editorRowAppendString(erow *row, char *s, size_t len){
    row->chars = realloc(row->chars, row->size + len + 1);
    memcpy(&row->chars[row->size], s, len);
    row->size += len;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
    E.dirty++;
}

void editorRowDelChar(erow *row, int at){
    if (at < 0 || at >= row->size) return;
    memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
    row->size--;
    editorUpdateRow(row);
    E.dirty++;
}

void editorDelChar(){
    if (E.my == E.numRows) return;
    if (E.mx == 0 && E.my == 0) return;

    erow *row = &E.row[E.my];
    if (E.mx > 0){
        editorRowDelChar(row, E.mx - 1);
        E.mx--;
    }else{
        E.mx = E.row[E.my - 1].size;
        editorRowAppendString(&E.row[E.my - 1], row->chars, row->size);
        editorDelRow(E.my);
        E.my--;
    }
}

void editorInsertNewLine(){
    if (E.mx == 0){
        editorInsertRow(E.my, "", 0);
    }else{
        erow *row = &E.row[E.my];
        editorInsertRow(E.my + 1, &row->chars[E.mx], row->size - E.mx);
        row = &E.row[E.my];
        row->size = E.mx;
        row->chars[row->size] = '\0';
        editorUpdateRow(row);
    }
    E.my++;
    E.mx = 0;
}

void editorInsertChar(int c){
    if (E.my == E.numRows){
        editorInsertRow(E.numRows, "", 0);
    }
    editorRowInsertChar(&E.row[E.my], E.mx, c);
    E.mx++;
}

