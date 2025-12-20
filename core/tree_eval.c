/*
 * tree_eval.c - Tree-Walking TCL Evaluator
 *
 * Evaluates AST nodes by walking the tree structure.
 * Uses an explicit state stack for non-recursive evaluation,
 * enabling coroutine suspend/resume by saving the stack state.
 */

#include "internal.h"
#include "ast.h"

/* ========================================================================
 * Evaluation State
 *
 * The evaluator maintains an explicit stack of states instead of using
 * C recursion. This allows the evaluation to be suspended (for yield)
 * and resumed later by saving/restoring the state stack.
 * ======================================================================== */

typedef enum {
    TREE_EVAL_SCRIPT,      /* Evaluating a script node */
    TREE_EVAL_COMMAND,     /* Evaluating a command node */
    TREE_EVAL_WORD,        /* Evaluating a word node */
    TREE_EVAL_VAR,         /* Looking up a variable */
    TREE_EVAL_CMD_SUBST,   /* Evaluating command substitution */
} TreeEvalPhase;

typedef struct TreeEvalFrame {
    TreeEvalPhase phase;
    TclAstNode *node;           /* Current node being evaluated */
    int index;                  /* Current child index */
    TclObj **args;              /* Accumulated arguments (for commands) */
    int argCount;               /* Number of accumulated arguments */
    int argCapacity;            /* Capacity of args array */
    TclObj *result;             /* Partial result for words */
    struct TreeEvalFrame *parent;
} TreeEvalFrame;

/* Maximum stack depth */
enum { MAX_EVAL_DEPTH = 256 };

typedef struct TreeEvalState {
    TreeEvalFrame *top;         /* Top of evaluation stack */
    void *arena;                /* Arena for allocation */
    int suspended;              /* Was evaluation suspended (yield)? */
    TclObj *yieldValue;         /* Value passed to yield */
} TreeEvalState;

/* ========================================================================
 * State Management
 * ======================================================================== */

static TreeEvalFrame *pushFrame(TclInterp *interp, TreeEvalState *state, TreeEvalPhase phase, TclAstNode *node) {
    const TclHost *host = interp->host;

    TreeEvalFrame *frame = host->arenaAlloc(state->arena, sizeof(TreeEvalFrame), sizeof(void*));
    if (!frame) return NULL;

    frame->phase = phase;
    frame->node = node;
    frame->index = 0;
    frame->args = NULL;
    frame->argCount = 0;
    frame->argCapacity = 0;
    frame->result = NULL;
    frame->parent = state->top;

    state->top = frame;
    return frame;
}

static void popFrame(TreeEvalState *state) {
    if (state->top) {
        state->top = state->top->parent;
    }
}

/* Add argument to command frame */
static int addArg(TclInterp *interp, TreeEvalState *state, TreeEvalFrame *frame, TclObj *arg) {
    const TclHost *host = interp->host;

    if (frame->argCount >= frame->argCapacity) {
        int newCap = frame->argCapacity == 0 ? 8 : frame->argCapacity * 2;
        TclObj **newArgs = host->arenaAlloc(state->arena, newCap * sizeof(TclObj*), sizeof(void*));
        if (!newArgs) return -1;

        for (int i = 0; i < frame->argCount; i++) {
            newArgs[i] = frame->args[i];
        }
        frame->args = newArgs;
        frame->argCapacity = newCap;
    }

    frame->args[frame->argCount++] = arg;
    return 0;
}

/* Concatenate result for word building */
static int appendResult(TclInterp *interp, TreeEvalState *state, TreeEvalFrame *frame, TclObj *part) {
    const TclHost *host = interp->host;

    if (!frame->result) {
        frame->result = part;
        return 0;
    }

    /* Concatenate strings */
    size_t len1, len2;
    const char *s1 = host->getStringPtr(frame->result, &len1);
    const char *s2 = host->getStringPtr(part, &len2);

    char *buf = host->arenaAlloc(state->arena, len1 + len2 + 1, 1);
    if (!buf) return -1;

    for (size_t i = 0; i < len1; i++) buf[i] = s1[i];
    for (size_t i = 0; i < len2; i++) buf[len1 + i] = s2[i];
    buf[len1 + len2] = '\0';

    frame->result = host->newString(buf, len1 + len2);
    return 0;
}

/* ========================================================================
 * Variable Lookup
 * ======================================================================== */

static TclObj *lookupVar(TclInterp *interp, const char *name, size_t len) {
    const TclHost *host = interp->host;

    /* Try current frame first */
    TclObj *val = host->varGet(interp->currentFrame->varsHandle, name, len);
    if (val) return val;

    /* Try global frame if different */
    if (interp->currentFrame != interp->globalFrame) {
        val = host->varGet(interp->globalFrame->varsHandle, name, len);
    }

    return val;
}

/* ========================================================================
 * Command Dispatch
 * ======================================================================== */

static TclResult dispatchCommand(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc == 0) {
        tclSetResult(interp, host->newString("", 0));
        return TCL_OK;
    }

    size_t cmdLen;
    const char *cmdName = host->getStringPtr(objv[0], &cmdLen);

    /* Check builtins first */
    int builtinIdx = tclBuiltinLookup(cmdName, cmdLen);
    if (builtinIdx >= 0) {
        const TclBuiltinEntry *entry = tclBuiltinGet(builtinIdx);
        if (entry) {
            return entry->proc(interp, objc, objv);
        }
    }

    /* Try host command lookup (procs, extensions) */
    TclCmdInfo cmdInfo;
    if (host->cmdLookup(interp->hostCtx, cmdName, cmdLen, &cmdInfo) == 0) {
        switch (cmdInfo.type) {
            case TCL_CMD_PROC: {
                TclObj *argList = NULL;
                TclObj *body = NULL;
                if (host->procGetDef(cmdInfo.u.procHandle, &argList, &body) != 0) {
                    tclSetError(interp, "proc definition not found", -1);
                    return TCL_ERROR;
                }

                /* Parse argument specification */
                TclObj **argSpecs = NULL;
                size_t argCount = 0;
                if (host->asList(argList, &argSpecs, &argCount) != 0) {
                    tclSetError(interp, "invalid argument list", -1);
                    return TCL_ERROR;
                }

                /* Create proc frame */
                TclFrame *procFrame = host->frameAlloc(interp->hostCtx);
                if (!procFrame) {
                    tclSetError(interp, "out of memory", -1);
                    return TCL_ERROR;
                }
                procFrame->parent = interp->currentFrame;
                procFrame->level = interp->currentFrame->level + 1;
                procFrame->flags = TCL_FRAME_PROC;
                procFrame->procName = cmdName;
                procFrame->invocationObjs = objv;
                procFrame->invocationCount = objc;

                /* Check for 'args' parameter */
                int hasArgs = 0;
                if (argCount > 0) {
                    size_t lastLen;
                    const char *lastName = host->getStringPtr(argSpecs[argCount - 1], &lastLen);
                    if (lastLen == 4 && tclStrncmp(lastName, "args", 4) == 0) {
                        hasArgs = 1;
                    }
                }

                int requiredArgs = (int)argCount - (hasArgs ? 1 : 0);
                int actualArgs = objc - 1;

                /* Count args with defaults */
                int minArgs = 0;
                for (size_t i = 0; i < (size_t)requiredArgs; i++) {
                    if (host->listLength(argSpecs[i]) < 2) {
                        minArgs++;
                    }
                }

                if (actualArgs < minArgs || (!hasArgs && actualArgs > requiredArgs)) {
                    tclSetError(interp, "wrong # args", -1);
                    host->frameFree(interp->hostCtx, procFrame);
                    return TCL_ERROR;
                }

                /* Bind arguments */
                for (size_t i = 0; i < (size_t)requiredArgs; i++) {
                    TclObj *argSpec = argSpecs[i];
                    size_t argNameLen;
                    const char *argName;
                    TclObj *value = NULL;

                    size_t listLen = host->listLength(argSpec);
                    if (listLen >= 2) {
                        TclObj *nameObj = host->listIndex(argSpec, 0);
                        argName = host->getStringPtr(nameObj, &argNameLen);
                        if ((int)i < actualArgs) {
                            value = objv[i + 1];
                        } else {
                            value = host->listIndex(argSpec, 1);
                        }
                    } else {
                        argName = host->getStringPtr(argSpec, &argNameLen);
                        if ((int)i < actualArgs) {
                            value = objv[i + 1];
                        }
                    }

                    if (value) {
                        host->varSet(procFrame->varsHandle, argName, argNameLen, host->dup(value));
                    }
                }

                if (hasArgs) {
                    int argsStart = requiredArgs;
                    int argsCount = actualArgs - argsStart;
                    if (argsCount < 0) argsCount = 0;

                    TclObj *argsList;
                    if (argsCount > 0) {
                        argsList = host->newList(&objv[argsStart + 1], argsCount);
                    } else {
                        argsList = host->newString("", 0);
                    }
                    host->varSet(procFrame->varsHandle, "args", 4, argsList);
                }

                /* Execute body */
                TclFrame *savedFrame = interp->currentFrame;
                interp->currentFrame = procFrame;

                size_t bodyLen;
                const char *bodyStr = host->getStringPtr(body, &bodyLen);

                /* Use AST-based evaluation */
                TclResult result = tclTreeEvalStr(interp, bodyStr, bodyLen);

                if (result == TCL_RETURN) {
                    result = TCL_OK;
                }

                interp->currentFrame = savedFrame;
                host->frameFree(interp->hostCtx, procFrame);
                return result;
            }

            case TCL_CMD_EXTENSION:
                return host->extInvoke(interp, cmdInfo.u.extHandle, objc, objv);

            default:
                break;
        }
    }

    /* Check coroutines */
    TclCoroutine *coro = tclCoroLookup(cmdName, cmdLen);
    if (coro) {
        return tclCoroInvoke(interp, coro, objc, objv);
    }

    /* Command not found */
    void *arena = host->arenaPush(interp->hostCtx);
    char *msg = host->arenaAlloc(arena, cmdLen + 30, 1);
    char *p = msg;
    const char *prefix = "invalid command name \"";
    while (*prefix) *p++ = *prefix++;
    for (size_t i = 0; i < cmdLen; i++) *p++ = cmdName[i];
    *p++ = '"';
    *p = '\0';
    tclSetError(interp, msg, p - msg);
    host->arenaPop(interp->hostCtx, arena);
    return TCL_ERROR;
}

/* ========================================================================
 * Tree-Walking Evaluation Step
 *
 * Processes one step of evaluation. Returns:
 * - TCL_OK: Evaluation complete
 * - TCL_ERROR: Error occurred
 * - TCL_CONTINUE: More work to do (internal use)
 * ======================================================================== */

static TclResult evalStep(TclInterp *interp, TreeEvalState *state) {
    const TclHost *host = interp->host;

    if (!state->top) {
        return TCL_OK; /* Stack empty, done */
    }

    TreeEvalFrame *frame = state->top;

    switch (frame->phase) {
        case TREE_EVAL_SCRIPT: {
            TclAstNode *script = frame->node;

            if (frame->index >= script->u.script.count) {
                /* Script done */
                popFrame(state);
                return TCL_CONTINUE;
            }

            /* Push frame for next command */
            TclAstNode *cmd = script->u.script.cmds[frame->index++];
            if (!pushFrame(interp, state, TREE_EVAL_COMMAND, cmd)) {
                tclSetError(interp, "out of memory", -1);
                return TCL_ERROR;
            }
            return TCL_CONTINUE;
        }

        case TREE_EVAL_COMMAND: {
            TclAstNode *cmd = frame->node;

            if (frame->index >= cmd->u.command.count) {
                /* All words evaluated, dispatch command */
                TclResult result = dispatchCommand(interp, frame->argCount, frame->args);

                /* Handle control flow */
                if (result == TCL_ERROR || result == TCL_RETURN ||
                    result == TCL_BREAK || result == TCL_CONTINUE) {
                    return result;
                }

                /* Check for yield */
                if (tclCoroYieldPending()) {
                    state->suspended = 1;
                    return TCL_OK;
                }

                popFrame(state);
                return TCL_CONTINUE;
            }

            /* Push frame for next word */
            TclAstNode *wordNode = cmd->u.command.words[frame->index++];

            /* Handle expand nodes */
            if (wordNode->type == TCL_NODE_EXPAND) {
                /* Push word evaluation for inner node */
                if (!pushFrame(interp, state, TREE_EVAL_WORD, wordNode->u.expand.word)) {
                    tclSetError(interp, "out of memory", -1);
                    return TCL_ERROR;
                }
                /* Mark that we need to expand the result */
                state->top->phase = TREE_EVAL_WORD;
                /* TODO: Handle expansion after word evaluation */
            } else if (wordNode->type == TCL_NODE_LITERAL) {
                /* Literal - just add as argument */
                TclObj *arg = host->newString(wordNode->u.literal.value, wordNode->u.literal.len);
                if (addArg(interp, state, frame, arg) != 0) {
                    tclSetError(interp, "out of memory", -1);
                    return TCL_ERROR;
                }
            } else if (wordNode->type == TCL_NODE_VAR_SIMPLE) {
                /* Variable reference */
                TclObj *val = lookupVar(interp, wordNode->u.varSimple.name, wordNode->u.varSimple.len);
                if (!val) {
                    void *arena = host->arenaPush(interp->hostCtx);
                    char *msg = host->arenaAlloc(arena, wordNode->u.varSimple.len + 30, 1);
                    char *p = msg;
                    const char *prefix = "can't read \"";
                    while (*prefix) *p++ = *prefix++;
                    for (size_t i = 0; i < wordNode->u.varSimple.len; i++) *p++ = wordNode->u.varSimple.name[i];
                    const char *suffix = "\": no such variable";
                    while (*suffix) *p++ = *suffix++;
                    *p = '\0';
                    tclSetError(interp, msg, p - msg);
                    host->arenaPop(interp->hostCtx, arena);
                    return TCL_ERROR;
                }
                if (addArg(interp, state, frame, val) != 0) {
                    tclSetError(interp, "out of memory", -1);
                    return TCL_ERROR;
                }
            } else {
                /* Complex word - push word frame */
                if (!pushFrame(interp, state, TREE_EVAL_WORD, wordNode)) {
                    tclSetError(interp, "out of memory", -1);
                    return TCL_ERROR;
                }
            }
            return TCL_CONTINUE;
        }

        case TREE_EVAL_WORD: {
            TclAstNode *word = frame->node;

            if (word->type == TCL_NODE_LITERAL) {
                /* Simple literal */
                frame->result = host->newString(word->u.literal.value, word->u.literal.len);
                goto word_done;
            }

            if (word->type == TCL_NODE_BACKSLASH) {
                /* Backslash escape */
                frame->result = host->newString(word->u.backslash.value, word->u.backslash.len);
                goto word_done;
            }

            if (word->type == TCL_NODE_VAR_SIMPLE) {
                TclObj *val = lookupVar(interp, word->u.varSimple.name, word->u.varSimple.len);
                if (!val) {
                    void *arena = host->arenaPush(interp->hostCtx);
                    char *msg = host->arenaAlloc(arena, word->u.varSimple.len + 30, 1);
                    char *p = msg;
                    const char *prefix = "can't read \"";
                    while (*prefix) *p++ = *prefix++;
                    for (size_t i = 0; i < word->u.varSimple.len; i++) *p++ = word->u.varSimple.name[i];
                    const char *suffix = "\": no such variable";
                    while (*suffix) *p++ = *suffix++;
                    *p = '\0';
                    tclSetError(interp, msg, p - msg);
                    host->arenaPop(interp->hostCtx, arena);
                    return TCL_ERROR;
                }
                frame->result = val;
                goto word_done;
            }

            if (word->type == TCL_NODE_VAR_ARRAY) {
                /* Array reference - first evaluate index */
                if (frame->index == 0) {
                    frame->index = 1;
                    if (word->u.varArray.index) {
                        if (!pushFrame(interp, state, TREE_EVAL_WORD, word->u.varArray.index)) {
                            tclSetError(interp, "out of memory", -1);
                            return TCL_ERROR;
                        }
                        return TCL_CONTINUE;
                    }
                }

                /* Index evaluated, look up array element */
                const char *arrName = word->u.varArray.name;
                size_t arrLen = word->u.varArray.nameLen;
                size_t indexLen;
                const char *indexStr = "";
                if (frame->result) {
                    indexStr = host->getStringPtr(frame->result, &indexLen);
                } else {
                    indexLen = 0;
                }

                /* Build array(index) key */
                char *key = host->arenaAlloc(state->arena, arrLen + indexLen + 3, 1);
                char *p = key;
                for (size_t i = 0; i < arrLen; i++) *p++ = arrName[i];
                *p++ = '(';
                for (size_t i = 0; i < indexLen; i++) *p++ = indexStr[i];
                *p++ = ')';
                *p = '\0';

                TclObj *val = lookupVar(interp, key, p - key);
                if (!val) {
                    void *arena = host->arenaPush(interp->hostCtx);
                    char *msg = host->arenaAlloc(arena, (p - key) + 30, 1);
                    char *mp = msg;
                    const char *prefix = "can't read \"";
                    while (*prefix) *mp++ = *prefix++;
                    for (char *k = key; k < p; k++) *mp++ = *k;
                    const char *suffix = "\": no such variable";
                    while (*suffix) *mp++ = *suffix++;
                    *mp = '\0';
                    tclSetError(interp, msg, mp - msg);
                    host->arenaPop(interp->hostCtx, arena);
                    return TCL_ERROR;
                }
                frame->result = val;
                goto word_done;
            }

            if (word->type == TCL_NODE_CMD_SUBST) {
                /* Command substitution */
                if (frame->index == 0) {
                    frame->index = 1;
                    if (word->u.cmdSubst.script) {
                        if (!pushFrame(interp, state, TREE_EVAL_SCRIPT, word->u.cmdSubst.script)) {
                            tclSetError(interp, "out of memory", -1);
                            return TCL_ERROR;
                        }
                        return TCL_CONTINUE;
                    }
                }
                /* Script evaluated, result is in interp->result */
                frame->result = interp->result ? interp->result : host->newString("", 0);
                goto word_done;
            }

            if (word->type == TCL_NODE_WORD) {
                /* Composite word - evaluate parts */
                if (frame->index >= word->u.word.count) {
                    /* All parts done */
                    if (!frame->result) {
                        frame->result = host->newString("", 0);
                    }
                    goto word_done;
                }

                TclAstNode *part = word->u.word.parts[frame->index++];

                /* Handle simple parts inline */
                if (part->type == TCL_NODE_LITERAL) {
                    TclObj *lit = host->newString(part->u.literal.value, part->u.literal.len);
                    if (appendResult(interp, state, frame, lit) != 0) {
                        tclSetError(interp, "out of memory", -1);
                        return TCL_ERROR;
                    }
                    return TCL_CONTINUE;
                }

                if (part->type == TCL_NODE_BACKSLASH) {
                    TclObj *bs = host->newString(part->u.backslash.value, part->u.backslash.len);
                    if (appendResult(interp, state, frame, bs) != 0) {
                        tclSetError(interp, "out of memory", -1);
                        return TCL_ERROR;
                    }
                    return TCL_CONTINUE;
                }

                /* Complex part - push frame */
                if (!pushFrame(interp, state, TREE_EVAL_WORD, part)) {
                    tclSetError(interp, "out of memory", -1);
                    return TCL_ERROR;
                }
                return TCL_CONTINUE;
            }

            /* Unknown node type */
            tclSetError(interp, "internal error: unknown node type", -1);
            return TCL_ERROR;

        word_done:
            /* Word evaluation complete */
            popFrame(state);

            /* If parent is a command, add as argument */
            if (state->top && state->top->phase == TREE_EVAL_COMMAND) {
                if (addArg(interp, state, state->top, frame->result) != 0) {
                    tclSetError(interp, "out of memory", -1);
                    return TCL_ERROR;
                }
            }
            /* If parent is a word, append to result */
            else if (state->top && state->top->phase == TREE_EVAL_WORD) {
                if (appendResult(interp, state, state->top, frame->result) != 0) {
                    tclSetError(interp, "out of memory", -1);
                    return TCL_ERROR;
                }
            }
            return TCL_CONTINUE;
        }

        default:
            tclSetError(interp, "internal error: invalid eval phase", -1);
            return TCL_ERROR;
    }
}

/* ========================================================================
 * Public API
 * ======================================================================== */

TclResult tclTreeEvalAst(TclInterp *interp, TclAstNode *ast) {
    const TclHost *host = interp->host;

    if (!ast) {
        tclSetResult(interp, host->newString("", 0));
        return TCL_OK;
    }

    /* Initialize state */
    void *arena = host->arenaPush(interp->hostCtx);
    TreeEvalState state;
    state.top = NULL;
    state.arena = arena;
    state.suspended = 0;
    state.yieldValue = NULL;

    /* Push initial frame */
    if (ast->type == TCL_NODE_SCRIPT) {
        if (!pushFrame(interp, &state, TREE_EVAL_SCRIPT, ast)) {
            host->arenaPop(interp->hostCtx, arena);
            tclSetError(interp, "out of memory", -1);
            return TCL_ERROR;
        }
    } else {
        /* Single node - wrap in evaluation */
        if (!pushFrame(interp, &state, TREE_EVAL_WORD, ast)) {
            host->arenaPop(interp->hostCtx, arena);
            tclSetError(interp, "out of memory", -1);
            return TCL_ERROR;
        }
    }

    /* Set default result */
    tclSetResult(interp, host->newString("", 0));

    /* Run evaluation loop */
    TclResult result;
    while (1) {
        result = evalStep(interp, &state);

        if (result == TCL_CONTINUE) {
            continue;
        }

        if (state.suspended) {
            /* Yield occurred - need to save state for resume */
            /* For now, just return */
            break;
        }

        break;
    }

    host->arenaPop(interp->hostCtx, arena);
    return result == TCL_CONTINUE ? TCL_OK : result;
}

TclResult tclTreeEvalStr(TclInterp *interp, const char *script, size_t len) {
    const TclHost *host = interp->host;

    if (len == 0 || !script) {
        tclSetResult(interp, host->newString("", 0));
        return TCL_OK;
    }

    /* Parse script to AST */
    void *arena = host->arenaPush(interp->hostCtx);
    TclAstNode *ast = tclAstParse(interp, arena, script, len);

    if (!ast) {
        host->arenaPop(interp->hostCtx, arena);
        tclSetError(interp, "parse error", -1);
        return TCL_ERROR;
    }

    /* Evaluate AST */
    TclResult result = tclTreeEvalAst(interp, ast);

    host->arenaPop(interp->hostCtx, arena);
    return result;
}
