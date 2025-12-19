/*
 * main.c - Entry Point for C Host (GLib version)
 *
 * Creates interpreter, reads script, evaluates, exits.
 */

#include "../../core/tclc.h"
#include <glib.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <string.h>

/* External: get the C host table */
extern const TclHost *tclGetCHost(void);

/* External: eval script (from core/eval.c) */
extern TclResult tclEvalScript(TclInterp *interp, const char *script, size_t len);

/* External: interpreter management (from core/eval.c) */
extern TclInterp *tclInterpNew(const TclHost *host, void *hostCtx);
extern void tclInterpFree(TclInterp *interp);

/* Read entire file into buffer */
static gchar *readFile(const gchar *filename, gsize *sizeOut) {
    GError *error = NULL;
    gchar *contents = NULL;

    if (!g_file_get_contents(filename, &contents, sizeOut, &error)) {
        g_printerr("couldn't read file \"%s\": %s\n", filename, error->message);
        g_error_free(error);
        *sizeOut = 0;
        return NULL;
    }

    return contents;
}

/* Read from stdin */
static gchar *readStdin(gsize *sizeOut) {
    GString *content = g_string_new(NULL);
    gchar buf[4096];
    gsize bytes_read;

    while ((bytes_read = fread(buf, 1, sizeof(buf), stdin)) > 0) {
        g_string_append_len(content, buf, bytes_read);
    }

    *sizeOut = content->len;
    return g_string_free(content, FALSE);
}

int main(int argc, char **argv) {
    const gchar *filename = NULL;
    gchar *script = NULL;
    gsize scriptLen = 0;

    /* Parse arguments */
    if (argc > 1) {
        filename = argv[1];
        script = readFile(filename, &scriptLen);
        if (!script) {
            return 1;
        }
    } else {
        /* Read from stdin */
        script = readStdin(&scriptLen);
        if (!script) {
            g_printerr("error reading from stdin\n");
            return 1;
        }
    }

    /* Get host and create interpreter */
    const TclHost *host = tclGetCHost();
    void *hostCtx = host->interpContextNew(NULL, 0);
    if (!hostCtx) {
        g_printerr("failed to create host context\n");
        g_free(script);
        return 1;
    }

    TclInterp *interp = tclInterpNew(host, hostCtx);
    if (!interp) {
        g_printerr("failed to create interpreter\n");
        host->interpContextFree(hostCtx);
        g_free(script);
        return 1;
    }

    /* Set script file for error reporting */
    interp->scriptFile = filename;

    /* Evaluate script */
    TclResult result = tclEvalScript(interp, script, scriptLen);

    /* Report errors */
    int exitCode = 0;
    if (result == TCL_ERROR) {
        /* Print error message */
        if (interp->result) {
            size_t len;
            const char *msg = host->getStringPtr(interp->result, &len);
            g_printerr("%.*s\n", (int)len, msg);
        }

        /* Print error info (stack trace) */
        if (interp->errorInfo) {
            size_t len;
            const char *info = host->getStringPtr(interp->errorInfo, &len);
            g_printerr("%.*s\n", (int)len, info);
        }

        exitCode = 1;
    }

    /* Cleanup */
    tclInterpFree(interp);
    host->interpContextFree(hostCtx);
    g_free(script);

    return exitCode;
}
