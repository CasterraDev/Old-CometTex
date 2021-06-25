#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "syntaxHighlighting.h"

char *C_HL_extensions[] = {".c", ".h", ".cpp", NULL};
char *C_HL_keywords[] = {
    "if", "switch", "while", "for", "break", "continue", "return", "else", "struct", "union", "typedef", "static", "enum", "class", "case", 

    "int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|", "void|", NULL
};

struct editorSyntax HLDB[] = {
    {
        "c",
        C_HL_extensions,
        C_HL_keywords,
        "//", "/*", "*/",
        HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
    },
};

int isSeparator(int c){
    return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}

void editorUpdateSyntax(erow *row){
    row->hl = realloc(row->hl, row->rsize);
    memset(row->hl, HL_NORMAL, row->rsize);

    if (E.syntax == NULL) return;

    char **keywords = E.syntax->keywords;

    char *scs = E.syntax->singleCommentStart;
    char *mcs = E.syntax->multiCommentStart;
    char *mce = E.syntax->multiCommentEnd;

    int scsLen = scs ? strlen(scs) : 0;
    int mcsLen = mcs ? strlen(mcs) : 0;
    int mceLen = mce ? strlen(mce) : 0;
    
    int preSep = 1;
    int inString = 0;
    int inComment = (row->idx > 0 && E.row[row->idx - 1].hlOpenComment);

    int i = 0;
    while(i < row->rsize){
        char c = row->render[i];
        unsigned char preHL = (i > 0) ? row->hl[i - 1] : HL_NORMAL;

        if (scsLen && !inString && !inComment){
            if (!strncmp(&row->render[i], scs, scsLen)){
                memset(&row->hl[i], HL_COMMENT, row->rsize - i);
                break;
            }
        }

        if (mcsLen && mceLen && !inString){
            if (inComment){
                row->hl[i] = HL_MLCOMMENT;
                if (!strncmp(&row->render[i], mce, mceLen)){
                    memset(&row->hl[i], HL_MLCOMMENT, mceLen);
                    i += mceLen;
                    inComment = 0;
                    preSep = 1;
                    continue;
                }else{
                    i++;
                    continue;
                }
            }else if (!strncmp(&row->render[i], mcs, mcsLen)){
                memset(&row->hl[i], HL_MLCOMMENT, mcsLen);
                i += mcsLen;
                inComment = 1;
                continue;
            }
        }
        
        if (E.syntax->flags & HL_HIGHLIGHT_STRINGS){
            if (inString){
                row->hl[i] = HL_STRING;
                if (c == '\\' && i + 1 < row->rsize){
                    row->hl[i + 1] = HL_STRING;
                    i += 2;
                    continue;
                }
                if (c == inString) inString = 0;
                i++;
                preSep = 1;
                continue;
            }else{
                if (c == '"' || c == '\''){
                    inString = c;
                    row->hl[i] = HL_STRING;
                    i++;
                    continue;
                }
            }
        }
        
        if (E.syntax->flags & HL_HIGHLIGHT_NUMBERS){
            if (isdigit(c)){
                row->hl[i] = HL_NUMBER;
                i++;
                preSep = 0;
                continue;
            }
        }

        if (preSep){
            int j;
            for (j = 0; keywords[j]; j++){
                int klen = strlen(keywords[j]);
                int kw2 = keywords[j][klen - 1] == '|';
                if (kw2) klen--;

                if (!strncmp(&row->render[i], keywords[j], klen) && isSeparator(row->render[i + klen])){
                    memset(&row->hl[i], kw2 ? HL_KEYWORD2 : HL_KEYWORD1, klen);
                    i += klen;
                    break;
                }
            }
            if (keywords[j] != NULL){
                preSep = 0;
                continue;
            }
        }

        preSep = isSeparator(c);
        i++;
    }

    int changed = (row->hlOpenComment != inComment);
    row->hlOpenComment = inComment;
    if (changed && row->idx + 1 < E.numRows){
        editorUpdateSyntax(&E.row[row->idx + 1]);
    }
}

int editorSyntaxToColor(int hl){
    switch (hl){
        case HL_NUMBER: return 31; //Red
        case HL_KEYWORD1: return 34; //Blue
        case HL_STRING: return 33; //Yellow
        case HL_MATCH: return 36; //Cyan
        case HL_KEYWORD2: return 34; //Blue
        case HL_COMMENT:
        case HL_MLCOMMENT: return 32; //Green
        default: return 37; //White
    }
}

void editorSelectSyntaxHighlight(){
    E.syntax = NULL;
    if (E.filename == NULL) return;

    char *ext = strrchr(E.filename, '.');

    for (unsigned int i = 0;i<HLDB_ENTRIES;i++){
        struct editorSyntax *s = &HLDB[i];
        unsigned int j = 0;
        while(s->fileMatch[j]){
            int is_ext = (s->fileMatch[j][0] == '.');
            if ((is_ext && ext && !strcmp(ext, s->fileMatch[j])) || (!is_ext && strstr(E.filename, s->fileMatch[j]))){
                E.syntax = s;

                int fileRow;
                for (fileRow = 0;fileRow < E.numRows; fileRow++){
                    editorUpdateSyntax(&E.row[fileRow]);
                }

                return;
            }
            j++;
        }
    }
}