/*
 * channel.c - I/O Channel Implementation for C Host
 *
 * FILE*-based channels for stdin, stdout, stderr, and files.
 */

#include "../../core/tclc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* External object functions from object.c */
extern TclObj *hostNewString(const char *s, size_t len);

/* Channel structure */
struct TclChannel {
    FILE       *fp;
    char       *name;
    int         isStd;      /* Is standard stream (don't close) */
    int         readable;
    int         writable;
};

/* Standard channels */
static TclChannel stdinChan  = {NULL, "stdin",  1, 1, 0};
static TclChannel stdoutChan = {NULL, "stdout", 1, 0, 1};
static TclChannel stderrChan = {NULL, "stderr", 1, 0, 1};
static int stdInitialized = 0;

static void initStdChannels(void) {
    if (!stdInitialized) {
        stdinChan.fp = stdin;
        stdoutChan.fp = stdout;
        stderrChan.fp = stderr;
        stdInitialized = 1;
    }
}

/* Open a channel */
TclChannel *hostChanOpen(void *ctx, const char *name, const char *mode) {
    (void)ctx;

    FILE *fp = fopen(name, mode);
    if (!fp) return NULL;

    TclChannel *chan = malloc(sizeof(TclChannel));
    if (!chan) {
        fclose(fp);
        return NULL;
    }

    chan->fp = fp;
    chan->name = strdup(name);
    chan->isStd = 0;
    chan->readable = (strchr(mode, 'r') != NULL);
    chan->writable = (strchr(mode, 'w') != NULL || strchr(mode, 'a') != NULL);

    return chan;
}

/* Close a channel */
void hostChanClose(void *ctx, TclChannel *chan) {
    (void)ctx;
    if (!chan || chan->isStd) return;

    fclose(chan->fp);
    free(chan->name);
    free(chan);
}

/* Get standard channels */
TclChannel *hostChanStdin(void *ctx) {
    (void)ctx;
    initStdChannels();
    return &stdinChan;
}

TclChannel *hostChanStdout(void *ctx) {
    (void)ctx;
    initStdChannels();
    return &stdoutChan;
}

TclChannel *hostChanStderr(void *ctx) {
    (void)ctx;
    initStdChannels();
    return &stderrChan;
}

/* Read from channel */
int hostChanRead(TclChannel *chan, char *buf, size_t len) {
    if (!chan || !chan->fp) return -1;
    return (int)fread(buf, 1, len, chan->fp);
}

/* Write to channel */
int hostChanWrite(TclChannel *chan, const char *buf, size_t len) {
    if (!chan || !chan->fp) return -1;
    return (int)fwrite(buf, 1, len, chan->fp);
}

/* Read a line */
TclObj *hostChanGets(TclChannel *chan, int *eofOut) {
    if (!chan || !chan->fp) {
        if (eofOut) *eofOut = 1;
        return NULL;
    }

    char buf[4096];
    if (!fgets(buf, sizeof(buf), chan->fp)) {
        if (eofOut) *eofOut = 1;
        return NULL;
    }

    if (eofOut) *eofOut = 0;

    /* Remove trailing newline */
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[--len] = '\0';
    }

    return hostNewString(buf, len);
}

/* Flush channel */
int hostChanFlush(TclChannel *chan) {
    if (!chan || !chan->fp) return -1;
    return fflush(chan->fp);
}

/* Seek */
int hostChanSeek(TclChannel *chan, int64_t offset, int whence) {
    if (!chan || !chan->fp) return -1;
    return fseek(chan->fp, (long)offset, whence);
}

/* Tell position */
int64_t hostChanTell(TclChannel *chan) {
    if (!chan || !chan->fp) return -1;
    return ftell(chan->fp);
}

/* Check EOF */
int hostChanEof(TclChannel *chan) {
    if (!chan || !chan->fp) return 1;
    return feof(chan->fp);
}

/* Check if blocked (always 0 for files) */
int hostChanBlocked(TclChannel *chan) {
    (void)chan;
    return 0;
}

/* Configure channel (stub) */
int hostChanConfigure(TclChannel *chan, const char *opt, TclObj *val) {
    (void)chan;
    (void)opt;
    (void)val;
    return 0;
}

/* Get channel option (stub) */
TclObj *hostChanCget(TclChannel *chan, const char *opt) {
    (void)chan;
    (void)opt;
    return hostNewString("", 0);
}

/* List channel names (stub) */
TclObj *hostChanNames(void *ctx, const char *pattern) {
    (void)ctx;
    (void)pattern;
    return hostNewString("stdin stdout stderr", 19);
}

/* Channel sharing (stubs) */
void hostChanShare(void *fromCtx, void *toCtx, TclChannel *chan) {
    (void)fromCtx;
    (void)toCtx;
    (void)chan;
}

void hostChanTransfer(void *fromCtx, void *toCtx, TclChannel *chan) {
    (void)fromCtx;
    (void)toCtx;
    (void)chan;
}
