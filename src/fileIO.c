#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ops.h"
#include "CometTex.h"
#include "syntaxHighlighting.h"

#define COMETTEX_CONFIG_FILENAME "comettex.con"

//-------------------Function currently Does not Work----------------
char *searchConfigFile(char *n){
    FILE *f = fopen("src/comettex.con", "r");
    if (!f) die("fopen config:");

    char *l = NULL;
    size_t lCap = 0;
    ssize_t lLen;
    char p[124][124];
    int i = 0, c = 0, j = 0;
    while ((lLen = getline(&l, &lCap, f)) != -1){
        if (n[i] == ' ' || n[i] == '\0'){
            p[c][j] = '\0';
            j=0;
            c++;
        }else{
            p[c][j] = n[i];
            j++;
        }
        i++;
    }
    fclose(f);
    die("Couldn't find config defination");
}

char *editorRowsToString(int *buflen){
    int totlen = 0;
    for (int i = 0;i<E.numRows;i++){
        totlen += E.row[i].size + 1;
    }
    *buflen = totlen;

    char *buf = malloc(totlen);
    char *p = buf;
    for (int i = 0;i<E.numRows;i++){
        memcpy(p, E.row[i].chars, E.row[i].size);
        p += E.row[i].size;
        *p = '\n';
        p++;
    }
    return buf;
}

void editorOpen(char *filename){
    free(E.filename);
    E.filename = strdup(filename);

    editorSelectSyntaxHighlight();

    FILE *fp = fopen(filename, "r");
    if (!fp) die("fopen:");

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while ((linelen = getline(&line, &linecap, fp)) != -1){
        while(linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r')){
            linelen--;
        }
        editorInsertRow(E.numRows, line, linelen);
    }
    free(line);
    fclose(fp);
    E.dirty = 0;
}

void editorSave(){

    if (E.filename == NULL){
        E.filename = editorPrompt("Save as: %s (ESC to cancel", NULL);
        if (E.filename == NULL){
            editorSetStatusMessage("Save aborted");
            return;
        }
        editorSelectSyntaxHighlight();
    }


    int len;
    char *buf = editorRowsToString(&len);

    int fd = open(E.filename, O_RDWR | O_CREAT, 0644);
    if (fd != -1){
        if (ftruncate(fd, len) != -1){
            if (write(fd, buf, len) == len){
                close(fd);
                free(buf);
                E.dirty = 0;
                editorSetStatusMessage("%d bytes written to disk", len);
                return;
            }
        }
       close(fd);
    }

    free(buf);
    editorSetStatusMessage("Can't Save! I/O error %s", strerror(errno));
}