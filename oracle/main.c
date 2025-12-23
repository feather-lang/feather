/*
 * Oracle: Reference TCL interpreter for test comparison
 *
 * This embeds the real TCL interpreter to provide ground truth
 * for tclc's behavior.
 */

#include <tcl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int in_harness = 0;
static FILE *harness_fd = NULL;

static void write_harness_result(const char *return_code, const char *result,
                                  const char *error_msg) {
    if (!in_harness || !harness_fd) {
        return;
    }

    fprintf(harness_fd, "return: %s\n", return_code);
    if (result && result[0] != '\0') {
        fprintf(harness_fd, "result: %s\n", result);
    }
    if (error_msg && error_msg[0] != '\0') {
        fprintf(harness_fd, "error: %s\n", error_msg);
    }
    fflush(harness_fd);
}

static char *read_stdin(size_t *len) {
    size_t capacity = 4096;
    size_t size = 0;
    char *buffer = malloc(capacity);
    if (!buffer) {
        return NULL;
    }

    while (1) {
        size_t to_read = capacity - size;
        size_t n = fread(buffer + size, 1, to_read, stdin);
        size += n;
        if (n < to_read) {
            break;
        }
        capacity *= 2;
        char *new_buffer = realloc(buffer, capacity);
        if (!new_buffer) {
            free(buffer);
            return NULL;
        }
        buffer = new_buffer;
    }

    buffer[size] = '\0';
    *len = size;
    return buffer;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    /* Check if running in harness mode */
    const char *harness_env = getenv("TCLC_IN_HARNESS");
    if (harness_env && strcmp(harness_env, "1") == 0) {
        in_harness = 1;
        harness_fd = fdopen(3, "w");
    }

    /* Read script from stdin */
    size_t script_len;
    char *script = read_stdin(&script_len);
    if (!script) {
        fprintf(stderr, "error reading script\n");
        write_harness_result("TCL_ERROR", "", "error reading script");
        return 1;
    }

    /* Create TCL interpreter */
    Tcl_Interp *interp = Tcl_CreateInterp();
    if (!interp) {
        fprintf(stderr, "failed to create interpreter\n");
        write_harness_result("TCL_ERROR", "", "failed to create interpreter");
        free(script);
        return 1;
    }

    /* Initialize the interpreter */
    if (Tcl_Init(interp) != TCL_OK) {
        fprintf(stderr, "Tcl_Init failed: %s\n", Tcl_GetStringResult(interp));
        write_harness_result("TCL_ERROR", "", Tcl_GetStringResult(interp));
        Tcl_DeleteInterp(interp);
        free(script);
        return 1;
    }

    /* Evaluate the script */
    int result = Tcl_Eval(interp, script);
    const char *result_str = Tcl_GetStringResult(interp);

    if (result == TCL_OK) {
        if (result_str && result_str[0] != '\0') {
            printf("%s\n", result_str);
        }
        write_harness_result("TCL_OK", result_str, "");
    } else {
        printf("%s\n", result_str);
        write_harness_result("TCL_ERROR", "", result_str);
    }

    /* Cleanup */
    Tcl_DeleteInterp(interp);
    free(script);

    if (harness_fd) {
        fclose(harness_fd);
    }

    return (result == TCL_OK) ? 0 : 1;
}
