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
// char searchConfigFile(char* _n){
//     FILE *f = fopen("src/comettex.con", "r");
//     if (!f) die("fopen config:");

//     char *result = NULL;
//     char *l = NULL;
//     size_t lCap = 0;
//     ssize_t lLen;
//     char p[124][124];
//     int i = 0, c = 0, j = 0;
//     while ((lLen = getline(&l, &lCap, f)) != -1){
//         if (!strncmp(l,_n,strlen(_n))){
//             int h = 1;
//             int len = 1;
//             int found = 0;
//             while (l[strlen(_n) + len] != '\0'){
//                 if (l[strlen(_n) + len] != ':' && !found){
//                     h++;
//                 }else{
//                     found = 1;
//                 }
//                 len++;
//             }
//             //PROBLEM
//             result = getSubString(l,result,h,len);
//         }
//         if (*l == ' ' || *l == '\0'){
//             p[c][j] = '\0';
//             j=0;
//             c++;
//         }else{
//             p[c][j] = _n[i];
//             j++;
//         }
//         i++;
//     }
//     fclose(f);
//     die("Couldn't find config defination");
// }

int getSubString(char* src,char* dest, int from, int to){
    int length = 0;
    int i=0,j=0;
    length = strlen(src);

    if (from < 0 || from > length){
        printf("Invaild from value");
    }

    if (to > length){
        printf("Invalid to value");
    }

    for(i=from,j=0;i<=to;i++,j++){
        dest[j]=src[i];
    }

    dest[j] = '\0';

    return 0;
}

/*
 Converts the array of erow structs into one single string with \n separating them
 @returns *variable expecting you to free the memory
*/
char *editorRowsToString(editorConfig *ce, int *_buflen){
    int totlen = 0;
    for (int i = 0;i<ce->numRows;i++){
        //Adding 1 to the end because of the \n we are about to add to each line
        totlen += ce->row[i].size + 1;
    }
    *_buflen = totlen;
    //Allocate the memory needed
    char *buf = malloc(totlen);

    char *p = buf;
    for (int i = 0;i<ce->numRows;i++){
        memcpy(p, ce->row[i].chars, ce->row[i].size);
        p += ce->row[i].size;
        *p = '\n';
        p++;
    }
    return buf;
}

void editorOpen(editorConfig *ce, char *_filename){
    FILE *fp;

    ce->dirty = 0;
    free(ce->filename);
    size_t fnlen = strlen(_filename)+1;
    ce->filename = malloc(fnlen);
    memcpy(ce->filename,_filename,fnlen);

    fp = fopen(_filename,"r");
    if (!fp) {
        if (errno != ENOENT) {
            perror("Opening file");
            exit(1);
        }
    }

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while((linelen = getline(&line,&linecap,fp)) != -1) {
        if (linelen && (line[linelen-1] == '\n' || line[linelen-1] == '\r'))
            line[--linelen] = '\0';
        editorInsertRow(ce, ce->numRows,line,linelen);
    }
    free(line);
    fclose(fp);
    ce->dirty = 0;
}

void editorSave(editorConfig *ce){
    if (ce->filename == NULL){
        ce->filename = editorPrompt("Save as: %s (ESC to cancel", NULL);
        if (ce->filename == NULL){
            editorSetStatusMessage("Save aborted");
            return;
        }
        editorSelectSyntaxHighlight(ce);
    }


    int len;
    char *buf = editorRowsToString(ce,&len);

    int fd = open(ce->filename, O_RDWR | O_CREAT, 0644);
    if (fd != -1){
        if (ftruncate(fd, len) != -1){
            if (write(fd, buf, len) == len){
                close(fd);
                free(buf);
                ce->dirty = 0;
                editorSetStatusMessage("%d bytes written to disk", len);
                return;
            }
        }
       close(fd);
    }

    free(buf);
    editorSetStatusMessage("Can't Save! I/O error %s", strerror(errno));
}