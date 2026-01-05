// feather-c is the C host interpreter for the feather test harness.
// It links against libfeather.so (built from the Go package) and provides
// REPL, script mode, and benchmark mode.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "libfeather.h"

// -----------------------------------------------------------------------------
// Harness Protocol
// -----------------------------------------------------------------------------

static int in_harness = 0;
static FILE *harness_fd = NULL;

static void write_harness_result(const char *code, const char *result, const char *error) {
    if (!in_harness || !harness_fd) return;

    fprintf(harness_fd, "return: %s\n", code);
    if (result && result[0]) {
        fprintf(harness_fd, "result: %s\n", result);
    }
    if (error && error[0]) {
        fprintf(harness_fd, "error: %s\n", error);
    }
    fflush(harness_fd);
}

// -----------------------------------------------------------------------------
// Test Commands
// -----------------------------------------------------------------------------

static int cmd_say_hello(void *userData, int argc, char **argv, char **result, char **error) {
    (void)userData; (void)argc; (void)argv; (void)error;
    printf("hello\n");
    *result = strdup("");
    return 0;
}

static int cmd_echo(void *userData, int argc, char **argv, char **result, char **error) {
    (void)userData; (void)error;
    for (int i = 0; i < argc; i++) {
        if (i > 0) printf(" ");
        printf("%s", argv[i]);
    }
    printf("\n");
    *result = strdup("");
    return 0;
}

static int cmd_count(void *userData, int argc, char **argv, char **result, char **error) {
    (void)userData; (void)argv; (void)error;
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", argc);
    *result = strdup(buf);
    return 0;
}

static int cmd_list(void *userData, int argc, char **argv, char **result, char **error) {
    (void)userData; (void)error;

    // Calculate result size
    size_t total_len = 0;
    for (int i = 0; i < argc; i++) {
        total_len += strlen(argv[i]) + 3; // space + possible braces
    }

    char *buf = malloc(total_len + 1);
    if (!buf) {
        *error = strdup("out of memory");
        return 1;
    }
    buf[0] = '\0';

    for (int i = 0; i < argc; i++) {
        if (i > 0) strcat(buf, " ");

        const char *s = argv[i];
        int needs_braces = 0;
        for (const char *p = s; *p; p++) {
            if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '{' || *p == '}') {
                needs_braces = 1;
                break;
            }
        }

        if (needs_braces) {
            strcat(buf, "{");
            strcat(buf, s);
            strcat(buf, "}");
        } else if (s[0] == '\0') {
            strcat(buf, "{}");
        } else {
            strcat(buf, s);
        }
    }

    *result = buf;
    return 0;
}

// -----------------------------------------------------------------------------
// Counter Foreign Type
// -----------------------------------------------------------------------------

typedef struct {
    int value;
} Counter;

// Counter constructor - creates a new counter, returns pointer as result
static int counter_new(void *userData, int argc, char **argv, char **result, char **error) {
    (void)userData; (void)argc; (void)argv; (void)error;
    Counter *c = malloc(sizeof(Counter));
    if (!c) {
        *error = strdup("out of memory");
        return 1;
    }
    c->value = 0;
    // Return pointer as hex string
    char buf[32];
    snprintf(buf, sizeof(buf), "%p", (void*)c);
    *result = strdup(buf);
    return 0;
}

// Helper to parse integer with validation
static int parse_int(const char *s, int argNum, int *out, char **error) {
    char *endptr;
    long val = strtol(s, &endptr, 10);
    // Check for conversion errors
    if (endptr == s || *endptr != '\0') {
        char buf[128];
        snprintf(buf, sizeof(buf), "argument %d: expected integer but got \"%s\"", argNum, s);
        *error = strdup(buf);
        return 0;
    }
    *out = (int)val;
    return 1;
}

// Counter methods - userData is the Counter pointer
static int counter_get(void *userData, int argc, char **argv, char **result, char **error) {
    (void)argc; (void)argv; (void)error;
    Counter *c = (Counter*)userData;
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", c->value);
    *result = strdup(buf);
    return 0;
}

static int counter_set(void *userData, int argc, char **argv, char **result, char **error) {
    Counter *c = (Counter*)userData;
    if (argc != 1) {
        char buf[64];
        snprintf(buf, sizeof(buf), "wrong # args: expected 1, got %d", argc);
        *error = strdup(buf);
        return 1;
    }
    int val;
    if (!parse_int(argv[0], 1, &val, error)) {
        return 1;
    }
    c->value = val;
    *result = strdup("");
    return 0;
}

static int counter_incr(void *userData, int argc, char **argv, char **result, char **error) {
    (void)argc; (void)argv; (void)error;
    Counter *c = (Counter*)userData;
    c->value++;
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", c->value);
    *result = strdup(buf);
    return 0;
}

static int counter_add(void *userData, int argc, char **argv, char **result, char **error) {
    Counter *c = (Counter*)userData;
    if (argc != 1) {
        char buf[64];
        snprintf(buf, sizeof(buf), "wrong # args: expected 1, got %d", argc);
        *error = strdup(buf);
        return 1;
    }
    int val;
    if (!parse_int(argv[0], 1, &val, error)) {
        return 1;
    }
    c->value += val;
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", c->value);
    *result = strdup(buf);
    return 0;
}

// -----------------------------------------------------------------------------
// Command Registration
// -----------------------------------------------------------------------------

static void register_test_commands(GoUintptr interp) {
    // Set milestone variables
    FeatherSetVar(interp, "milestone", "m1");
    FeatherSetVar(interp, "current-step", "m1");

    // Register test commands
    FeatherRegisterCommand(interp, "say-hello", cmd_say_hello, NULL);
    FeatherRegisterCommand(interp, "echo", cmd_echo, NULL);
    FeatherRegisterCommand(interp, "count", cmd_count, NULL);
    FeatherRegisterCommand(interp, "list", cmd_list, NULL);

    // Register Counter type
    FeatherRegisterForeignType(interp, "Counter", counter_new, NULL);
    FeatherRegisterForeignMethod(interp, "Counter", "get", counter_get);
    FeatherRegisterForeignMethod(interp, "Counter", "set", counter_set);
    FeatherRegisterForeignMethod(interp, "Counter", "incr", counter_incr);
    FeatherRegisterForeignMethod(interp, "Counter", "add", counter_add);
}

// -----------------------------------------------------------------------------
// REPL Mode
// -----------------------------------------------------------------------------

static void run_repl(GoUintptr interp) {
    char line[4096];
    char input_buffer[65536];
    input_buffer[0] = '\0';

    for (;;) {
        if (input_buffer[0] == '\0') {
            printf("%% ");
        } else {
            printf("> ");
        }
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }

        // Remove trailing newline
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }

        // Append to input buffer
        if (input_buffer[0] != '\0') {
            strcat(input_buffer, "\n");
        }
        strcat(input_buffer, line);

        // Check if input is complete
        int status = FeatherParse(interp, input_buffer, (int)strlen(input_buffer));
        if (status == 1) { // ParseIncomplete
            continue;
        }

        if (status == 2) { // ParseError
            char *msg = FeatherParseMessage(interp);
            fprintf(stderr, "error: %s\n", msg);
            FeatherFreeString(msg);
            input_buffer[0] = '\0';
            continue;
        }

        // Evaluate
        int ok = FeatherEval(interp, input_buffer, (int)strlen(input_buffer));
        if (!ok) {
            char *errMsg = FeatherEvalError(interp);
            fprintf(stderr, "error: %s\n", errMsg);
            FeatherFreeString(errMsg);
        } else {
            char *result = FeatherEvalResult(interp);
            if (result && result[0]) {
                printf("%s\n", result);
            }
            FeatherFreeString(result);
        }

        input_buffer[0] = '\0';
    }
}

// -----------------------------------------------------------------------------
// Script Mode
// -----------------------------------------------------------------------------

static char* read_all_stdin(size_t *out_len) {
    size_t capacity = 4096;
    size_t len = 0;
    char *buf = malloc(capacity);
    if (!buf) return NULL;

    for (;;) {
        if (len + 1024 > capacity) {
            capacity *= 2;
            char *new_buf = realloc(buf, capacity);
            if (!new_buf) {
                free(buf);
                return NULL;
            }
            buf = new_buf;
        }

        size_t n = fread(buf + len, 1, capacity - len - 1, stdin);
        if (n == 0) break;
        len += n;
    }

    buf[len] = '\0';
    *out_len = len;
    return buf;
}

static void run_script(GoUintptr interp) {
    size_t script_len;
    char *script = read_all_stdin(&script_len);
    if (!script) {
        fprintf(stderr, "error reading script\n");
        write_harness_result("TCL_ERROR", "", "error reading script");
        exit(1);
    }

    // Check for parse errors
    int status = FeatherParse(interp, script, (int)script_len);
    if (status == 1) { // ParseIncomplete
        char *result = FeatherParseResult(interp);
        write_harness_result("TCL_OK", result ? result : "", "");
        FeatherFreeString(result);
        free(script);
        exit(2);
    }
    if (status == 2) { // ParseError
        char *msg = FeatherParseMessage(interp);
        char *result = FeatherParseResult(interp);
        write_harness_result("TCL_ERROR", result ? result : "", msg ? msg : "");
        FeatherFreeString(msg);
        FeatherFreeString(result);
        free(script);
        exit(3);
    }

    // Evaluate
    int ok = FeatherEval(interp, script, (int)script_len);
    free(script);

    if (!ok) {
        char *errMsg = FeatherEvalError(interp);
        printf("%s\n", errMsg);
        write_harness_result("TCL_ERROR", "", errMsg);
        FeatherFreeString(errMsg);
        exit(1);
    }

    char *result = FeatherEvalResult(interp);
    if (result && result[0]) {
        printf("%s\n", result);
    }
    write_harness_result("TCL_OK", result ? result : "", "");
    FeatherFreeString(result);
}

// -----------------------------------------------------------------------------
// Benchmark Mode (simplified - no JSON parsing)
// -----------------------------------------------------------------------------

static void run_benchmark_mode(void) {
    // For now, just print an error - full JSON support requires more code
    fprintf(stderr, "benchmark mode not yet implemented\n");
    exit(1);
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------

int main(int argc, char *argv[]) {
    // Check for benchmark mode
    if (argc > 1 && strcmp(argv[1], "--benchmark") == 0) {
        run_benchmark_mode();
        return 0;
    }

    // Check harness mode
    const char *harness_env = getenv("FEATHER_IN_HARNESS");
    if (harness_env && strcmp(harness_env, "1") == 0) {
        in_harness = 1;
        harness_fd = fdopen(3, "w");
    }

    // Create interpreter
    GoUintptr interp = FeatherNew();
    register_test_commands(interp);

    // Check if stdin is TTY
    if (isatty(0)) {
        run_repl(interp);
    } else {
        run_script(interp);
    }

    FeatherClose(interp);
    return 0;
}
