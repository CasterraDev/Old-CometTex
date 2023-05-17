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

char *C_HL_importwords[] = {"#include", "#define"};

struct editorSyntax HLDB[] = {
    {
        "c",
        C_HL_extensions,
        C_HL_keywords,
        C_HL_importwords,
        "//", "/*", "*/",
        HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
    },
};

int isSeparator(int c){
    return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}

void editorUpdateSyntax(editorConfig *ce, erow *row){
    row->hl = realloc(row->hl, row->rsize);
    memset(row->hl, HL_NORMAL, row->rsize);

    if (ce->syntax == NULL) return;

    char **keywords = ce->syntax->keywords;
    char **importwords = ce->syntax->importwords;

    char *scs = ce->syntax->singleCommentStart;
    char *mcs = ce->syntax->multiCommentStart;
    char *mce = ce->syntax->multiCommentEnd;

    int scsLen = scs ? strlen(scs) : 0;
    int mcsLen = mcs ? strlen(mcs) : 0;
    int mceLen = mce ? strlen(mce) : 0;
    
    int preSep = 1;
    int inString = 0;
    int inComment = (row->idx > 0 && ce->row[row->idx - 1].hlOpenComment);

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

        //If we started a multiline comment and make sure we aren't in a string
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
        
        if (ce->syntax->flags & HL_HIGHLIGHT_STRINGS){
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
        //If a function is starting
        //Make the chars before it a certain color
        if (c == '('){
            int cnt = i-1;
            while(!isSeparator(row->render[cnt])){
                row->hl[cnt] = HL_FUNCTIONS;
                cnt--;
            }
        }
        
        if (ce->syntax->flags & HL_HIGHLIGHT_NUMBERS){
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

            int jk;
            for (jk = 0; importwords[jk];jk++){
                int iwLen = strlen(importwords[jk]);
                if (!strncmp(&row->render[i],importwords[jk],iwLen)){
                    memset(&row->hl[i],HL_IMPORTKEYWORDS,iwLen);
                    i += iwLen;
                    break;
                }
            }
        }

        preSep = isSeparator(c);
        i++;
    }

    int changed = (row->hlOpenComment != inComment);
    row->hlOpenComment = inComment;
    if (changed && row->idx + 1 < ce->numRows){
        editorUpdateSyntax(ce, &ce->row[row->idx + 1]);
    }
}

// int fromIdxToSep(int idx, erow *row){
//     int cnt = 1;
//     int i = idx;
//     int b = 1;
//     while(i > row->rsize && b){
//         if (isSeparator(&row->render[i])){
//             b = 0;
//         }else{
//             cnt++;
//         }
//         i++;
//     }
//     return cnt;
// }

int editorSyntaxToColor(int hl){
    switch (hl){
        case HL_NUMBER: return 31; //Red
        case HL_KEYWORD1: return 34; //Blue
        case HL_STRING: return 33; //Yellow
        case HL_MATCH: return 36; //Cyan
        case HL_KEYWORD2: return 34; //Blue
        case HL_COMMENT:
        case HL_MLCOMMENT: return 32; //Green
        case HL_FUNCTIONS: return 35;
        case HL_IMPORTKEYWORDS: return 34;
        default: return 37; //White
    }
}

void editorSelectSyntaxHighlight(editorConfig *ce){
    ce->syntax = NULL;
    if (ce->filename == NULL) return;

    char *ext = strrchr(ce->filename, '.');

    for (unsigned int i = 0;i<HLDB_ENTRIES;i++){
        struct editorSyntax *s = &HLDB[i];
        unsigned int j = 0;
        while(s->fileMatch[j]){
            int is_ext = (s->fileMatch[j][0] == '.');
            if ((is_ext && ext && !strcmp(ext, s->fileMatch[j])) || (!is_ext && strstr(ce->filename, s->fileMatch[j]))){
                ce->syntax = s;

                int fileRow;
                for (fileRow = 0;fileRow < ce->numRows; fileRow++){
                    editorUpdateSyntax(ce, &ce->row[fileRow]);
                }

                return;
            }
            j++;
        }
    }
}