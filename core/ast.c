/*
 * ast.c - TCL Abstract Syntax Tree Implementation
 *
 * Implements AST node allocation and construction helpers.
 * All nodes are allocated from the host-provided arena.
 */

#include "ast.h"
#include "internal.h"

/* ========================================================================
 * Node Allocation
 * ======================================================================== */

TclAstNode *tclAstNew(TclInterp *interp, void *arena, TclNodeType type, int line) {
    const TclHost *host = interp->host;

    TclAstNode *node = host->arenaAlloc(arena, sizeof(TclAstNode), sizeof(void*));
    if (!node) {
        return NULL;
    }

    node->type = type;
    node->line = line;

    /* Zero-initialize the union */
    for (size_t i = 0; i < sizeof(node->u); i++) {
        ((char*)&node->u)[i] = 0;
    }

    return node;
}

/* ========================================================================
 * Convenience Constructors
 * ======================================================================== */

TclAstNode *tclAstScript(TclInterp *interp, void *arena, int line) {
    TclAstNode *node = tclAstNew(interp, arena, TCL_NODE_SCRIPT, line);
    if (node) {
        node->u.script.cmds = NULL;
        node->u.script.count = 0;
    }
    return node;
}

TclAstNode *tclAstCommand(TclInterp *interp, void *arena, int line) {
    TclAstNode *node = tclAstNew(interp, arena, TCL_NODE_COMMAND, line);
    if (node) {
        node->u.command.words = NULL;
        node->u.command.count = 0;
    }
    return node;
}

TclAstNode *tclAstWord(TclInterp *interp, void *arena, int line) {
    TclAstNode *node = tclAstNew(interp, arena, TCL_NODE_WORD, line);
    if (node) {
        node->u.word.parts = NULL;
        node->u.word.count = 0;
    }
    return node;
}

TclAstNode *tclAstLiteral(TclInterp *interp, void *arena, const char *value, size_t len, int line) {
    TclAstNode *node = tclAstNew(interp, arena, TCL_NODE_LITERAL, line);
    if (node) {
        /* Copy the string into the arena */
        char *copy = interp->host->arenaAlloc(arena, len + 1, 1);
        if (!copy) {
            return NULL;
        }
        for (size_t i = 0; i < len; i++) {
            copy[i] = value[i];
        }
        copy[len] = '\0';

        node->u.literal.value = copy;
        node->u.literal.len = len;
    }
    return node;
}

TclAstNode *tclAstVarSimple(TclInterp *interp, void *arena, const char *name, size_t len, int line) {
    TclAstNode *node = tclAstNew(interp, arena, TCL_NODE_VAR_SIMPLE, line);
    if (node) {
        /* Copy the name into the arena */
        char *copy = interp->host->arenaAlloc(arena, len + 1, 1);
        if (!copy) {
            return NULL;
        }
        for (size_t i = 0; i < len; i++) {
            copy[i] = name[i];
        }
        copy[len] = '\0';

        node->u.varSimple.name = copy;
        node->u.varSimple.len = len;
    }
    return node;
}

TclAstNode *tclAstVarArray(TclInterp *interp, void *arena, const char *name, size_t nameLen, TclAstNode *index, int line) {
    TclAstNode *node = tclAstNew(interp, arena, TCL_NODE_VAR_ARRAY, line);
    if (node) {
        /* Copy the name into the arena */
        char *copy = interp->host->arenaAlloc(arena, nameLen + 1, 1);
        if (!copy) {
            return NULL;
        }
        for (size_t i = 0; i < nameLen; i++) {
            copy[i] = name[i];
        }
        copy[nameLen] = '\0';

        node->u.varArray.name = copy;
        node->u.varArray.nameLen = nameLen;
        node->u.varArray.index = index;
    }
    return node;
}

TclAstNode *tclAstCmdSubst(TclInterp *interp, void *arena, TclAstNode *script, int line) {
    TclAstNode *node = tclAstNew(interp, arena, TCL_NODE_CMD_SUBST, line);
    if (node) {
        node->u.cmdSubst.script = script;
    }
    return node;
}

TclAstNode *tclAstBackslash(TclInterp *interp, void *arena, const char *value, size_t len, int line) {
    TclAstNode *node = tclAstNew(interp, arena, TCL_NODE_BACKSLASH, line);
    if (node) {
        /* Copy the resolved value into the arena */
        char *copy = interp->host->arenaAlloc(arena, len + 1, 1);
        if (!copy) {
            return NULL;
        }
        for (size_t i = 0; i < len; i++) {
            copy[i] = value[i];
        }
        copy[len] = '\0';

        node->u.backslash.value = copy;
        node->u.backslash.len = len;
    }
    return node;
}

TclAstNode *tclAstExpand(TclInterp *interp, void *arena, TclAstNode *word, int line) {
    TclAstNode *node = tclAstNew(interp, arena, TCL_NODE_EXPAND, line);
    if (node) {
        node->u.expand.word = word;
    }
    return node;
}

/* ========================================================================
 * Child Node Management
 *
 * Uses a simple growth strategy: allocate new array, copy old entries.
 * This is fine for arena allocation since we don't need to free old arrays.
 * ======================================================================== */

/* Initial capacity for child arrays */
enum { AST_INITIAL_CAPACITY = 8 };

int tclAstScriptAddCmd(TclInterp *interp, void *arena, TclAstNode *script, TclAstNode *cmd) {
    if (script->type != TCL_NODE_SCRIPT) {
        return -1;
    }

    const TclHost *host = interp->host;
    int count = script->u.script.count;

    /* Allocate new array with space for one more */
    TclAstNode **newCmds = host->arenaAlloc(arena, (count + 1) * sizeof(TclAstNode*), sizeof(void*));
    if (!newCmds) {
        return -1;
    }

    /* Copy existing entries */
    for (int i = 0; i < count; i++) {
        newCmds[i] = script->u.script.cmds[i];
    }

    /* Add new entry */
    newCmds[count] = cmd;

    script->u.script.cmds = newCmds;
    script->u.script.count = count + 1;

    return 0;
}

int tclAstCommandAddWord(TclInterp *interp, void *arena, TclAstNode *command, TclAstNode *word) {
    if (command->type != TCL_NODE_COMMAND) {
        return -1;
    }

    const TclHost *host = interp->host;
    int count = command->u.command.count;

    /* Allocate new array with space for one more */
    TclAstNode **newWords = host->arenaAlloc(arena, (count + 1) * sizeof(TclAstNode*), sizeof(void*));
    if (!newWords) {
        return -1;
    }

    /* Copy existing entries */
    for (int i = 0; i < count; i++) {
        newWords[i] = command->u.command.words[i];
    }

    /* Add new entry */
    newWords[count] = word;

    command->u.command.words = newWords;
    command->u.command.count = count + 1;

    return 0;
}

int tclAstWordAddPart(TclInterp *interp, void *arena, TclAstNode *word, TclAstNode *part) {
    if (word->type != TCL_NODE_WORD) {
        return -1;
    }

    const TclHost *host = interp->host;
    int count = word->u.word.count;

    /* Allocate new array with space for one more */
    TclAstNode **newParts = host->arenaAlloc(arena, (count + 1) * sizeof(TclAstNode*), sizeof(void*));
    if (!newParts) {
        return -1;
    }

    /* Copy existing entries */
    for (int i = 0; i < count; i++) {
        newParts[i] = word->u.word.parts[i];
    }

    /* Add new entry */
    newParts[count] = part;

    word->u.word.parts = newParts;
    word->u.word.count = count + 1;

    return 0;
}

/* ========================================================================
 * AST Parsing
 *
 * Parse TCL source into an AST. Uses the existing lexer for tokenization
 * but builds tree nodes instead of TclWord arrays.
 * ======================================================================== */

/* Forward declarations for recursive parsing */
static TclAstNode *parseScript(TclInterp *interp, void *arena, const char *script, size_t len, int *lineOut);
static TclAstNode *parseCommand(TclInterp *interp, void *arena, TclLexer *lex);
static TclAstNode *parseWordContent(TclInterp *interp, void *arena, const char *str, size_t len, int line, int quoted);

/* Check if character is a valid variable name character */
static int isVarNameChar(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') ||
           c == '_';
}

/* Parse a backslash escape sequence, return length consumed */
static int parseBackslashEscape(const char *p, const char *end, char *out, size_t *outLen) {
    if (p >= end || *p != '\\') {
        return 0;
    }

    p++; /* Skip backslash */
    if (p >= end) {
        out[0] = '\\';
        *outLen = 1;
        return 1;
    }

    switch (*p) {
        case 'a': out[0] = '\a'; *outLen = 1; return 2;
        case 'b': out[0] = '\b'; *outLen = 1; return 2;
        case 'f': out[0] = '\f'; *outLen = 1; return 2;
        case 'n': out[0] = '\n'; *outLen = 1; return 2;
        case 'r': out[0] = '\r'; *outLen = 1; return 2;
        case 't': out[0] = '\t'; *outLen = 1; return 2;
        case 'v': out[0] = '\v'; *outLen = 1; return 2;
        case '\\': out[0] = '\\'; *outLen = 1; return 2;
        case '"': out[0] = '"'; *outLen = 1; return 2;
        case '{': out[0] = '{'; *outLen = 1; return 2;
        case '}': out[0] = '}'; *outLen = 1; return 2;
        case '[': out[0] = '['; *outLen = 1; return 2;
        case ']': out[0] = ']'; *outLen = 1; return 2;
        case '$': out[0] = '$'; *outLen = 1; return 2;
        case '\n':
            /* Backslash-newline: skip newline and following whitespace */
            out[0] = ' ';
            *outLen = 1;
            p++;
            while (p < end && (*p == ' ' || *p == '\t')) p++;
            return (int)(p - (end - (*outLen + 1))) + 1;
        case 'x': {
            /* Hex escape: \xNN */
            p++;
            int val = 0;
            int digits = 0;
            while (p < end && digits < 2) {
                char c = *p;
                if (c >= '0' && c <= '9') val = val * 16 + (c - '0');
                else if (c >= 'a' && c <= 'f') val = val * 16 + (c - 'a' + 10);
                else if (c >= 'A' && c <= 'F') val = val * 16 + (c - 'A' + 10);
                else break;
                p++;
                digits++;
            }
            if (digits > 0) {
                out[0] = (char)val;
                *outLen = 1;
                return 2 + digits;
            }
            out[0] = 'x';
            *outLen = 1;
            return 2;
        }
        case 'u': {
            /* Unicode escape: \uNNNN */
            p++;
            int val = 0;
            int digits = 0;
            while (p < end && digits < 4) {
                char c = *p;
                if (c >= '0' && c <= '9') val = val * 16 + (c - '0');
                else if (c >= 'a' && c <= 'f') val = val * 16 + (c - 'a' + 10);
                else if (c >= 'A' && c <= 'F') val = val * 16 + (c - 'A' + 10);
                else break;
                p++;
                digits++;
            }
            if (digits == 4) {
                /* Simple UTF-8 encoding */
                if (val < 0x80) {
                    out[0] = (char)val;
                    *outLen = 1;
                } else if (val < 0x800) {
                    out[0] = (char)(0xC0 | (val >> 6));
                    out[1] = (char)(0x80 | (val & 0x3F));
                    *outLen = 2;
                } else {
                    out[0] = (char)(0xE0 | (val >> 12));
                    out[1] = (char)(0x80 | ((val >> 6) & 0x3F));
                    out[2] = (char)(0x80 | (val & 0x3F));
                    *outLen = 3;
                }
                return 2 + digits;
            }
            out[0] = 'u';
            *outLen = 1;
            return 2;
        }
        default:
            /* Check for octal */
            if (*p >= '0' && *p <= '7') {
                int val = 0;
                int digits = 0;
                while (p < end && digits < 3 && *p >= '0' && *p <= '7') {
                    val = val * 8 + (*p - '0');
                    p++;
                    digits++;
                }
                out[0] = (char)val;
                *outLen = 1;
                return 1 + digits;
            }
            /* Unknown escape - keep as-is */
            out[0] = *p;
            *outLen = 1;
            return 2;
    }
}

/* Parse the content of a word (inside quotes or bare) into word parts */
static TclAstNode *parseWordContent(TclInterp *interp, void *arena, const char *str, size_t len, int line, int quoted) {
    (void)quoted; /* Currently unused, may be needed for context-specific parsing */
    TclAstNode *word = tclAstWord(interp, arena, line);
    if (!word) return NULL;

    const char *p = str;
    const char *end = str + len;
    const char *literalStart = p;

    while (p < end) {
        if (*p == '$') {
            /* Variable substitution */

            /* Flush pending literal */
            if (p > literalStart) {
                TclAstNode *lit = tclAstLiteral(interp, arena, literalStart, p - literalStart, line);
                if (!lit || tclAstWordAddPart(interp, arena, word, lit) != 0) return NULL;
            }

            p++; /* Skip $ */

            if (p < end && *p == '{') {
                /* ${varname} form */
                p++;
                const char *nameStart = p;
                int depth = 1;
                while (p < end && depth > 0) {
                    if (*p == '{') depth++;
                    else if (*p == '}') depth--;
                    if (depth > 0) p++;
                }
                size_t nameLen = p - nameStart;
                if (p < end) p++; /* Skip closing } */

                TclAstNode *var = tclAstVarSimple(interp, arena, nameStart, nameLen, line);
                if (!var || tclAstWordAddPart(interp, arena, word, var) != 0) return NULL;
            } else if (p < end && isVarNameChar(*p)) {
                /* $varname form */
                const char *nameStart = p;
                while (p < end && isVarNameChar(*p)) p++;
                size_t nameLen = p - nameStart;

                /* Check for array index */
                if (p < end && *p == '(') {
                    p++; /* Skip ( */
                    const char *indexStart = p;
                    int depth = 1;
                    while (p < end && depth > 0) {
                        if (*p == '(') depth++;
                        else if (*p == ')') depth--;
                        if (depth > 0) p++;
                    }
                    size_t indexLen = p - indexStart;
                    if (p < end) p++; /* Skip closing ) */

                    /* Parse index as a word (may contain substitutions) */
                    TclAstNode *indexWord = parseWordContent(interp, arena, indexStart, indexLen, line, 1);
                    TclAstNode *var = tclAstVarArray(interp, arena, nameStart, nameLen, indexWord, line);
                    if (!var || tclAstWordAddPart(interp, arena, word, var) != 0) return NULL;
                } else {
                    TclAstNode *var = tclAstVarSimple(interp, arena, nameStart, nameLen, line);
                    if (!var || tclAstWordAddPart(interp, arena, word, var) != 0) return NULL;
                }
            } else {
                /* Lone $ - treat as literal */
                TclAstNode *lit = tclAstLiteral(interp, arena, "$", 1, line);
                if (!lit || tclAstWordAddPart(interp, arena, word, lit) != 0) return NULL;
            }
            literalStart = p;

        } else if (*p == '[') {
            /* Command substitution */

            /* Flush pending literal */
            if (p > literalStart) {
                TclAstNode *lit = tclAstLiteral(interp, arena, literalStart, p - literalStart, line);
                if (!lit || tclAstWordAddPart(interp, arena, word, lit) != 0) return NULL;
            }

            p++; /* Skip [ */
            const char *cmdStart = p;
            int depth = 1;
            while (p < end && depth > 0) {
                if (*p == '[') depth++;
                else if (*p == ']') depth--;
                else if (*p == '{') {
                    /* Skip braced content */
                    int braceDepth = 1;
                    p++;
                    while (p < end && braceDepth > 0) {
                        if (*p == '{') braceDepth++;
                        else if (*p == '}') braceDepth--;
                        p++;
                    }
                    continue;
                } else if (*p == '"') {
                    /* Skip quoted content */
                    p++;
                    while (p < end && *p != '"') {
                        if (*p == '\\' && p + 1 < end) p++;
                        p++;
                    }
                    if (p < end) p++;
                    continue;
                }
                if (depth > 0) p++;
            }
            size_t cmdLen = p - cmdStart;
            if (p < end) p++; /* Skip closing ] */

            /* Parse the command substitution as a script */
            int dummy;
            TclAstNode *script = parseScript(interp, arena, cmdStart, cmdLen, &dummy);
            TclAstNode *cmdSubst = tclAstCmdSubst(interp, arena, script, line);
            if (!cmdSubst || tclAstWordAddPart(interp, arena, word, cmdSubst) != 0) return NULL;

            literalStart = p;

        } else if (*p == '\\') {
            /* Backslash escape */

            /* Flush pending literal */
            if (p > literalStart) {
                TclAstNode *lit = tclAstLiteral(interp, arena, literalStart, p - literalStart, line);
                if (!lit || tclAstWordAddPart(interp, arena, word, lit) != 0) return NULL;
            }

            char buf[4];
            size_t bufLen;
            int consumed = parseBackslashEscape(p, end, buf, &bufLen);

            TclAstNode *bs = tclAstBackslash(interp, arena, buf, bufLen, line);
            if (!bs || tclAstWordAddPart(interp, arena, word, bs) != 0) return NULL;

            p += consumed;
            literalStart = p;

        } else {
            p++;
        }
    }

    /* Flush final literal */
    if (p > literalStart) {
        TclAstNode *lit = tclAstLiteral(interp, arena, literalStart, p - literalStart, line);
        if (!lit || tclAstWordAddPart(interp, arena, word, lit) != 0) return NULL;
    }

    /* Optimize: if word has only one literal part, return just the literal */
    if (word->u.word.count == 1 && word->u.word.parts[0]->type == TCL_NODE_LITERAL) {
        return word->u.word.parts[0];
    }

    /* If word has no parts, return empty literal */
    if (word->u.word.count == 0) {
        return tclAstLiteral(interp, arena, "", 0, line);
    }

    return word;
}

/* Parse a braced word - no substitution */
static TclAstNode *parseBracedWord(TclInterp *interp, void *arena, const char *str, size_t len, int line) {
    /* Content between braces is literal, skip outer braces */
    if (len >= 2 && str[0] == '{' && str[len-1] == '}') {
        return tclAstLiteral(interp, arena, str + 1, len - 2, line);
    }
    return tclAstLiteral(interp, arena, str, len, line);
}

/* Parse a single command */
static TclAstNode *parseCommand(TclInterp *interp, void *arena, TclLexer *lex) {
    TclAstNode *cmd = tclAstCommand(interp, arena, lex->line);
    if (!cmd) return NULL;

    while (!tclLexerAtCommandEnd(lex) && !tclLexerAtEnd(lex)) {
        TclWord word;
        int r = tclLexerNextWord(lex, &word, interp);
        if (r != 0) break;

        TclAstNode *wordNode;

        /* Check for {*} expansion */
        int isExpand = 0;
        const char *wordStart = word.start;
        size_t wordLen = word.len;

        if (word.type == TCL_WORD_BARE && wordLen >= 3 &&
            wordStart[0] == '{' && wordStart[1] == '*' && wordStart[2] == '}') {
            isExpand = 1;
            wordStart += 3;
            wordLen -= 3;
        }

        if (word.type == TCL_WORD_BRACES) {
            /* Braced - no substitution */
            wordNode = parseBracedWord(interp, arena, word.start, word.len, word.line);
        } else if (word.type == TCL_WORD_QUOTES) {
            /* Quoted - strip quotes, parse content */
            const char *content = word.start + 1;
            size_t contentLen = word.len - 2;
            wordNode = parseWordContent(interp, arena, content, contentLen, word.line, 1);
        } else {
            /* Bare word */
            if (isExpand) {
                wordNode = parseWordContent(interp, arena, wordStart, wordLen, word.line, 0);
            } else {
                wordNode = parseWordContent(interp, arena, word.start, word.len, word.line, 0);
            }
        }

        if (!wordNode) return NULL;

        /* Wrap in expand node if needed */
        if (isExpand) {
            wordNode = tclAstExpand(interp, arena, wordNode, word.line);
            if (!wordNode) return NULL;
        }

        if (tclAstCommandAddWord(interp, arena, cmd, wordNode) != 0) return NULL;
    }

    /* Skip command terminator */
    if (!tclLexerAtEnd(lex)) {
        if (*lex->pos == ';' || *lex->pos == '\n') {
            if (*lex->pos == '\n') lex->line++;
            lex->pos++;
        }
    }

    return cmd;
}

/* Parse a complete script */
static TclAstNode *parseScript(TclInterp *interp, void *arena, const char *script, size_t len, int *lineOut) {
    TclLexer lex;
    tclLexerInit(&lex, script, len);

    TclAstNode *scriptNode = tclAstScript(interp, arena, lex.line);
    if (!scriptNode) return NULL;

    while (!tclLexerAtEnd(&lex)) {
        /* Skip whitespace and blank lines */
        tclLexerSkipSpace(&lex);

        if (tclLexerAtEnd(&lex)) break;

        /* Skip comments */
        if (tclLexerAtComment(&lex)) {
            while (!tclLexerAtEnd(&lex) && *lex.pos != '\n') {
                lex.pos++;
            }
            if (!tclLexerAtEnd(&lex)) {
                lex.line++;
                lex.pos++;
            }
            continue;
        }

        /* Skip empty commands */
        if (*lex.pos == ';' || *lex.pos == '\n') {
            if (*lex.pos == '\n') lex.line++;
            lex.pos++;
            continue;
        }

        TclAstNode *cmd = parseCommand(interp, arena, &lex);
        if (!cmd) return NULL;

        /* Only add non-empty commands */
        if (cmd->u.command.count > 0) {
            if (tclAstScriptAddCmd(interp, arena, scriptNode, cmd) != 0) return NULL;
        }
    }

    if (lineOut) *lineOut = lex.line;
    return scriptNode;
}

/* ========================================================================
 * Public Parsing API
 * ======================================================================== */

TclAstNode *tclAstParse(TclInterp *interp, void *arena, const char *script, size_t len) {
    int line;
    return parseScript(interp, arena, script, len, &line);
}

TclAstNode *tclAstParseWord(TclInterp *interp, void *arena, const char *word, size_t len, int quoted) {
    return parseWordContent(interp, arena, word, len, 1, quoted);
}
