#ifndef SYNTAX_HIGHLIGHTING_C_
#define SYNTAX_HIGHLIGHTING_C_

#include "CometTex.h"

#define HL_HIGHLIGHT_NUMBERS (1<<0)
#define HL_HIGHLIGHT_STRINGS (1<<1)

struct editorSyntax{
    char *fileType;
    char **fileMatch;
    char **keywords;
    char **importwords;
    char *singleCommentStart;
    char *multiCommentStart;
    char *multiCommentEnd;
    int flags;
};

enum editorHighlight{
    HL_NORMAL = 0,
    HL_NUMBER,
    HL_MATCH,
    HL_STRING,
    HL_COMMENT,
    HL_KEYWORD1,
    HL_KEYWORD2,
    HL_MLCOMMENT,
    HL_FUNCTIONS,
    HL_IMPORTKEYWORDS,
};

extern char *C_HL_extensions[];

extern struct editorSyntax HLDB[];

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

void editorUpdateSyntax(editorConfig *ce, erow *row);
int fromIdxToSep(int idx, erow *row);
int editorSyntaxToColor(int hl);
void editorSelectSyntaxHighlight(editorConfig *ce);

#endif