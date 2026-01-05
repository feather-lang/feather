// feather-c is the C host interpreter for the feather test harness.
// It links against libfeather.so (built from the Go package) and provides
// REPL, script mode, and benchmark mode.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "libfeather.h"

// Convenience typedefs
typedef size_t FeatherInterp;
typedef size_t FeatherObj;

#define FEATHER_OK 0
#define FEATHER_PARSE_OK 0
#define FEATHER_PARSE_INCOMPLETE 1
#define FEATHER_PARSE_ERROR 2

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

static void* counter_new(void *userData) {
    (void)userData;
    Counter *c = malloc(sizeof(Counter));
    if (c) c->value = 0;
    return c;
}

// Helper to check if a string is a valid integer
static int is_valid_int(const char *s, int *out) {
    if (!s || !*s) return 0;
    char *endptr;
    long val = strtol(s, &endptr, 10);
    if (*endptr != '\0') return 0; // Not a valid integer
    if (out) *out = (int)val;
    return 1;
}

static int counter_invoke(void *instance, const char *method,
                          int argc, char **argv,
                          char **result, char **error) {
    Counter *c = (Counter*)instance;
    char buf[256];

    if (strcmp(method, "get") == 0) {
        if (argc != 0) {
            snprintf(buf, sizeof(buf), "wrong # args: expected 0, got %d", argc);
            *error = strdup(buf);
            return 1;
        }
        snprintf(buf, sizeof(buf), "%d", c->value);
        *result = strdup(buf);
        return 0;
    }

    if (strcmp(method, "set") == 0) {
        if (argc != 1) {
            snprintf(buf, sizeof(buf), "wrong # args: expected 1, got %d", argc);
            *error = strdup(buf);
            return 1;
        }
        int val;
        if (!is_valid_int(argv[0], &val)) {
            snprintf(buf, sizeof(buf), "argument 1: expected integer but got \"%s\"", argv[0]);
            *error = strdup(buf);
            return 1;
        }
        c->value = val;
        *result = strdup("");
        return 0;
    }

    if (strcmp(method, "incr") == 0) {
        if (argc != 0) {
            snprintf(buf, sizeof(buf), "wrong # args: expected 0, got %d", argc);
            *error = strdup(buf);
            return 1;
        }
        c->value++;
        snprintf(buf, sizeof(buf), "%d", c->value);
        *result = strdup(buf);
        return 0;
    }

    if (strcmp(method, "add") == 0) {
        if (argc != 1) {
            snprintf(buf, sizeof(buf), "wrong # args: expected 1, got %d", argc);
            *error = strdup(buf);
            return 1;
        }
        int val;
        if (!is_valid_int(argv[0], &val)) {
            snprintf(buf, sizeof(buf), "argument 1: expected integer but got \"%s\"", argv[0]);
            *error = strdup(buf);
            return 1;
        }
        c->value += val;
        snprintf(buf, sizeof(buf), "%d", c->value);
        *result = strdup(buf);
        return 0;
    }

    snprintf(buf, sizeof(buf), "unknown method \"%s\": must be get, set, incr, add, destroy", method);
    *error = strdup(buf);
    return 1;
}

static void counter_destroy(void *instance) {
    free(instance);
}

// -----------------------------------------------------------------------------
// Register Commands
// -----------------------------------------------------------------------------

static void register_test_commands(FeatherInterp interp) {
    // Set milestone variables (like Go version)
    FeatherObj milestone = FeatherString(interp, "m1", 2);
    FeatherSetVar(interp, "milestone", milestone);
    FeatherSetVar(interp, "current-step", milestone);

    // Register test commands
    FeatherRegisterCommand(interp, "say-hello", cmd_say_hello, NULL);
    FeatherRegisterCommand(interp, "echo", cmd_echo, NULL);
    FeatherRegisterCommand(interp, "count", cmd_count, NULL);
    FeatherRegisterCommand(interp, "list", cmd_list, NULL);

    // Register Counter type
    FeatherRegisterForeign(interp, "Counter", counter_new, counter_invoke, counter_destroy, NULL);

    // Register Counter methods for info methods to work
    FeatherRegisterForeignMethod(interp, "Counter", "get");
    FeatherRegisterForeignMethod(interp, "Counter", "set");
    FeatherRegisterForeignMethod(interp, "Counter", "incr");
    FeatherRegisterForeignMethod(interp, "Counter", "add");
}

// -----------------------------------------------------------------------------
// REPL Mode
// -----------------------------------------------------------------------------

static void run_repl(FeatherInterp interp) {
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

        // Evaluate (we don't have a separate parse check in the new API)
        FeatherObj result;
        int status = FeatherEval(interp, input_buffer, strlen(input_buffer), &result);

        if (status != FEATHER_OK) {
            size_t errLen;
            char *errMsg = FeatherGetString(result, interp, &errLen);
            fprintf(stderr, "error: %s\n", errMsg ? errMsg : "unknown error");
            if (errMsg) FeatherFreeString(errMsg);
        } else {
            size_t resLen;
            char *resStr = FeatherGetString(result, interp, &resLen);
            if (resStr && resStr[0]) {
                printf("%s\n", resStr);
            }
            if (resStr) FeatherFreeString(resStr);
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

static void run_script(FeatherInterp interp) {
    size_t script_len;
    char *script = read_all_stdin(&script_len);
    if (!script) {
        fprintf(stderr, "error reading script\n");
        write_harness_result("TCL_ERROR", "", "error reading script");
        exit(1);
    }

    // Check parse status first (like Go version)
    FeatherObj parseResultObj = 0;
    char *parseErrorMsg = NULL;
    int parseStatus = FeatherParseResult(interp, script, script_len, &parseResultObj, &parseErrorMsg);

    if (parseStatus == FEATHER_PARSE_INCOMPLETE) {
        // Return parse result as TCL_OK with the INCOMPLETE info
        size_t resLen;
        char *resStr = FeatherGetString(parseResultObj, interp, &resLen);
        write_harness_result("TCL_OK", resStr ? resStr : "", "");
        if (resStr) FeatherFreeString(resStr);
        free(script);
        exit(2);
    }

    if (parseStatus == FEATHER_PARSE_ERROR) {
        // Return parse result with error info
        size_t resLen;
        char *resStr = FeatherGetString(parseResultObj, interp, &resLen);
        write_harness_result("TCL_ERROR", resStr ? resStr : "", parseErrorMsg ? parseErrorMsg : "");
        if (resStr) FeatherFreeString(resStr);
        if (parseErrorMsg) FeatherFreeString(parseErrorMsg);
        free(script);
        exit(3);
    }

    // Parse OK - evaluate the script
    FeatherObj result;
    int status = FeatherEval(interp, script, script_len, &result);
    free(script);

    size_t resLen;
    char *resStr = FeatherGetString(result, interp, &resLen);

    if (status != FEATHER_OK) {
        if (resStr && resStr[0]) {
            printf("%s\n", resStr);
        }
        write_harness_result("TCL_ERROR", "", resStr ? resStr : "");
        if (resStr) FeatherFreeString(resStr);
        exit(1);
    }

    if (resStr && resStr[0]) {
        printf("%s\n", resStr);
    }
    write_harness_result("TCL_OK", resStr ? resStr : "", "");
    if (resStr) FeatherFreeString(resStr);
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
    FeatherInterp interp = FeatherNew();
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
