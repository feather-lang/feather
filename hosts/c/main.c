/*
 * main.c - Entry Point for C Host
 *
 * Creates interpreter, reads script, evaluates, exits.
 */

#include "../../core/tclc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* External: get the C host table */
extern const TclHost *tclGetCHost(void);

/* External: eval script (from core/eval.c) */
extern TclResult tclEvalScript(TclInterp *interp, const char *script, size_t len);

/* External: interpreter management (from core/eval.c) */
extern TclInterp *tclInterpNew(const TclHost *host, void *hostCtx);
extern void tclInterpFree(TclInterp *interp);

/* Read entire file into buffer */
static char *readFile(const char *filename, size_t *sizeOut) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        *sizeOut = 0;
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(size + 1);
    if (!buf) {
        fclose(f);
        *sizeOut = 0;
        return NULL;
    }

    size_t read = fread(buf, 1, size, f);
    buf[read] = '\0';
    fclose(f);

    *sizeOut = read;
    return buf;
}

/* Read from stdin */
static char *readStdin(size_t *sizeOut) {
    size_t capacity = 4096;
    size_t size = 0;
    char *buf = malloc(capacity);
    if (!buf) {
        *sizeOut = 0;
        return NULL;
    }

    int c;
    while ((c = getchar()) != EOF) {
        if (size + 1 >= capacity) {
            capacity *= 2;
            char *newBuf = realloc(buf, capacity);
            if (!newBuf) {
                free(buf);
                *sizeOut = 0;
                return NULL;
            }
            buf = newBuf;
        }
        buf[size++] = (char)c;
    }
    buf[size] = '\0';

    *sizeOut = size;
    return buf;
}

int main(int argc, char **argv) {
    const char *filename = NULL;
    char *script = NULL;
    size_t scriptLen = 0;

    /* Parse arguments */
    if (argc > 1) {
        filename = argv[1];
        script = readFile(filename, &scriptLen);
        if (!script) {
            fprintf(stderr, "couldn't read file \"%s\": no such file or directory\n", filename);
            return 1;
        }
    } else {
        /* Read from stdin */
        script = readStdin(&scriptLen);
        if (!script) {
            fprintf(stderr, "error reading from stdin\n");
            return 1;
        }
    }

    /* Get host and create interpreter */
    const TclHost *host = tclGetCHost();
    void *hostCtx = host->interpContextNew(NULL, 0);
    if (!hostCtx) {
        fprintf(stderr, "failed to create host context\n");
        free(script);
        return 1;
    }

    TclInterp *interp = tclInterpNew(host, hostCtx);
    if (!interp) {
        fprintf(stderr, "failed to create interpreter\n");
        host->interpContextFree(hostCtx);
        free(script);
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
            fprintf(stderr, "%.*s\n", (int)len, msg);
        }

        /* Print error info (stack trace) */
        if (interp->errorInfo) {
            size_t len;
            const char *info = host->getStringPtr(interp->errorInfo, &len);
            fprintf(stderr, "%.*s\n", (int)len, info);
        }

        exitCode = 1;
    }

    /* Cleanup */
    tclInterpFree(interp);
    host->interpContextFree(hostCtx);
    free(script);

    return exitCode;
}
