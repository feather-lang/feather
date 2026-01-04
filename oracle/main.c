/*
 * Oracle: Reference TCL interpreter for test comparison
 *
 * This embeds the real TCL interpreter to provide ground truth
 * for feather's behavior.
 */

#include <tcl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <limits.h>

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

/* Benchmark mode implementation */
typedef struct {
    char *name;
    char *setup;
    char *script;
    int warmup;
    int iterations;
} Benchmark;

typedef struct {
    int success;
    long long total_time_ns;
    long long avg_time_ns;
    long long min_time_ns;
    long long max_time_ns;
    int iterations;
    double ops_per_second;
    char *error;
} BenchmarkResult;

static long long get_time_ns(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000000000LL + (long long)tv.tv_usec * 1000LL;
}

static void run_benchmark(Tcl_Interp *interp, Benchmark *b, BenchmarkResult *result) {
    memset(result, 0, sizeof(*result));
    result->success = 1;
    result->iterations = 0;
    result->min_time_ns = LLONG_MAX;
    result->max_time_ns = 0;

    /* Run setup if provided */
    if (b->setup && b->setup[0] != '\0') {
        if (Tcl_Eval(interp, b->setup) != TCL_OK) {
            result->success = 0;
            result->error = strdup(Tcl_GetStringResult(interp));
            return;
        }
    }

    /* Warmup iterations */
    for (int i = 0; i < b->warmup; i++) {
        if (Tcl_Eval(interp, b->script) != TCL_OK) {
            result->success = 0;
            result->error = strdup(Tcl_GetStringResult(interp));
            return;
        }
    }

    /* Measured iterations */
    for (int i = 0; i < b->iterations; i++) {
        long long start = get_time_ns();
        if (Tcl_Eval(interp, b->script) != TCL_OK) {
            result->success = 0;
            result->error = strdup(Tcl_GetStringResult(interp));
            return;
        }
        long long elapsed = get_time_ns() - start;

        result->total_time_ns += elapsed;
        if (elapsed < result->min_time_ns) {
            result->min_time_ns = elapsed;
        }
        if (elapsed > result->max_time_ns) {
            result->max_time_ns = elapsed;
        }
        result->iterations++;
    }

    /* Calculate statistics */
    if (result->iterations > 0) {
        result->avg_time_ns = result->total_time_ns / result->iterations;
        if (result->avg_time_ns > 0) {
            result->ops_per_second = 1000000000.0 / (double)result->avg_time_ns;
        }
    }
}

/* Escape a string for JSON */
static void fputs_json_escaped(FILE *f, const char *s) {
    if (!s) return;
    while (*s) {
        switch (*s) {
            case '"':  fputs("\\\"", f); break;
            case '\\': fputs("\\\\", f); break;
            case '\n': fputs("\\n", f); break;
            case '\r': fputs("\\r", f); break;
            case '\t': fputs("\\t", f); break;
            default:   fputc(*s, f); break;
        }
        s++;
    }
}

static void write_benchmark_result(FILE *f, Benchmark *b, BenchmarkResult *r) {
    fprintf(f, "{");
    fprintf(f, "\"Benchmark\":{");
    fprintf(f, "\"Name\":\""); fputs_json_escaped(f, b->name); fprintf(f, "\",");
    fprintf(f, "\"Setup\":\""); fputs_json_escaped(f, b->setup); fprintf(f, "\",");
    fprintf(f, "\"Script\":\""); fputs_json_escaped(f, b->script); fprintf(f, "\",");
    fprintf(f, "\"Warmup\":%d,", b->warmup);
    fprintf(f, "\"Iterations\":%d", b->iterations);
    fprintf(f, "},");
    fprintf(f, "\"Success\":%s,", r->success ? "true" : "false");
    fprintf(f, "\"TotalTime\":%lld,", r->total_time_ns);
    fprintf(f, "\"AvgTime\":%lld,", r->avg_time_ns);
    fprintf(f, "\"MinTime\":%lld,", r->min_time_ns);
    fprintf(f, "\"MaxTime\":%lld,", r->max_time_ns);
    fprintf(f, "\"Iterations\":%d,", r->iterations);
    fprintf(f, "\"OpsPerSecond\":%.2f,", r->ops_per_second);
    fprintf(f, "\"Error\":\""); fputs_json_escaped(f, r->error); fprintf(f, "\"");
    fprintf(f, "}\n");
    fflush(f);
}

/* Simple JSON parser for benchmarks */
static char *extract_json_string(const char *json, const char *key) {
    char search[256];
    snprintf(search, sizeof(search), "\"%s\":\"", key);
    const char *start = strstr(json, search);
    if (!start) return strdup("");

    start += strlen(search);

    /* Find end of string, accounting for escapes */
    const char *p = start;
    while (*p) {
        if (*p == '\\' && *(p+1)) {
            p += 2;  /* Skip escaped character */
        } else if (*p == '"') {
            break;  /* Found unescaped quote */
        } else {
            p++;
        }
    }

    /* Allocate and unescape */
    size_t max_len = p - start;
    char *result = malloc(max_len + 1);
    char *out = result;

    while (start < p) {
        if (*start == '\\' && start + 1 < p) {
            start++;
            switch (*start) {
                case 'n':  *out++ = '\n'; break;
                case 'r':  *out++ = '\r'; break;
                case 't':  *out++ = '\t'; break;
                case '\\': *out++ = '\\'; break;
                case '"':  *out++ = '"'; break;
                default:   *out++ = *start; break;
            }
            start++;
        } else {
            *out++ = *start++;
        }
    }
    *out = '\0';
    return result;
}

static int extract_json_int(const char *json, const char *key, int default_val) {
    char search[256];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *start = strstr(json, search);
    if (!start) return default_val;

    start += strlen(search);
    return atoi(start);
}

static void run_benchmark_mode(void) {
    /* Open harness channel */
    FILE *harness = fdopen(3, "w");
    if (!harness) {
        fprintf(stderr, "error: harness channel not available\n");
        exit(1);
    }

    /* Read benchmarks from stdin */
    size_t input_len;
    char *input = read_stdin(&input_len);
    if (!input) {
        fprintf(stderr, "error reading benchmarks\n");
        exit(1);
    }

    /* Create interpreter once for all benchmarks */
    Tcl_Interp *interp = Tcl_CreateInterp();
    if (!interp || Tcl_Init(interp) != TCL_OK) {
        fprintf(stderr, "failed to create interpreter\n");
        exit(1);
    }

    /* Parse and run each benchmark */
    /* This is a simple parser - expects JSON array of benchmark objects */
    const char *p = input;
    while (*p) {
        /* Skip to next benchmark object */
        p = strchr(p, '{');
        if (!p) break;

        /* Find end of this benchmark object */
        const char *end = p;
        int depth = 0;
        do {
            if (*end == '{') depth++;
            else if (*end == '}') depth--;
            end++;
        } while (depth > 0 && *end);

        /* Extract benchmark object */
        size_t obj_len = end - p;
        char *obj = malloc(obj_len + 1);
        memcpy(obj, p, obj_len);
        obj[obj_len] = '\0';

        /* Parse benchmark fields */
        Benchmark bench;
        bench.name = extract_json_string(obj, "Name");
        bench.setup = extract_json_string(obj, "Setup");
        bench.script = extract_json_string(obj, "Script");
        bench.warmup = extract_json_int(obj, "Warmup", 0);
        bench.iterations = extract_json_int(obj, "Iterations", 1000);

        /* Run benchmark */
        BenchmarkResult result;
        run_benchmark(interp, &bench, &result);

        /* Write result */
        write_benchmark_result(harness, &bench, &result);

        /* Cleanup */
        free(bench.name);
        free(bench.setup);
        free(bench.script);
        if (result.error) free(result.error);
        free(obj);

        p = end;
    }

    Tcl_DeleteInterp(interp);
    free(input);
    fclose(harness);
}

int main(int argc, char *argv[]) {
    /* Check for benchmark mode */
    if (argc > 1 && strcmp(argv[1], "--benchmark") == 0) {
        run_benchmark_mode();
        return 0;
    }

    /* Check if running in harness mode */
    const char *harness_env = getenv("FEATHER_IN_HARNESS");
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
