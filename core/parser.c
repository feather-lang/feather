/*
 * parser.c - TCL Parser (Command Structure Building)
 *
 * Builds command structures from token stream.
 * Uses arena for temporary parse structures.
 */

#include "internal.h"

/* Maximum words per command */
enum { MAX_WORDS_PER_CMD = 256 };

/* ========================================================================
 * Parser Initialization and Cleanup
 * ======================================================================== */

void tclParserInit(TclParser *parser, TclInterp *interp,
                   const char *script, size_t len) {
    tclLexerInit(&parser->lex, script, len);
    parser->interp = interp;
    parser->arena = interp->host->arenaPush(interp->hostCtx);
    parser->arenaMark = interp->host->arenaMark(parser->arena);
}

void tclParserCleanup(TclParser *parser) {
    if (parser->arena && parser->interp) {
        parser->interp->host->arenaPop(parser->interp->hostCtx, parser->arena);
        parser->arena = NULL;
    }
}

/* ========================================================================
 * Skip Blank Lines and Comments
 * ======================================================================== */

static void parserSkipBlankAndComments(TclParser *parser) {
    TclLexer *lex = &parser->lex;

    while (!tclLexerAtEnd(lex)) {
        /* Skip whitespace */
        tclLexerSkipSpace(lex);

        if (tclLexerAtEnd(lex)) {
            break;
        }

        /* Skip blank lines */
        if (*lex->pos == '\n') {
            lex->pos++;
            lex->line++;
            continue;
        }

        /* Skip comments */
        if (tclLexerAtComment(lex)) {
            tclLexerSkipLine(lex);
            continue;
        }

        /* Found non-blank, non-comment content */
        break;
    }
}

/* ========================================================================
 * Parse Next Command
 * ======================================================================== */

int tclParserNextCommand(TclParser *parser, TclParsedCmd *cmd) {
    TclLexer *lex = &parser->lex;
    TclInterp *interp = parser->interp;
    const TclHost *host = interp->host;

    /* Reset arena to mark for this command */
    host->arenaReset(parser->arena, parser->arenaMark);

    /* Skip blank lines and comments */
    parserSkipBlankAndComments(parser);

    /* Check for end of script */
    if (tclLexerAtEnd(lex)) {
        cmd->words = NULL;
        cmd->wordCount = 0;
        return 1;  /* EOF */
    }

    /* Allocate word array from arena */
    TclWord *words = host->arenaAlloc(parser->arena,
                                       MAX_WORDS_PER_CMD * sizeof(TclWord),
                                       sizeof(void*));
    if (!words) {
        tclSetError(interp, "out of memory", -1);
        return -1;
    }

    cmd->lineStart = lex->line;
    cmd->wordCount = 0;

    /* Collect words until command terminator */
    while (!tclLexerAtEnd(lex) && !tclLexerAtCommandEnd(lex)) {
        if (cmd->wordCount >= MAX_WORDS_PER_CMD) {
            tclSetError(interp, "too many words in command", -1);
            return -1;
        }

        TclWord *word = &words[cmd->wordCount];
        int result = tclLexerNextWord(lex, word, interp);

        if (result < 0) {
            return -1;  /* Error */
        }
        if (result > 0) {
            break;  /* No more words */
        }

        cmd->wordCount++;
    }

    cmd->words = words;
    cmd->lineEnd = lex->line;

    /* Skip command terminator */
    if (!tclLexerAtEnd(lex)) {
        if (*lex->pos == '\n') {
            lex->pos++;
            lex->line++;
        } else if (*lex->pos == ';') {
            lex->pos++;
        }
    }

    return 0;  /* Success */
}
