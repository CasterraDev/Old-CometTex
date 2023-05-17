#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "CometTex.h"
#include "syntaxHighlighting.h"
#include "ops.h"

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

void editorUpdateRow(editorConfig *ce, erow *row){
    int tabs = 0;
    for (int i = 0;i<row->size;i++){
        if (row->chars[i] == '\t') tabs++;
    }

    free(row->render);
    row->render = malloc(row->size + tabs*(COMETTEX_TAB_STOP) + 1);

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

    editorUpdateSyntax(ce, row);
}

void editorInsertRow(editorConfig *ce, int at, char *s, size_t len){
    if (at < 0 || at > ce->numRows) return;

    ce->row = realloc(ce->row, sizeof(erow) * (ce->numRows + 1));
    memmove(&ce->row[at + 1], &ce->row[at], sizeof(erow) * (ce->numRows - at));
    //Increment the below rows by one
    for (int j = at + 1; j <= ce->numRows;j++){
        ce->row[j].idx++;
    }

    ce->row[at].idx = at;

    ce->row[at].size = len;
    ce->row[at].chars = malloc(len + 1);
    memcpy(ce->row[at].chars, s, len);
    ce->row[at].chars[len] = '\0';

    ce->row[at].rsize = 0;
    ce->row[at].render = NULL;
    ce->row[at].hl = NULL;
    ce->row[at].hlOpenComment = 0;
    editorUpdateRow(ce, &ce->row[at]);

    ce->numRows++;
    ce->dirty++;
}

void editorFreeRow(erow *row){
    free(row->render);
    free(row->chars);
    free(row->hl);
}

void editorDelRow(editorConfig *ce, int at){
    if (at < 0 || at >= ce->numRows) return;
    editorFreeRow(&ce->row[at]);
    memmove(&ce->row[at], &ce->row[at + 1], sizeof(erow) * (ce->numRows - at - 1));
    //Decrement the below rows by one
    for (int j = at; j < ce->numRows - 1;j++) ce->row[j].idx--;
    ce->numRows--;
    ce->dirty++;
}

void editorRowInsertChar(editorConfig *ce, erow *row, int at, int c){
    if (at < 0 || at > row->size) at = row->size;
    row->chars = realloc(row->chars, row->size + 2);
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
    row->size++;
    row->chars[at] = c;
    editorUpdateRow(ce, row);
    ce->dirty++;
}

void editorRowAppendString(editorConfig *ce, erow *row, char *s, size_t len){
    row->chars = realloc(row->chars, row->size + len + 1);
    memcpy(&row->chars[row->size], s, len);
    row->size += len;
    row->chars[row->size] = '\0';
    editorUpdateRow(ce, row);
    ce->dirty++;
}

void editorRowDelChar(editorConfig *ce, erow *row, int at){
    if (at < 0 || at >= row->size) return;
    memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
    row->size--;
    editorUpdateRow(ce, row);
    ce->dirty++;
}

void editorDelChar(editorConfig *ce){
    if (ce->my == ce->numRows) return;
    if (ce->mx == 0 && ce->my == 0) return;

    erow *row = &ce->row[ce->my];
    if (ce->mx > 0){
        editorRowDelChar(ce, row, ce->mx - 1);
        ce->mx--;
    }else{
        ce->mx = ce->row[ce->my - 1].size;
        editorRowAppendString(ce, &ce->row[ce->my - 1], row->chars, row->size);
        editorDelRow(ce, ce->my);
        ce->my--;
    }
}

void editorInsertNewLine(editorConfig *ce){
    if (ce->mx == 0){
        editorInsertRow(&E,ce->my, "", 0);
    }else{
        erow *row = &ce->row[ce->my];
        editorInsertRow(ce,ce->my + 1, &row->chars[ce->mx], row->size - ce->mx);
        row = &ce->row[ce->my];
        row->size = ce->mx;
        row->chars[row->size] = '\0';
        editorUpdateRow(ce, row);
    }
    ce->my++;
    ce->mx = 0;
}

void editorInsertChar(editorConfig *ce, int c){
    if (ce->my == ce->numRows){
        editorInsertRow(&E,ce->numRows, "", 0);
    }
    editorRowInsertChar(ce, &ce->row[ce->my], ce->mx, c);
    ce->mx++;
}

