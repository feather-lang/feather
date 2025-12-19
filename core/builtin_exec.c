/*
 * builtin_exec.c - TCL exec Command Implementation
 *
 * Implements the exec command for subprocess execution with pipelines,
 * redirections, and background execution support.
 */

#include "internal.h"

/* Maximum number of processes in a pipeline */
enum { MAX_PIPELINE = 32 };

/* Maximum number of arguments per command */
enum { MAX_ARGS = 256 };

/* Redirection types */
typedef enum {
    REDIR_NONE = 0,
    REDIR_INPUT_FILE,       /* < fileName */
    REDIR_INPUT_STRING,     /* << value */
    REDIR_INPUT_HANDLE,     /* <@ fileId */
    REDIR_OUTPUT_FILE,      /* > fileName */
    REDIR_OUTPUT_APPEND,    /* >> fileName */
    REDIR_OUTPUT_HANDLE,    /* >@ fileId */
    REDIR_ERROR_FILE,       /* 2> fileName */
    REDIR_ERROR_APPEND,     /* 2>> fileName */
    REDIR_ERROR_HANDLE,     /* 2>@ fileId */
    REDIR_ERROR_TO_OUT,     /* 2>@1 */
    REDIR_BOTH_FILE,        /* >& fileName */
    REDIR_BOTH_APPEND,      /* >>& fileName */
    REDIR_BOTH_HANDLE,      /* >&@ fileId */
} RedirType;

/* A single command in the pipeline */
typedef struct {
    const char *argv[MAX_ARGS];
    int         argc;
} PipelineCmd;

/* Parse state for exec arguments */
typedef struct {
    int keepNewline;
    int ignoreStderr;
    int background;

    /* Redirections */
    RedirType   stdinType;
    const char *stdinValue;
    size_t      stdinLen;

    RedirType   stdoutType;
    const char *stdoutValue;

    RedirType   stderrType;
    const char *stderrValue;

    /* Pipeline */
    PipelineCmd cmds[MAX_PIPELINE];
    int         cmdCount;
    int         mergeStderr;  /* |& was used */
} ExecState;

/* Helper: check if string matches */
static int strMatch(const char *s, size_t len, const char *target) {
    size_t tlen = tclStrlen(target);
    return len == tlen && tclStrncmp(s, target, len) == 0;
}

/* Helper: check if string starts with prefix */
static int strPrefix(const char *s, size_t len, const char *prefix, size_t plen) {
    return len >= plen && tclStrncmp(s, prefix, plen) == 0;
}

/* Parse exec switches and arguments */
static TclResult parseExecArgs(TclInterp *interp, int objc, TclObj **objv,
                                ExecState *state, int *argStart) {
    const TclHost *host = interp->host;
    int i = 1;

    /* Initialize state */
    state->keepNewline = 0;
    state->ignoreStderr = 0;
    state->background = 0;
    state->stdinType = REDIR_NONE;
    state->stdinValue = NULL;
    state->stdinLen = 0;
    state->stdoutType = REDIR_NONE;
    state->stdoutValue = NULL;
    state->stderrType = REDIR_NONE;
    state->stderrValue = NULL;
    state->cmdCount = 0;
    state->mergeStderr = 0;

    /* Parse switches */
    while (i < objc) {
        size_t len;
        const char *arg = host->getStringPtr(objv[i], &len);

        if (len == 0 || arg[0] != '-') {
            break;
        }

        if (strMatch(arg, len, "--")) {
            i++;
            break;
        } else if (strMatch(arg, len, "-keepnewline")) {
            state->keepNewline = 1;
            i++;
        } else if (strMatch(arg, len, "-ignorestderr")) {
            state->ignoreStderr = 1;
            i++;
        } else {
            /* Unknown switch - treat as start of command */
            break;
        }
    }

    *argStart = i;

    /* Must have at least one argument for command */
    if (i >= objc) {
        tclSetError(interp, "wrong # args: should be \"exec ?switches? arg ?arg ...?\"", -1);
        return TCL_ERROR;
    }

    /* Parse pipeline and redirections */
    state->cmdCount = 1;
    state->cmds[0].argc = 0;
    int currentCmd = 0;

    while (i < objc) {
        size_t len;
        const char *arg = host->getStringPtr(objv[i], &len);

        /* Check for background */
        if (strMatch(arg, len, "&") && i == objc - 1) {
            state->background = 1;
            break;
        }

        /* Check for pipe */
        if (strMatch(arg, len, "|")) {
            if (state->cmds[currentCmd].argc == 0) {
                tclSetError(interp, "illegal use of | or |& in command", -1);
                return TCL_ERROR;
            }
            currentCmd++;
            if (currentCmd >= MAX_PIPELINE) {
                tclSetError(interp, "too many commands in pipeline", -1);
                return TCL_ERROR;
            }
            state->cmdCount++;
            state->cmds[currentCmd].argc = 0;
            i++;
            continue;
        }

        if (strMatch(arg, len, "|&")) {
            if (state->cmds[currentCmd].argc == 0) {
                tclSetError(interp, "illegal use of | or |& in command", -1);
                return TCL_ERROR;
            }
            currentCmd++;
            if (currentCmd >= MAX_PIPELINE) {
                tclSetError(interp, "too many commands in pipeline", -1);
                return TCL_ERROR;
            }
            state->cmdCount++;
            state->cmds[currentCmd].argc = 0;
            state->mergeStderr = 1;
            i++;
            continue;
        }

        /* Check for redirections */
        if (strMatch(arg, len, "<<")) {
            i++;
            if (i >= objc) {
                tclSetError(interp, "can't specify \"<<\" as last word in command", -1);
                return TCL_ERROR;
            }
            state->stdinType = REDIR_INPUT_STRING;
            state->stdinValue = host->getStringPtr(objv[i], &state->stdinLen);
            i++;
            continue;
        }

        if (strMatch(arg, len, "<") || strPrefix(arg, len, "<", 1)) {
            const char *fileName;
            size_t fileLen;
            if (len == 1) {
                i++;
                if (i >= objc) {
                    tclSetError(interp, "can't specify \"<\" as last word in command", -1);
                    return TCL_ERROR;
                }
                fileName = host->getStringPtr(objv[i], &fileLen);
            } else {
                fileName = arg + 1;
                fileLen = len - 1;
            }
            state->stdinType = REDIR_INPUT_FILE;
            state->stdinValue = fileName;
            i++;
            continue;
        }

        if (strMatch(arg, len, "2>@1")) {
            state->stderrType = REDIR_ERROR_TO_OUT;
            i++;
            continue;
        }

        if (strMatch(arg, len, ">") || strPrefix(arg, len, ">", 1)) {
            const char *fileName;
            size_t fileLen;
            if (len == 1) {
                i++;
                if (i >= objc) {
                    tclSetError(interp, "can't specify \">\" as last word in command", -1);
                    return TCL_ERROR;
                }
                fileName = host->getStringPtr(objv[i], &fileLen);
            } else {
                fileName = arg + 1;
                fileLen = len - 1;
            }
            state->stdoutType = REDIR_OUTPUT_FILE;
            state->stdoutValue = fileName;
            i++;
            continue;
        }

        if (strMatch(arg, len, "2>") || strPrefix(arg, len, "2>", 2)) {
            const char *fileName;
            size_t fileLen;
            if (len == 2) {
                i++;
                if (i >= objc) {
                    tclSetError(interp, "can't specify \"2>\" as last word in command", -1);
                    return TCL_ERROR;
                }
                fileName = host->getStringPtr(objv[i], &fileLen);
            } else {
                fileName = arg + 2;
                fileLen = len - 2;
            }
            state->stderrType = REDIR_ERROR_FILE;
            state->stderrValue = fileName;
            i++;
            continue;
        }

        /* Regular argument */
        if (state->cmds[currentCmd].argc >= MAX_ARGS - 1) {
            tclSetError(interp, "too many arguments", -1);
            return TCL_ERROR;
        }
        state->cmds[currentCmd].argv[state->cmds[currentCmd].argc++] = arg;
        i++;
    }

    /* Check we have at least one command with arguments */
    if (state->cmds[0].argc == 0) {
        tclSetError(interp, "wrong # args: should be \"exec ?switches? arg ?arg ...?\"", -1);
        return TCL_ERROR;
    }

    /* Null-terminate all argv arrays */
    for (int c = 0; c < state->cmdCount; c++) {
        state->cmds[c].argv[state->cmds[c].argc] = NULL;
    }

    return TCL_OK;
}

/* Execute the pipeline */
static TclResult execPipeline(TclInterp *interp, ExecState *state) {
    const TclHost *host = interp->host;

    /* For single command without background */
    if (state->cmdCount == 1 && !state->background) {
        int flags = TCL_PROCESS_PIPE_STDOUT;

        if (state->stdinType == REDIR_INPUT_STRING) {
            flags |= TCL_PROCESS_PIPE_STDIN;
        }

        if (state->stderrType == REDIR_ERROR_TO_OUT || !state->ignoreStderr) {
            flags |= TCL_PROCESS_PIPE_STDERR;
        }

        TclChannel *pipeIn = NULL;
        TclChannel *pipeOut = NULL;
        TclChannel *pipeErr = NULL;

        TclProcess *proc = host->processSpawn(
            state->cmds[0].argv,
            state->cmds[0].argc,
            flags,
            &pipeIn, &pipeOut, &pipeErr
        );

        if (proc == NULL) {
            /* Build error message with command name */
            void *arena = host->arenaPush(interp->hostCtx);
            const char *cmdName = state->cmds[0].argv[0];
            size_t cmdLen = tclStrlen(cmdName);
            size_t msgLen = cmdLen + 32;
            char *msg = host->arenaAlloc(arena, msgLen, 1);

            /* Simple concatenation */
            size_t pos = 0;
            const char *prefix = "couldn't execute \"";
            while (*prefix) msg[pos++] = *prefix++;
            for (size_t j = 0; j < cmdLen; j++) msg[pos++] = cmdName[j];
            const char *suffix = "\": no such file or directory";
            while (*suffix) msg[pos++] = *suffix++;
            msg[pos] = '\0';

            tclSetError(interp, msg, (int)pos);
            host->arenaPop(interp->hostCtx, arena);
            return TCL_ERROR;
        }

        /* Write stdin if needed */
        if (state->stdinType == REDIR_INPUT_STRING && pipeIn != NULL) {
            host->chanWrite(pipeIn, state->stdinValue, state->stdinLen);
            host->chanClose(interp->hostCtx, pipeIn);
        }

        /* Read stdout */
        void *arena = host->arenaPush(interp->hostCtx);
        char *outBuf = host->arenaAlloc(arena, 65536, 1);
        size_t outLen = 0;

        if (pipeOut != NULL) {
            int n;
            while ((n = host->chanRead(pipeOut, outBuf + outLen, 65536 - outLen - 1)) > 0) {
                outLen += n;
                if (outLen >= 65535) break;
            }
            host->chanClose(interp->hostCtx, pipeOut);
        }
        outBuf[outLen] = '\0';

        /* Read stderr */
        char *errBuf = host->arenaAlloc(arena, 65536, 1);
        size_t errLen = 0;

        if (pipeErr != NULL) {
            int n;
            while ((n = host->chanRead(pipeErr, errBuf + errLen, 65536 - errLen - 1)) > 0) {
                errLen += n;
                if (errLen >= 65535) break;
            }
            host->chanClose(interp->hostCtx, pipeErr);
        }
        errBuf[errLen] = '\0';

        /* Wait for process */
        int exitCode = 0;
        host->processWait(proc, &exitCode);

        /* Strip trailing newline unless -keepnewline */
        if (!state->keepNewline && outLen > 0 && outBuf[outLen - 1] == '\n') {
            outLen--;
            outBuf[outLen] = '\0';
        }

        /* Merge stderr with stdout if 2>@1 */
        if (state->stderrType == REDIR_ERROR_TO_OUT && errLen > 0) {
            /* Append stderr to stdout */
            if (outLen + errLen < 65535) {
                for (size_t j = 0; j < errLen; j++) {
                    outBuf[outLen++] = errBuf[j];
                }
                outBuf[outLen] = '\0';

                /* Strip trailing newline again */
                if (!state->keepNewline && outLen > 0 && outBuf[outLen - 1] == '\n') {
                    outLen--;
                    outBuf[outLen] = '\0';
                }
            }
            errLen = 0;  /* Don't report as error */
        }

        /* Check for errors */
        if (exitCode != 0) {
            /* Non-zero exit code */
            tclSetResult(interp, host->newString(outBuf, outLen));

            /* Set errorcode to CHILDSTATUS pid exitCode */
            void *ecArena = host->arenaPush(interp->hostCtx);
            TclObj **ecParts = host->arenaAlloc(ecArena, 3 * sizeof(TclObj*), sizeof(void*));
            ecParts[0] = host->newString("CHILDSTATUS", 11);
            ecParts[1] = host->newInt(host->processPid(proc));
            ecParts[2] = host->newInt(exitCode);
            TclObj *errorCode = host->newList(ecParts, 3);
            tclSetErrorCode(interp, errorCode);
            host->arenaPop(interp->hostCtx, ecArena);

            if (errLen > 0) {
                tclSetError(interp, errBuf, (int)errLen);
            } else {
                tclSetError(interp, "child process exited abnormally", -1);
            }
            host->arenaPop(interp->hostCtx, arena);
            return TCL_ERROR;
        }

        /* Check for stderr output (unless ignored) */
        if (errLen > 0 && !state->ignoreStderr && state->stderrType != REDIR_ERROR_TO_OUT) {
            /* Stderr output is an error */
            tclSetResult(interp, host->newString(outBuf, outLen));
            tclSetError(interp, errBuf, (int)errLen);
            host->arenaPop(interp->hostCtx, arena);
            return TCL_ERROR;
        }

        /* Success */
        tclSetResult(interp, host->newString(outBuf, outLen));
        host->arenaPop(interp->hostCtx, arena);
        return TCL_OK;
    }

    /* Background execution - return PIDs */
    if (state->background) {
        void *arena = host->arenaPush(interp->hostCtx);
        TclObj **pids = host->arenaAlloc(arena, state->cmdCount * sizeof(TclObj*), sizeof(void*));
        int pidCount = 0;

        for (int c = 0; c < state->cmdCount; c++) {
            int flags = TCL_PROCESS_BACKGROUND;

            TclProcess *proc = host->processSpawn(
                state->cmds[c].argv,
                state->cmds[c].argc,
                flags,
                NULL, NULL, NULL
            );

            if (proc != NULL) {
                pids[pidCount++] = host->newInt(host->processPid(proc));
            }
        }

        tclSetResult(interp, host->newList(pids, pidCount));
        host->arenaPop(interp->hostCtx, arena);
        return TCL_OK;
    }

    /* Multi-command pipeline - simplified implementation */
    /* TODO: Implement full pipeline with inter-process pipes */
    tclSetError(interp, "pipeline execution not yet implemented", -1);
    return TCL_ERROR;
}

/* Main exec command */
TclResult tclCmdExec(TclInterp *interp, int objc, TclObj **objv) {
    if (objc < 2) {
        tclSetError(interp, "wrong # args: should be \"exec ?switches? arg ?arg ...?\"", -1);
        return TCL_ERROR;
    }

    ExecState state;
    int argStart;

    TclResult r = parseExecArgs(interp, objc, objv, &state, &argStart);
    if (r != TCL_OK) {
        return r;
    }

    return execPipeline(interp, &state);
}
