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
#include <unistd.h>

/* External object functions from object.c */
extern TclObj *hostNewString(const char *s, size_t len);
extern TclObj *hostNewList(TclObj **elems, size_t count);

/* Channel structure */
struct TclChannel {
    FILE       *fp;
    gchar      *name;
    gboolean    isStd;      /* Is standard stream (don't close) */
    gboolean    readable;
    gboolean    writable;
    gchar      *translation;  /* Translation mode: auto, lf, cr, crlf, binary */
    gchar      *encoding;     /* Encoding: utf-8, etc. */
    gchar      *buffering;    /* Buffering: full, line, none */
    gboolean    blocking;     /* Blocking mode */
};

/* Standard channels */
static TclChannel stdinChan  = {NULL, "stdin",  TRUE, TRUE, FALSE, NULL, NULL, NULL, TRUE};
static TclChannel stdoutChan = {NULL, "stdout", TRUE, FALSE, TRUE, NULL, NULL, NULL, TRUE};
static TclChannel stderrChan = {NULL, "stderr", TRUE, FALSE, TRUE, NULL, NULL, NULL, TRUE};
static gboolean stdInitialized = FALSE;

/* Channel table - maps channel name to TclChannel* */
static GHashTable *channelTable = NULL;
static int nextFileId = 1;

static void initStdChannels(void) {
    if (!stdInitialized) {
        stdinChan.fp = stdin;
        stdinChan.translation = g_strdup("auto");
        stdinChan.encoding = g_strdup("utf-8");
        stdinChan.buffering = g_strdup("line");

        stdoutChan.fp = stdout;
        stdoutChan.translation = g_strdup("auto");
        stdoutChan.encoding = g_strdup("utf-8");
        stdoutChan.buffering = g_strdup("line");

        stderrChan.fp = stderr;
        stderrChan.translation = g_strdup("auto");
        stderrChan.encoding = g_strdup("utf-8");
        stderrChan.buffering = g_strdup("none");

        stdInitialized = TRUE;
    }

    if (!channelTable) {
        channelTable = g_hash_table_new(g_str_hash, g_str_equal);
        g_hash_table_insert(channelTable, "stdin", &stdinChan);
        g_hash_table_insert(channelTable, "stdout", &stdoutChan);
        g_hash_table_insert(channelTable, "stderr", &stderrChan);
    }
}

/* Look up a channel by name */
TclChannel *hostChanLookup(void *ctx, const char *name) {
    (void)ctx;
    initStdChannels();
    return g_hash_table_lookup(channelTable, name);
}

/* Open a channel */
TclChannel *hostChanOpen(void *ctx, const char *name, const char *mode) {
    (void)ctx;
    initStdChannels();

    FILE *fp = g_fopen(name, mode);
    if (!fp) return NULL;

    TclChannel *chan = g_new0(TclChannel, 1);
    if (!chan) {
        fclose(fp);
        return NULL;
    }

    chan->fp = fp;
    /* Generate a unique channel ID like "file3" */
    chan->name = g_strdup_printf("file%d", nextFileId++);
    chan->isStd = FALSE;
    chan->readable = (strchr(mode, 'r') != NULL || strchr(mode, '+') != NULL);
    chan->writable = (strchr(mode, 'w') != NULL || strchr(mode, 'a') != NULL || strchr(mode, '+') != NULL);
    chan->translation = g_strdup("auto");
    chan->encoding = g_strdup("utf-8");
    chan->buffering = g_strdup("full");
    chan->blocking = TRUE;

    /* Register in channel table */
    g_hash_table_insert(channelTable, chan->name, chan);

    return chan;
}

/* Create a channel from a file descriptor (for process pipes) */
TclChannel *hostChanFromFd(int fd, gboolean readable, gboolean writable) {
    initStdChannels();

    const char *mode;
    if (readable && writable) {
        mode = "r+";
    } else if (readable) {
        mode = "r";
    } else {
        mode = "w";
    }

    FILE *fp = fdopen(fd, mode);
    if (!fp) {
        close(fd);
        return NULL;
    }

    TclChannel *chan = g_new0(TclChannel, 1);
    if (!chan) {
        fclose(fp);
        return NULL;
    }

    chan->fp = fp;
    chan->name = g_strdup_printf("pipe%d", nextFileId++);
    chan->isStd = FALSE;
    chan->readable = readable;
    chan->writable = writable;
    chan->translation = g_strdup("binary");
    chan->encoding = g_strdup("utf-8");
    chan->buffering = g_strdup("full");
    chan->blocking = TRUE;

    /* Register in channel table */
    g_hash_table_insert(channelTable, chan->name, chan);

    return chan;
}

/* Close a channel */
void hostChanClose(void *ctx, TclChannel *chan) {
    (void)ctx;
    if (!chan || chan->isStd) return;

    /* Remove from table */
    if (channelTable && chan->name) {
        g_hash_table_remove(channelTable, chan->name);
    }

    fclose(chan->fp);
    g_free(chan->name);
    g_free(chan->translation);
    g_free(chan->encoding);
    g_free(chan->buffering);
    g_free(chan);
}

/* Get channel name */
const char *hostChanGetName(TclChannel *chan) {
    return chan ? chan->name : NULL;
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

/* Configure channel */
int hostChanConfigure(TclChannel *chan, const char *opt, TclObj *val) {
    if (!chan) return -1;

    size_t valLen;
    const char *valStr = NULL;
    extern const char *hostGetStringPtr(TclObj *obj, size_t *lenOut);
    if (val) {
        valStr = hostGetStringPtr(val, &valLen);
    }

    if (strcmp(opt, "-translation") == 0 && valStr) {
        g_free(chan->translation);
        /* TCL converts "binary" to "lf" */
        if (strcmp(valStr, "binary") == 0) {
            chan->translation = g_strdup("lf");
        } else {
            chan->translation = g_strdup(valStr);
        }
        return 0;
    } else if (strcmp(opt, "-encoding") == 0 && valStr) {
        g_free(chan->encoding);
        chan->encoding = g_strdup(valStr);
        return 0;
    } else if (strcmp(opt, "-buffering") == 0 && valStr) {
        g_free(chan->buffering);
        chan->buffering = g_strdup(valStr);
        return 0;
    } else if (strcmp(opt, "-blocking") == 0 && val) {
        extern int hostAsBool(TclObj *obj, int *out);
        int b;
        if (hostAsBool(val, &b) == 0) {
            chan->blocking = b;
        }
        return 0;
    }

    return 0;
}

/* Get channel option */
TclObj *hostChanCget(TclChannel *chan, const char *opt) {
    if (!chan) return hostNewString("", 0);

    if (strcmp(opt, "-translation") == 0) {
        const char *val = chan->translation ? chan->translation : "auto";
        return hostNewString(val, strlen(val));
    } else if (strcmp(opt, "-encoding") == 0) {
        const char *val = chan->encoding ? chan->encoding : "utf-8";
        return hostNewString(val, strlen(val));
    } else if (strcmp(opt, "-buffering") == 0) {
        const char *val = chan->buffering ? chan->buffering : "full";
        return hostNewString(val, strlen(val));
    } else if (strcmp(opt, "-blocking") == 0) {
        return hostNewString(chan->blocking ? "1" : "0", 1);
    }

    return hostNewString("", 0);
}

/* List channel names */
TclObj *hostChanNames(void *ctx, const char *pattern) {
    (void)ctx;
    initStdChannels();

    if (!channelTable) {
        return hostNewString("", 0);
    }

    GPtrArray *names = g_ptr_array_new();

    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, channelTable);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        const char *name = (const char *)key;
        if (!pattern || g_pattern_match_simple(pattern, name)) {
            g_ptr_array_add(names, hostNewString(name, strlen(name)));
        }
    }

    TclObj *result = hostNewList((TclObj **)names->pdata, names->len);
    g_ptr_array_free(names, TRUE);
    return result;
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

/* Truncate channel */
int hostChanTruncate(TclChannel *chan, int64_t length) {
    if (!chan || !chan->fp) return -1;

    /* Flush first */
    fflush(chan->fp);

    /* Get file descriptor */
    int fd = fileno(chan->fp);
    if (fd < 0) return -1;

    /* If length is -1, use current position */
    if (length < 0) {
        length = ftell(chan->fp);
    }

    /* Truncate */
    return ftruncate(fd, length);
}

/* Copy between channels */
int64_t hostChanCopy(TclChannel *src, TclChannel *dst, int64_t size) {
    if (!src || !src->fp || !dst || !dst->fp) return -1;

    char buf[8192];
    int64_t total = 0;

    while (size < 0 || total < size) {
        size_t toRead = sizeof(buf);
        if (size >= 0 && (size - total) < (int64_t)toRead) {
            toRead = (size_t)(size - total);
        }

        size_t n = fread(buf, 1, toRead, src->fp);
        if (n == 0) break;

        size_t written = fwrite(buf, 1, n, dst->fp);
        if (written != n) break;

        total += n;
    }

    return total;
}

/* Get pending data size */
int64_t hostChanPending(TclChannel *chan, int input) {
    (void)chan;
    (void)input;
    /* For regular files, there's no pending buffer we can easily measure */
    return 0;
}

/* Create a pipe pair */
int hostChanPipe(void *ctx, TclChannel **readChan, TclChannel **writeChan) {
    (void)ctx;
    initStdChannels();

    int fds[2];
    if (pipe(fds) < 0) return -1;

    FILE *readFp = fdopen(fds[0], "r");
    FILE *writeFp = fdopen(fds[1], "w");

    if (!readFp || !writeFp) {
        if (readFp) fclose(readFp);
        if (writeFp) fclose(writeFp);
        close(fds[0]);
        close(fds[1]);
        return -1;
    }

    TclChannel *rChan = g_new0(TclChannel, 1);
    TclChannel *wChan = g_new0(TclChannel, 1);

    rChan->fp = readFp;
    rChan->name = g_strdup_printf("file%d", nextFileId++);
    rChan->readable = TRUE;
    rChan->writable = FALSE;
    rChan->translation = g_strdup("auto");
    rChan->encoding = g_strdup("utf-8");
    rChan->buffering = g_strdup("full");
    rChan->blocking = TRUE;

    wChan->fp = writeFp;
    wChan->name = g_strdup_printf("file%d", nextFileId++);
    wChan->readable = FALSE;
    wChan->writable = TRUE;
    wChan->translation = g_strdup("auto");
    wChan->encoding = g_strdup("utf-8");
    wChan->buffering = g_strdup("full");
    wChan->blocking = TRUE;

    g_hash_table_insert(channelTable, rChan->name, rChan);
    g_hash_table_insert(channelTable, wChan->name, wChan);

    *readChan = rChan;
    *writeChan = wChan;

    return 0;
}
