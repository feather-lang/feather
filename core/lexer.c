/*
 * lexer.c - TCL Lexer (Pure Tokenization)
 *
 * Tokenizes TCL source into words respecting quoting rules.
 * No allocation - operates on input buffer, outputs indices/pointers.
 */

#include "internal.h"

/* ========================================================================
 * String Helpers (no libc)
 * ======================================================================== */

int tclStrncmp(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) {
            return (unsigned char)a[i] - (unsigned char)b[i];
        }
        if (a[i] == '\0') {
            return 0;
        }
    }
    return 0;
}

size_t tclStrlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

/* ========================================================================
 * Lexer Initialization
 * ======================================================================== */

void tclLexerInit(TclLexer *lex, const char *script, size_t len) {
    lex->script = script;
    lex->pos = script;
    lex->end = script + len;
    lex->line = 1;
}

/* ========================================================================
 * Whitespace and Position Helpers
 * ======================================================================== */

void tclLexerSkipSpace(TclLexer *lex) {
    while (lex->pos < lex->end) {
        if (*lex->pos == ' ' || *lex->pos == '\t') {
            lex->pos++;
        } else if (*lex->pos == '\\' && lex->pos + 1 < lex->end && lex->pos[1] == '\n') {
            /* Backslash-newline acts as whitespace */
            lex->pos += 2;
            lex->line++;
        } else {
            break;
        }
    }
}

void tclLexerSkipLine(TclLexer *lex) {
    while (lex->pos < lex->end && *lex->pos != '\n') {
        lex->pos++;
    }
    if (lex->pos < lex->end && *lex->pos == '\n') {
        lex->pos++;
        lex->line++;
    }
}

int tclLexerAtCommandEnd(TclLexer *lex) {
    if (lex->pos >= lex->end) return 1;
    return (*lex->pos == '\n' || *lex->pos == ';');
}

int tclLexerAtComment(TclLexer *lex) {
    return (lex->pos < lex->end && *lex->pos == '#');
}

int tclLexerAtEnd(TclLexer *lex) {
    return (lex->pos >= lex->end);
}

/* Advance past command terminator */
void tclLexerSkipCommandEnd(TclLexer *lex) {
    if (lex->pos < lex->end) {
        if (*lex->pos == '\n') {
            lex->pos++;
            lex->line++;
        } else if (*lex->pos == ';') {
            lex->pos++;
        }
    }
}

/* ========================================================================
 * Word Parsing
 * ======================================================================== */

/* Parse a brace-quoted word: {content} */
static int lexBraces(TclLexer *lex, TclWord *word, TclInterp *interp) {
    int depth = 1;
    const char *start = lex->pos;  /* After opening { */
    int startLine = lex->line;

    while (lex->pos < lex->end && depth > 0) {
        if (*lex->pos == '\\' && lex->pos + 1 < lex->end) {
            /* Skip escaped char, including \{ and \} */
            if (lex->pos[1] == '\n') lex->line++;
            lex->pos += 2;
        } else if (*lex->pos == '{') {
            depth++;
            lex->pos++;
        } else if (*lex->pos == '}') {
            depth--;
            if (depth > 0) lex->pos++;
        } else {
            if (*lex->pos == '\n') lex->line++;
            lex->pos++;
        }
    }

    if (depth != 0) {
        if (interp) {
            tclSetError(interp, "missing close-brace", -1);
        }
        return -1;
    }

    word->start = start;
    word->len = lex->pos - start;
    word->type = TCL_WORD_BRACES;
    word->line = startLine;

    lex->pos++;  /* Skip closing } */
    return 0;
}

/* Parse a double-quoted word: "content" */
static int lexQuotes(TclLexer *lex, TclWord *word, TclInterp *interp) {
    const char *start = lex->pos;  /* After opening " */
    int startLine = lex->line;

    while (lex->pos < lex->end && *lex->pos != '"') {
        if (*lex->pos == '\\' && lex->pos + 1 < lex->end) {
            if (lex->pos[1] == '\n') lex->line++;
            lex->pos += 2;
        } else {
            if (*lex->pos == '\n') lex->line++;
            lex->pos++;
        }
    }

    if (lex->pos >= lex->end || *lex->pos != '"') {
        if (interp) {
            tclSetError(interp, "missing \"", -1);
        }
        return -1;
    }

    word->start = start;
    word->len = lex->pos - start;
    word->type = TCL_WORD_QUOTES;
    word->line = startLine;

    lex->pos++;  /* Skip closing " */
    return 0;
}

/* Parse a bare (unquoted) word */
static int lexBareWord(TclLexer *lex, TclWord *word) {
    const char *start = lex->pos;
    int startLine = lex->line;

    while (lex->pos < lex->end) {
        char c = *lex->pos;

        /* Word terminators */
        if (c == ' ' || c == '\t' || c == '\n' || c == ';') {
            break;
        }

        /* Backslash handling */
        if (c == '\\' && lex->pos + 1 < lex->end) {
            if (lex->pos[1] == '\n') {
                /* Backslash-newline ends word (acts as whitespace) */
                break;
            } else {
                /* Skip escaped character */
                lex->pos += 2;
                continue;
            }
        }

        /* ${varname} - scan to matching } */
        if (c == '$' && lex->pos + 1 < lex->end && lex->pos[1] == '{') {
            lex->pos += 2;
            while (lex->pos < lex->end && *lex->pos != '}') {
                lex->pos++;
            }
            if (lex->pos < lex->end) lex->pos++;  /* Skip } */
            continue;
        }

        /* These end a bare word unless escaped */
        if (c == '"' || c == '{' || c == '}') {
            break;
        }

        /* [command] substitution - scan to matching ] */
        if (c == '[') {
            int depth = 1;
            lex->pos++;
            while (lex->pos < lex->end && depth > 0) {
                if (*lex->pos == '[') depth++;
                else if (*lex->pos == ']') depth--;
                if (*lex->pos == '\n') lex->line++;
                if (depth > 0) lex->pos++;
            }
            if (lex->pos < lex->end) lex->pos++;  /* Skip ] */
            continue;
        }

        lex->pos++;
    }

    word->start = start;
    word->len = lex->pos - start;
    word->type = TCL_WORD_BARE;
    word->line = startLine;

    return 0;
}

/* ========================================================================
 * Main Lexer Entry Point
 * ======================================================================== */

int tclLexerNextWord(TclLexer *lex, TclWord *word, TclInterp *interp) {
    /* Skip leading whitespace */
    tclLexerSkipSpace(lex);

    /* Check for end of command or input */
    if (tclLexerAtEnd(lex) || tclLexerAtCommandEnd(lex)) {
        return 1;  /* No more words in this command */
    }

    char c = *lex->pos;

    if (c == '{') {
        lex->pos++;  /* Skip opening { */
        return lexBraces(lex, word, interp);
    } else if (c == '"') {
        lex->pos++;  /* Skip opening " */
        return lexQuotes(lex, word, interp);
    } else {
        return lexBareWord(lex, word);
    }
}
