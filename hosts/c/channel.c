/*
 * channel.c - I/O Channel Implementation for C Host (GLib version)
 *
 * FILE*-based channels for stdin, stdout, stderr, and files using GLib.
 */

#include "../../core/tclc.h"
#include <glib.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <string.h>

/* External object functions from object.c */
extern TclObj *hostNewString(const char *s, size_t len);

/* Channel structure */
struct TclChannel {
    FILE       *fp;
    gchar      *name;
    gboolean    isStd;      /* Is standard stream (don't close) */
    gboolean    readable;
    gboolean    writable;
};

/* Standard channels */
static TclChannel stdinChan  = {NULL, "stdin",  TRUE, TRUE, FALSE};
static TclChannel stdoutChan = {NULL, "stdout", TRUE, FALSE, TRUE};
static TclChannel stderrChan = {NULL, "stderr", TRUE, FALSE, TRUE};
static gboolean stdInitialized = FALSE;

static void initStdChannels(void) {
    if (!stdInitialized) {
        stdinChan.fp = stdin;
        stdoutChan.fp = stdout;
        stderrChan.fp = stderr;
        stdInitialized = TRUE;
    }
}

/* Open a channel */
TclChannel *hostChanOpen(void *ctx, const char *name, const char *mode) {
    (void)ctx;

    FILE *fp = g_fopen(name, mode);
    if (!fp) return NULL;

    TclChannel *chan = g_new0(TclChannel, 1);
    if (!chan) {
        fclose(fp);
        return NULL;
    }

    chan->fp = fp;
    chan->name = g_strdup(name);
    chan->isStd = FALSE;
    chan->readable = (strchr(mode, 'r') != NULL);
    chan->writable = (strchr(mode, 'w') != NULL || strchr(mode, 'a') != NULL);

    return chan;
}

/* Close a channel */
void hostChanClose(void *ctx, TclChannel *chan) {
    (void)ctx;
    if (!chan || chan->isStd) return;

    fclose(chan->fp);
    g_free(chan->name);
    g_free(chan);
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

    GString *line = g_string_new(NULL);
    gint c;

    while ((c = fgetc(chan->fp)) != EOF) {
        if (c == '\n') break;
        g_string_append_c(line, c);
    }

    if (c == EOF && line->len == 0) {
        if (eofOut) *eofOut = 1;
        g_string_free(line, TRUE);
        return NULL;
    }

    if (eofOut) *eofOut = 0;

    TclObj *obj = hostNewString(line->str, line->len);
    g_string_free(line, TRUE);
    return obj;
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
