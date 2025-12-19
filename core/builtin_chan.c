/*
 * builtin_chan.c - TCL Channel/I/O Command Implementations
 *
 * I/O commands: puts, open, close, gets, read, chan
 * Also includes the tclLookupChannel helper function.
 */

#include "internal.h"

/* ========================================================================
 * Channel Lookup Helper
 * ======================================================================== */

/* External channel lookup function from host */
extern TclChannel *hostChanLookup(void *ctx, const char *name);
extern const char *hostChanGetName(TclChannel *chan);

/* Look up a channel by name, returns NULL and sets error if not found */
static TclChannel *tclLookupChannel(TclInterp *interp, const char *name, size_t len) {
    const TclHost *host = interp->host;

    /* Check standard channels first */
    if (len == 5 && tclStrncmp(name, "stdin", 5) == 0) {
        return host->chanStdin(interp->hostCtx);
    } else if (len == 6 && tclStrncmp(name, "stdout", 6) == 0) {
        return host->chanStdout(interp->hostCtx);
    } else if (len == 6 && tclStrncmp(name, "stderr", 6) == 0) {
        return host->chanStderr(interp->hostCtx);
    }

    /* Look up in channel table - need null-terminated string */
    void *arena = host->arenaPush(interp->hostCtx);
    char *namez = host->arenaStrdup(arena, name, len);
    TclChannel *chan = hostChanLookup(interp->hostCtx, namez);
    host->arenaPop(interp->hostCtx, arena);

    if (!chan) {
        /* Build error message */
        void *arena2 = host->arenaPush(interp->hostCtx);
        char *err = host->arenaAlloc(arena2, len + 50, 1);
        char *ep = err;
        const char *prefix = "can not find channel named \"";
        while (*prefix) *ep++ = *prefix++;
        for (size_t i = 0; i < len; i++) *ep++ = name[i];
        *ep++ = '"';
        *ep = '\0';
        tclSetError(interp, err, ep - err);
        host->arenaPop(interp->hostCtx, arena2);
    }

    return chan;
}

/* ========================================================================
 * puts Command
 * ======================================================================== */

TclResult tclCmdPuts(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;
    int newline = 1;
    int argStart = 1;
    TclChannel *chan = host->chanStdout(interp->hostCtx);

    /* Check for -nonewline flag */
    if (objc >= 2) {
        size_t len;
        const char *arg = host->getStringPtr(objv[1], &len);
        if (len == 10 && tclStrncmp(arg, "-nonewline", 10) == 0) {
            newline = 0;
            argStart = 2;
        }
    }

    /* Check argument count */
    int remaining = objc - argStart;
    if (remaining < 1 || remaining > 2) {
        tclSetError(interp, "wrong # args: should be \"puts ?-nonewline? ?channelId? string\"", -1);
        return TCL_ERROR;
    }

    /* Get channel and string */
    TclObj *strObj;
    if (remaining == 2) {
        size_t chanLen;
        const char *chanName = host->getStringPtr(objv[argStart], &chanLen);
        chan = tclLookupChannel(interp, chanName, chanLen);
        if (!chan) return TCL_ERROR;
        strObj = objv[argStart + 1];
    } else {
        strObj = objv[argStart];
    }

    /* Write string */
    size_t strLen;
    const char *str = host->getStringPtr(strObj, &strLen);
    host->chanWrite(chan, str, strLen);
    if (newline) {
        host->chanWrite(chan, "\n", 1);
    }

    tclSetResult(interp, host->newString("", 0));
    return TCL_OK;
}

/* ========================================================================
 * open Command
 * ======================================================================== */

TclResult tclCmdOpen(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2 || objc > 4) {
        tclSetError(interp, "wrong # args: should be \"open fileName ?access? ?permissions?\"", -1);
        return TCL_ERROR;
    }

    size_t nameLen;
    const char *fileName = host->getStringPtr(objv[1], &nameLen);

    /* Default mode is "r" */
    const char *mode = "r";
    char modeStr[8] = {0};

    if (objc >= 3) {
        size_t modeLen;
        const char *modeArg = host->getStringPtr(objv[2], &modeLen);

        /* Convert TCL access modes to C fopen modes */
        if (modeLen == 1 && modeArg[0] == 'r') {
            mode = "r";
        } else if (modeLen == 2 && modeArg[0] == 'r' && modeArg[1] == '+') {
            mode = "r+";
        } else if (modeLen == 1 && modeArg[0] == 'w') {
            mode = "w";
        } else if (modeLen == 2 && modeArg[0] == 'w' && modeArg[1] == '+') {
            mode = "w+";
        } else if (modeLen == 1 && modeArg[0] == 'a') {
            mode = "a";
        } else if (modeLen == 2 && modeArg[0] == 'a' && modeArg[1] == '+') {
            mode = "a+";
        } else if (modeLen >= 2 && modeArg[0] == 'r' && modeArg[1] == 'b') {
            mode = "rb";
        } else if (modeLen >= 2 && modeArg[0] == 'w' && modeArg[1] == 'b') {
            mode = "wb";
        } else if (modeLen >= 2 && modeArg[0] == 'a' && modeArg[1] == 'b') {
            mode = "ab";
        } else if (modeLen >= 3 && modeArg[0] == 'r' && modeArg[1] == '+' && modeArg[2] == 'b') {
            mode = "r+b";
        } else if (modeLen >= 3 && modeArg[0] == 'w' && modeArg[1] == '+' && modeArg[2] == 'b') {
            mode = "w+b";
        } else if (modeLen >= 3 && modeArg[0] == 'r' && modeArg[1] == 'b' && modeArg[2] == '+') {
            mode = "rb+";
        } else if (modeLen >= 3 && modeArg[0] == 'w' && modeArg[1] == 'b' && modeArg[2] == '+') {
            mode = "wb+";
        } else {
            /* Use as-is for now */
            for (size_t i = 0; i < modeLen && i < 7; i++) {
                modeStr[i] = modeArg[i];
            }
            mode = modeStr;
        }
    }

    /* Need null-terminated filename */
    void *arena = host->arenaPush(interp->hostCtx);
    char *fileNameZ = host->arenaStrdup(arena, fileName, nameLen);

    TclChannel *chan = host->chanOpen(interp->hostCtx, fileNameZ, mode);
    host->arenaPop(interp->hostCtx, arena);

    if (!chan) {
        tclSetError(interp, "couldn't open file", -1);
        return TCL_ERROR;
    }

    /* Return the channel name */
    const char *chanName = hostChanGetName(chan);
    tclSetResult(interp, host->newString(chanName, tclStrlen(chanName)));
    return TCL_OK;
}

/* ========================================================================
 * close Command
 * ======================================================================== */

TclResult tclCmdClose(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2 || objc > 3) {
        tclSetError(interp, "wrong # args: should be \"close channelId ?direction?\"", -1);
        return TCL_ERROR;
    }

    size_t chanLen;
    const char *chanName = host->getStringPtr(objv[1], &chanLen);

    TclChannel *chan = tclLookupChannel(interp, chanName, chanLen);
    if (!chan) return TCL_ERROR;

    host->chanClose(interp->hostCtx, chan);

    tclSetResult(interp, host->newString("", 0));
    return TCL_OK;
}

/* ========================================================================
 * gets Command
 * ======================================================================== */

TclResult tclCmdGets(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2 || objc > 3) {
        tclSetError(interp, "wrong # args: should be \"gets channelId ?varName?\"", -1);
        return TCL_ERROR;
    }

    size_t chanLen;
    const char *chanName = host->getStringPtr(objv[1], &chanLen);

    TclChannel *chan = tclLookupChannel(interp, chanName, chanLen);
    if (!chan) return TCL_ERROR;

    int eof = 0;
    TclObj *line = host->chanGets(chan, &eof);

    if (objc == 3) {
        /* Store in variable, return length */
        size_t varLen;
        const char *varName = host->getStringPtr(objv[2], &varLen);

        if (eof && !line) {
            /* EOF with no data */
            host->varSet(interp->currentFrame->varsHandle, varName, varLen, host->newString("", 0));
            tclSetResult(interp, host->newInt(-1));
        } else {
            host->varSet(interp->currentFrame->varsHandle, varName, varLen, line);
            size_t lineLen;
            host->getStringPtr(line, &lineLen);
            tclSetResult(interp, host->newInt(lineLen));
        }
    } else {
        /* Return the line directly */
        if (eof && !line) {
            tclSetResult(interp, host->newString("", 0));
        } else {
            tclSetResult(interp, line);
        }
    }

    return TCL_OK;
}

/* ========================================================================
 * read Command
 * ======================================================================== */

TclResult tclCmdRead(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2 || objc > 3) {
        tclSetError(interp, "wrong # args: should be \"read channelId ?numChars?\" or \"read ?-nonewline? channelId\"", -1);
        return TCL_ERROR;
    }

    int stripNewline = 0;
    int argStart = 1;

    /* Check for -nonewline */
    size_t len;
    const char *arg1 = host->getStringPtr(objv[1], &len);
    if (len == 10 && tclStrncmp(arg1, "-nonewline", 10) == 0) {
        stripNewline = 1;
        argStart = 2;
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"read ?-nonewline? channelId\"", -1);
            return TCL_ERROR;
        }
    }

    size_t chanLen;
    const char *chanName = host->getStringPtr(objv[argStart], &chanLen);

    TclChannel *chan = tclLookupChannel(interp, chanName, chanLen);
    if (!chan) return TCL_ERROR;

    int64_t numChars = -1;  /* -1 means read all */
    if (objc == 3 && !stripNewline) {
        if (host->asInt(objv[2], &numChars) != 0) {
            tclSetError(interp, "expected integer but got \"...\"", -1);
            return TCL_ERROR;
        }
    }

    /* Read data */
    void *arena = host->arenaPush(interp->hostCtx);
    size_t bufSize = (numChars > 0) ? (size_t)numChars : 4096;
    size_t totalRead = 0;
    char *data = NULL;
    size_t dataCapacity = 0;

    while (numChars < 0 || (int64_t)totalRead < numChars) {
        size_t toRead = bufSize;
        if (numChars > 0 && (numChars - (int64_t)totalRead) < (int64_t)toRead) {
            toRead = (size_t)(numChars - (int64_t)totalRead);
        }

        /* Grow buffer if needed */
        if (totalRead + toRead > dataCapacity) {
            size_t newCap = dataCapacity == 0 ? toRead : dataCapacity * 2;
            if (newCap < totalRead + toRead) newCap = totalRead + toRead;
            char *newData = host->arenaAlloc(arena, newCap, 1);
            if (data) {
                for (size_t i = 0; i < totalRead; i++) newData[i] = data[i];
            }
            data = newData;
            dataCapacity = newCap;
        }

        int n = host->chanRead(chan, data + totalRead, toRead);
        if (n <= 0) break;
        totalRead += n;

        /* If we hit EOF or got less than requested, we're done */
        if ((size_t)n < toRead) break;
    }

    /* Strip trailing newline if requested */
    if (stripNewline && totalRead > 0 && data[totalRead - 1] == '\n') {
        totalRead--;
    }

    TclObj *result = host->newString(data ? data : "", totalRead);
    host->arenaPop(interp->hostCtx, arena);

    tclSetResult(interp, result);
    return TCL_OK;
}

/* ========================================================================
 * chan Command
 * ======================================================================== */

TclResult tclCmdChan(TclInterp *interp, int objc, TclObj **objv) {
    const TclHost *host = interp->host;

    if (objc < 2) {
        tclSetError(interp, "wrong # args: should be \"chan subcommand ?arg ...?\"", -1);
        return TCL_ERROR;
    }

    size_t subcmdLen;
    const char *subcmd = host->getStringPtr(objv[1], &subcmdLen);

    /* chan names ?pattern? */
    if (TCL_STREQ(subcmd, subcmdLen, "names")) {
        const char *pattern = NULL;
        if (objc >= 3) {
            size_t patLen;
            pattern = host->getStringPtr(objv[2], &patLen);
        }
        TclObj *names = host->chanNames(interp->hostCtx, pattern);
        tclSetResult(interp, names);
        return TCL_OK;
    }

    /* chan eof channelId */
    if (TCL_STREQ(subcmd, subcmdLen, "eof")) {
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"chan eof channelId\"", -1);
            return TCL_ERROR;
        }
        size_t chanLen;
        const char *chanName = host->getStringPtr(objv[2], &chanLen);
        TclChannel *chan = tclLookupChannel(interp, chanName, chanLen);
        if (!chan) return TCL_ERROR;
        tclSetResult(interp, host->newInt(host->chanEof(chan)));
        return TCL_OK;
    }

    /* chan blocked channelId */
    if (TCL_STREQ(subcmd, subcmdLen, "blocked")) {
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"chan blocked channelId\"", -1);
            return TCL_ERROR;
        }
        size_t chanLen;
        const char *chanName = host->getStringPtr(objv[2], &chanLen);
        TclChannel *chan = tclLookupChannel(interp, chanName, chanLen);
        if (!chan) return TCL_ERROR;
        tclSetResult(interp, host->newInt(host->chanBlocked(chan)));
        return TCL_OK;
    }

    /* chan configure channelId ?optionName? ?value? ?optionName value ...? */
    if (TCL_STREQ(subcmd, subcmdLen, "configure")) {
        if (objc < 3) {
            tclSetError(interp, "wrong # args: should be \"chan configure channelId ?optionName? ?value? ?optionName value ...?\"", -1);
            return TCL_ERROR;
        }
        size_t chanLen;
        const char *chanName = host->getStringPtr(objv[2], &chanLen);
        TclChannel *chan = tclLookupChannel(interp, chanName, chanLen);
        if (!chan) return TCL_ERROR;

        if (objc == 3) {
            /* Return all options as a list */
            TclObj *opts[8];
            int optCount = 0;
            opts[optCount++] = host->newString("-blocking", 9);
            opts[optCount++] = host->chanCget(chan, "-blocking");
            opts[optCount++] = host->newString("-buffering", 10);
            opts[optCount++] = host->chanCget(chan, "-buffering");
            opts[optCount++] = host->newString("-encoding", 9);
            opts[optCount++] = host->chanCget(chan, "-encoding");
            opts[optCount++] = host->newString("-translation", 12);
            opts[optCount++] = host->chanCget(chan, "-translation");
            tclSetResult(interp, host->newList(opts, optCount));
            return TCL_OK;
        }

        if (objc == 4) {
            /* Query single option */
            size_t optLen;
            const char *opt = host->getStringPtr(objv[3], &optLen);
            void *arena = host->arenaPush(interp->hostCtx);
            char *optZ = host->arenaStrdup(arena, opt, optLen);
            TclObj *val = host->chanCget(chan, optZ);
            host->arenaPop(interp->hostCtx, arena);
            tclSetResult(interp, val);
            return TCL_OK;
        }

        /* Set options */
        for (int i = 3; i + 1 < objc; i += 2) {
            size_t optLen;
            const char *opt = host->getStringPtr(objv[i], &optLen);
            void *arena = host->arenaPush(interp->hostCtx);
            char *optZ = host->arenaStrdup(arena, opt, optLen);
            host->chanConfigure(chan, optZ, objv[i + 1]);
            host->arenaPop(interp->hostCtx, arena);
        }
        tclSetResult(interp, host->newString("", 0));
        return TCL_OK;
    }

    /* chan puts ?-nonewline? ?channelId? string */
    if (TCL_STREQ(subcmd, subcmdLen, "puts")) {
        /* Reuse puts command implementation */
        TclObj **newObjv = (TclObj **)host->arenaAlloc(host->arenaPush(interp->hostCtx), sizeof(TclObj*) * objc, sizeof(void*));
        newObjv[0] = objv[0];
        for (int i = 2; i < objc; i++) {
            newObjv[i - 1] = objv[i];
        }
        TclResult result = tclCmdPuts(interp, objc - 1, newObjv);
        return result;
    }

    /* chan gets channelId ?varName? */
    if (TCL_STREQ(subcmd, subcmdLen, "gets")) {
        /* Reuse gets command implementation */
        TclObj **newObjv = (TclObj **)host->arenaAlloc(host->arenaPush(interp->hostCtx), sizeof(TclObj*) * objc, sizeof(void*));
        newObjv[0] = objv[0];
        for (int i = 2; i < objc; i++) {
            newObjv[i - 1] = objv[i];
        }
        TclResult result = tclCmdGets(interp, objc - 1, newObjv);
        return result;
    }

    /* chan read channelId ?numChars? or chan read ?-nonewline? channelId */
    if (TCL_STREQ(subcmd, subcmdLen, "read")) {
        /* Reuse read command implementation */
        TclObj **newObjv = (TclObj **)host->arenaAlloc(host->arenaPush(interp->hostCtx), sizeof(TclObj*) * objc, sizeof(void*));
        newObjv[0] = objv[0];
        for (int i = 2; i < objc; i++) {
            newObjv[i - 1] = objv[i];
        }
        TclResult result = tclCmdRead(interp, objc - 1, newObjv);
        return result;
    }

    /* chan flush channelId */
    if (TCL_STREQ(subcmd, subcmdLen, "flush")) {
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"chan flush channelId\"", -1);
            return TCL_ERROR;
        }
        size_t chanLen;
        const char *chanName = host->getStringPtr(objv[2], &chanLen);
        TclChannel *chan = tclLookupChannel(interp, chanName, chanLen);
        if (!chan) return TCL_ERROR;
        host->chanFlush(chan);
        tclSetResult(interp, host->newString("", 0));
        return TCL_OK;
    }

    /* chan close channelId ?direction? */
    if (TCL_STREQ(subcmd, subcmdLen, "close")) {
        if (objc < 3 || objc > 4) {
            tclSetError(interp, "wrong # args: should be \"chan close channelId ?direction?\"", -1);
            return TCL_ERROR;
        }
        size_t chanLen;
        const char *chanName = host->getStringPtr(objv[2], &chanLen);
        TclChannel *chan = tclLookupChannel(interp, chanName, chanLen);
        if (!chan) return TCL_ERROR;
        host->chanClose(interp->hostCtx, chan);
        tclSetResult(interp, host->newString("", 0));
        return TCL_OK;
    }

    /* chan tell channelId */
    if (TCL_STREQ(subcmd, subcmdLen, "tell")) {
        if (objc != 3) {
            tclSetError(interp, "wrong # args: should be \"chan tell channelId\"", -1);
            return TCL_ERROR;
        }
        size_t chanLen;
        const char *chanName = host->getStringPtr(objv[2], &chanLen);
        TclChannel *chan = tclLookupChannel(interp, chanName, chanLen);
        if (!chan) return TCL_ERROR;
        tclSetResult(interp, host->newInt(host->chanTell(chan)));
        return TCL_OK;
    }

    /* chan seek channelId offset ?origin? */
    if (TCL_STREQ(subcmd, subcmdLen, "seek")) {
        if (objc < 4 || objc > 5) {
            tclSetError(interp, "wrong # args: should be \"chan seek channelId offset ?origin?\"", -1);
            return TCL_ERROR;
        }
        size_t chanLen;
        const char *chanName = host->getStringPtr(objv[2], &chanLen);
        TclChannel *chan = tclLookupChannel(interp, chanName, chanLen);
        if (!chan) return TCL_ERROR;

        int64_t offset;
        if (host->asInt(objv[3], &offset) != 0) {
            tclSetError(interp, "expected integer but got offset", -1);
            return TCL_ERROR;
        }

        int whence = TCL_SEEK_SET;
        if (objc == 5) {
            size_t originLen;
            const char *origin = host->getStringPtr(objv[4], &originLen);
            if (TCL_STREQ(origin, originLen, "start")) {
                whence = TCL_SEEK_SET;
            } else if (TCL_STREQ(origin, originLen, "current")) {
                whence = TCL_SEEK_CUR;
            } else if (TCL_STREQ(origin, originLen, "end")) {
                whence = TCL_SEEK_END;
            }
        }

        host->chanSeek(chan, offset, whence);
        tclSetResult(interp, host->newString("", 0));
        return TCL_OK;
    }

    /* chan truncate channelId ?length? */
    if (TCL_STREQ(subcmd, subcmdLen, "truncate")) {
        if (objc < 3 || objc > 4) {
            tclSetError(interp, "wrong # args: should be \"chan truncate channelId ?length?\"", -1);
            return TCL_ERROR;
        }
        size_t chanLen;
        const char *chanName = host->getStringPtr(objv[2], &chanLen);
        TclChannel *chan = tclLookupChannel(interp, chanName, chanLen);
        if (!chan) return TCL_ERROR;

        int64_t length = -1;  /* -1 means current position */
        if (objc == 4) {
            if (host->asInt(objv[3], &length) != 0) {
                tclSetError(interp, "expected integer but got length", -1);
                return TCL_ERROR;
            }
        }

        host->chanTruncate(chan, length);
        tclSetResult(interp, host->newString("", 0));
        return TCL_OK;
    }

    /* chan copy inputChan outputChan ?-size size? ?-command callback? */
    if (TCL_STREQ(subcmd, subcmdLen, "copy")) {
        if (objc < 4) {
            tclSetError(interp, "wrong # args: should be \"chan copy inputChan outputChan ?-size size? ?-command callback?\"", -1);
            return TCL_ERROR;
        }

        size_t inChanLen, outChanLen;
        const char *inChanName = host->getStringPtr(objv[2], &inChanLen);
        const char *outChanName = host->getStringPtr(objv[3], &outChanLen);

        TclChannel *inChan = tclLookupChannel(interp, inChanName, inChanLen);
        if (!inChan) return TCL_ERROR;
        TclChannel *outChan = tclLookupChannel(interp, outChanName, outChanLen);
        if (!outChan) return TCL_ERROR;

        int64_t size = -1;  /* -1 means copy all */
        for (int i = 4; i + 1 < objc; i += 2) {
            size_t optLen;
            const char *opt = host->getStringPtr(objv[i], &optLen);
            if (TCL_STREQ(opt, optLen, "-size")) {
                if (host->asInt(objv[i + 1], &size) != 0) {
                    tclSetError(interp, "expected integer for -size", -1);
                    return TCL_ERROR;
                }
            }
        }

        int64_t copied = host->chanCopy(inChan, outChan, size);
        tclSetResult(interp, host->newInt(copied));
        return TCL_OK;
    }

    /* chan pending mode channelId */
    if (TCL_STREQ(subcmd, subcmdLen, "pending")) {
        if (objc != 4) {
            tclSetError(interp, "wrong # args: should be \"chan pending mode channelId\"", -1);
            return TCL_ERROR;
        }
        size_t modeLen;
        const char *mode = host->getStringPtr(objv[2], &modeLen);
        int input = TCL_STREQ(mode, modeLen, "input");

        size_t chanLen;
        const char *chanName = host->getStringPtr(objv[3], &chanLen);
        TclChannel *chan = tclLookupChannel(interp, chanName, chanLen);
        if (!chan) return TCL_ERROR;

        tclSetResult(interp, host->newInt(host->chanPending(chan, input)));
        return TCL_OK;
    }

    /* Unknown subcommand */
    void *arena = host->arenaPush(interp->hostCtx);
    char *err = host->arenaAlloc(arena, subcmdLen + 100, 1);
    char *ep = err;
    const char *prefix = "bad option \"";
    while (*prefix) *ep++ = *prefix++;
    for (size_t i = 0; i < subcmdLen; i++) *ep++ = subcmd[i];
    const char *suffix = "\": must be blocked, close, configure, copy, eof, flush, gets, names, pending, puts, read, seek, tell, or truncate";
    while (*suffix) *ep++ = *suffix++;
    *ep = '\0';
    tclSetError(interp, err, ep - err);
    host->arenaPop(interp->hostCtx, arena);
    return TCL_ERROR;
}


