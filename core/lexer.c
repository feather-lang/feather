/*
 * lexer.c - TCL Lexer and Minimal Interpreter
 *
 * This implements a minimal TCL interpreter sufficient to pass lexer tests.
 * It tokenizes TCL source respecting quoting rules and executes basic commands.
 *
 * Build: cc -o tclc core/lexer.c
 * Usage: ./tclc < script.tcl
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define MAX_WORDS 256
#define MAX_WORD_LEN 4096
#define MAX_SCRIPT_SIZE (1024 * 1024)  /* 1MB max script */
#define MAX_VARS 256
#define MAX_VAR_NAME 256

/* Simple variable storage */
typedef struct {
    char name[MAX_VAR_NAME];
    char value[MAX_WORD_LEN];
    size_t value_len;
} Variable;

static Variable variables[MAX_VARS];
static int var_count = 0;

/* Variable operations */
static Variable *var_find(const char *name, size_t len) {
    for (int i = 0; i < var_count; i++) {
        if (strlen(variables[i].name) == len &&
            strncmp(variables[i].name, name, len) == 0) {
            return &variables[i];
        }
    }
    return NULL;
}

static void var_set(const char *name, size_t name_len, const char *value, size_t value_len) {
    Variable *v = var_find(name, name_len);
    if (!v) {
        if (var_count >= MAX_VARS) return;
        v = &variables[var_count++];
        if (name_len >= MAX_VAR_NAME) name_len = MAX_VAR_NAME - 1;
        memcpy(v->name, name, name_len);
        v->name[name_len] = '\0';
    }
    if (value_len >= MAX_WORD_LEN) value_len = MAX_WORD_LEN - 1;
    memcpy(v->value, value, value_len);
    v->value[value_len] = '\0';
    v->value_len = value_len;
}

static char *var_get(const char *name, size_t len, size_t *out_len) {
    Variable *v = var_find(name, len);
    if (v) {
        *out_len = v->value_len;
        return v->value;
    }
    return NULL;
}

/* Command result for substitution */
static char cmd_result[MAX_WORD_LEN];
static size_t cmd_result_len = 0;

static void set_result(const char *s, size_t len) {
    if (len >= MAX_WORD_LEN) len = MAX_WORD_LEN - 1;
    memcpy(cmd_result, s, len);
    cmd_result[len] = '\0';
    cmd_result_len = len;
}

static void set_result_int(long long val) {
    cmd_result_len = sprintf(cmd_result, "%lld", val);
}

/* Word types */
typedef enum {
    WORD_BARE,      /* Unquoted word */
    WORD_BRACES,    /* {braced} */
    WORD_QUOTES,    /* "quoted" */
} WordType;

typedef struct {
    char *text;
    size_t len;
    WordType type;
} Word;

typedef struct {
    Word words[MAX_WORDS];
    int count;
} Command;

/* Forward declarations */
static int execute_command(Command *cmd);
static const char *parse_command(const char *p, Command *cmd, int *line_count);

/* Error tracking */
static const char *script_file = NULL;
static int current_line = 0;
static int error_occurred = 0;
static int error_line = 0;
static char error_command[MAX_WORD_LEN];

/* Report error in TCL format */
static void tcl_error(const char *msg, ...) {
    va_list ap;
    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    va_end(ap);
    fprintf(stderr, "\n");

    if (error_command[0]) {
        fprintf(stderr, "    while executing\n\"%s\"\n", error_command);
    }
    if (script_file) {
        fprintf(stderr, "    (file \"%s\" line %d)\n", script_file, error_line);
    }
    error_occurred = 1;
}

/* Skip whitespace (not newlines), including backslash-newline */
static const char *skip_space(const char *p) {
    for (;;) {
        if (*p == ' ' || *p == '\t') {
            p++;
        } else if (*p == '\\' && p[1] == '\n') {
            /* Backslash-newline acts as whitespace */
            p += 2;
        } else {
            break;
        }
    }
    return p;
}

/* Parse a brace-quoted word */
static const char *parse_braces(const char *p, Word *word) {
    int depth = 1;
    const char *start = p;  /* After opening { */

    while (*p && depth > 0) {
        if (*p == '\\' && p[1]) {
            p += 2;  /* Skip escaped char, including \{ and \} */
        } else if (*p == '{') {
            depth++;
            p++;
        } else if (*p == '}') {
            depth--;
            if (depth > 0) p++;
        } else {
            p++;
        }
    }

    if (depth != 0) {
        tcl_error("missing close-brace");
        return NULL;
    }

    word->text = (char *)start;
    word->len = p - start;
    word->type = WORD_BRACES;

    return p + 1;  /* Skip closing } */
}

/* Parse a double-quoted word */
static const char *parse_quotes(const char *p, Word *word) {
    const char *start = p;  /* After opening " */

    while (*p && *p != '"') {
        if (*p == '\\' && p[1]) {
            p += 2;  /* Skip escaped char */
        } else {
            p++;
        }
    }

    if (*p != '"') {
        tcl_error("missing \"");
        return NULL;
    }

    word->text = (char *)start;
    word->len = p - start;
    word->type = WORD_QUOTES;

    return p + 1;  /* Skip closing " */
}

/* Parse a bare word */
static const char *parse_bare_word(const char *p, Word *word) {
    const char *start = p;

    while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != ';') {
        if (*p == '\\' && p[1]) {
            /* Backslash-newline ends this word (acts as whitespace) */
            if (p[1] == '\n') {
                break;
            } else {
                p += 2;  /* Skip other escaped chars */
            }
        } else if (*p == '$' && p[1] == '{') {
            /* ${varname} - scan to matching } */
            p += 2;  /* Skip ${ */
            while (*p && *p != '}') p++;
            if (*p == '}') p++;
        } else if (*p == '"' || *p == '{' || *p == '}') {
            /* These end a bare word unless escaped */
            break;
        } else if (*p == '[') {
            /* Command substitution - scan to matching ] */
            int depth = 1;
            p++;
            while (*p && depth > 0) {
                if (*p == '[') depth++;
                else if (*p == ']') depth--;
                if (depth > 0) p++;
            }
            if (*p == ']') p++;
        } else {
            p++;
        }
    }

    word->text = (char *)start;
    word->len = p - start;
    word->type = WORD_BARE;

    return p;
}

/* Execute a bracketed command and return its result */
static int execute_bracketed_command(const char *src, size_t len, char *result, size_t *result_len);

/* Substitution buffer stack to handle recursive calls */
static char subst_bufs[32][MAX_WORD_LEN];
static int subst_buf_idx = 0;

/* Full substitution: backslashes, variables, and commands */
static char *do_substitution(const char *src, size_t len, size_t *outlen, int do_vars, int do_cmds, int do_backslash) {
    /* Use a buffer from the stack */
    if (subst_buf_idx >= 32) {
        *outlen = 0;
        return subst_bufs[0];  /* Emergency fallback */
    }
    char *buf = subst_bufs[subst_buf_idx++];
    char *dst = buf;
    const char *end = src + len;

    while (src < end && (dst - buf) < MAX_WORD_LEN - 100) {
        if (do_backslash && *src == '\\' && src + 1 < end) {
            src++;
            switch (*src) {
                case 'n': *dst++ = '\n'; src++; break;
                case 't': *dst++ = '\t'; src++; break;
                case 'r': *dst++ = '\r'; src++; break;
                case '\\': *dst++ = '\\'; src++; break;
                case '"': *dst++ = '"'; src++; break;
                case '{': *dst++ = '{'; src++; break;
                case '}': *dst++ = '}'; src++; break;
                case '[': *dst++ = '['; src++; break;
                case ']': *dst++ = ']'; src++; break;
                case '$': *dst++ = '$'; src++; break;
                case '\n':
                    src++;
                    while (src < end && (*src == ' ' || *src == '\t')) src++;
                    *dst++ = ' ';
                    break;
                default:
                    *dst++ = *src++;
                    break;
            }
        } else if (do_vars && *src == '$') {
            src++;
            const char *var_name;
            size_t var_name_len;

            if (*src == '{') {
                /* ${varname} */
                src++;
                var_name = src;
                while (src < end && *src != '}') src++;
                var_name_len = src - var_name;
                if (src < end) src++;  /* skip } */
            } else {
                /* $varname - alphanumeric and underscore */
                var_name = src;
                while (src < end && ((*src >= 'a' && *src <= 'z') ||
                       (*src >= 'A' && *src <= 'Z') ||
                       (*src >= '0' && *src <= '9') || *src == '_')) {
                    src++;
                }
                var_name_len = src - var_name;

                /* Check for array element: $arr(key) */
                if (src < end && *src == '(') {
                    src++;
                    const char *key_start = src;
                    int depth = 1;
                    while (src < end && depth > 0) {
                        if (*src == '(') depth++;
                        else if (*src == ')') depth--;
                        if (depth > 0) src++;
                    }
                    size_t key_len = src - key_start;
                    if (src < end) src++;  /* skip ) */

                    /* Build array variable name: arr(key) */
                    char arr_name[MAX_VAR_NAME];
                    size_t arr_name_len = var_name_len + 1 + key_len + 1;
                    if (arr_name_len < MAX_VAR_NAME) {
                        memcpy(arr_name, var_name, var_name_len);
                        arr_name[var_name_len] = '(';
                        memcpy(arr_name + var_name_len + 1, key_start, key_len);
                        arr_name[var_name_len + 1 + key_len] = ')';
                        arr_name[arr_name_len] = '\0';

                        size_t val_len;
                        char *val = var_get(arr_name, arr_name_len, &val_len);
                        if (val) {
                            memcpy(dst, val, val_len);
                            dst += val_len;
                        } else {
                            tcl_error("can't read \"%.*s\": no such variable", (int)arr_name_len, arr_name);
                            error_occurred = 1;
                            *outlen = 0;
                            return buf;
                        }
                    }
                    continue;
                }
            }

            if (var_name_len > 0) {
                size_t val_len;
                char *val = var_get(var_name, var_name_len, &val_len);
                if (val) {
                    memcpy(dst, val, val_len);
                    dst += val_len;
                } else {
                    tcl_error("can't read \"%.*s\": no such variable", (int)var_name_len, var_name);
                    error_occurred = 1;
                    *outlen = 0;
                    return buf;
                }
            } else {
                *dst++ = '$';
            }
        } else if (do_cmds && *src == '[') {
            src++;
            /* Find matching ] */
            int depth = 1;
            const char *cmd_start = src;
            while (src < end && depth > 0) {
                if (*src == '[') depth++;
                else if (*src == ']') depth--;
                if (depth > 0) src++;
            }
            size_t cmd_len = src - cmd_start;
            if (src < end) src++;  /* skip ] */

            /* Execute the command */
            char sub_result[MAX_WORD_LEN];
            size_t sub_result_len;
            if (execute_bracketed_command(cmd_start, cmd_len, sub_result, &sub_result_len) == 0) {
                memcpy(dst, sub_result, sub_result_len);
                dst += sub_result_len;
            } else {
                *outlen = 0;
                return buf;
            }
        } else {
            *dst++ = *src++;
        }
    }

    *dst = '\0';
    *outlen = dst - buf;
    return buf;
}

/* Process backslash substitutions only */
static char *process_backslashes(const char *src, size_t len, size_t *outlen) {
    return do_substitution(src, len, outlen, 0, 0, 1);
}

/* Get the string value of a word (with substitutions if needed) */
static char *get_word_value(Word *word, size_t *len) {
    static char buf[MAX_WORD_LEN];

    if (word->type == WORD_BRACES) {
        /* Braces: literal, no substitution */
        if (word->len >= MAX_WORD_LEN) word->len = MAX_WORD_LEN - 1;
        memcpy(buf, word->text, word->len);
        buf[word->len] = '\0';
        *len = word->len;
        return buf;
    } else {
        /* Bare words and quotes: full substitution */
        return do_substitution(word->text, word->len, len, 1, 1, 1);
    }
}

/* Preprocess script to handle backslash-newline continuations in command building */
static const char *preprocess_command_line(const char *p, char *out, size_t *out_len, int *lines_consumed) {
    char *dst = out;
    *lines_consumed = 1;

    while (*p && *p != '\n' && *p != ';') {
        if (*p == '\\' && p[1] == '\n') {
            /* Backslash-newline continuation */
            *dst++ = '\\';
            *dst++ = '\n';
            p += 2;
            (*lines_consumed)++;
            /* Skip leading whitespace on continuation line */
            while (*p == ' ' || *p == '\t') {
                *dst++ = *p++;
            }
        } else if (*p == '{') {
            /* Braces: scan to matching close brace */
            int depth = 1;
            *dst++ = *p++;
            while (*p && depth > 0) {
                if (*p == '\n') (*lines_consumed)++;
                if (*p == '\\' && p[1]) {
                    *dst++ = *p++;
                    *dst++ = *p++;
                } else if (*p == '{') {
                    depth++;
                    *dst++ = *p++;
                } else if (*p == '}') {
                    depth--;
                    *dst++ = *p++;
                } else {
                    *dst++ = *p++;
                }
            }
        } else if (*p == '"') {
            /* Quotes: scan to closing quote */
            *dst++ = *p++;
            while (*p && *p != '"') {
                if (*p == '\n') (*lines_consumed)++;
                if (*p == '\\' && p[1]) {
                    *dst++ = *p++;
                    *dst++ = *p++;
                } else {
                    *dst++ = *p++;
                }
            }
            if (*p == '"') *dst++ = *p++;
        } else {
            *dst++ = *p++;
        }
    }

    *dst = '\0';
    *out_len = dst - out;

    /* Skip the terminator */
    if (*p == ';') p++;
    else if (*p == '\n') p++;

    return p;
}

/* Parse command buffer pool to handle recursive calls */
static char parse_bufs[8][MAX_WORD_LEN * 4];
static int parse_buf_idx = 0;

/* Parse a single command from the input */
static const char *parse_command(const char *p, Command *cmd, int *line_count) {
    if (parse_buf_idx >= 8) {
        return NULL;  /* Too deeply nested */
    }
    char *line_buf = parse_bufs[parse_buf_idx++];
    size_t line_len;
    int lines_consumed;

    cmd->count = 0;

    /* Skip blank lines and whitespace */
    while (*p == ' ' || *p == '\t' || *p == '\n') {
        if (*p == '\n') (*line_count)++;
        p++;
    }

    if (*p == '\0') return p;

    /* Check for comment */
    if (*p == '#') {
        /* Skip to end of line */
        while (*p && *p != '\n') p++;
        if (*p == '\n') { (*line_count)++; p++; }
        return p;
    }

    /* Record line number for error messages */
    error_line = *line_count;

    /* Get the logical command line (handling continuations and multi-line strings) */
    p = preprocess_command_line(p, line_buf, &line_len, &lines_consumed);
    *line_count += lines_consumed - 1;

    /* Now parse words from line_buf */
    const char *lp = line_buf;
    lp = skip_space(lp);

    while (*lp && *lp != '\0') {
        if (cmd->count >= MAX_WORDS) {
            tcl_error("too many words in command");
            return NULL;
        }

        Word *word = &cmd->words[cmd->count];

        if (*lp == '{') {
            lp++;
            lp = parse_braces(lp, word);
            if (!lp) return NULL;
        } else if (*lp == '"') {
            lp++;
            lp = parse_quotes(lp, word);
            if (!lp) return NULL;
        } else {
            lp = parse_bare_word(lp, word);
        }

        cmd->count++;
        lp = skip_space(lp);
    }

    /* Store command text for error messages */
    if (cmd->count > 0 && line_len < MAX_WORD_LEN - 1) {
        memcpy(error_command, line_buf, line_len);
        error_command[line_len] = '\0';
    }

    return p;
}

/* Execute the puts command */
static int cmd_puts(Command *cmd) {
    int newline = 1;
    int arg_start = 1;
    FILE *channel = stdout;

    /* Check for -nonewline flag (check raw text to avoid substitution) */
    if (cmd->count >= 2) {
        Word *w = &cmd->words[1];
        if (w->len == 10 && strncmp(w->text, "-nonewline", 10) == 0) {
            newline = 0;
            arg_start = 2;
        }
    }

    /* Check argument count */
    int remaining = cmd->count - arg_start;
    if (remaining < 1 || remaining > 2) {
        tcl_error("wrong # args: should be \"puts ?-nonewline? ?channelId? string\"");
        return 1;
    }

    /* Get the string to print */
    size_t str_len;
    char *str;

    if (remaining == 2) {
        /* Channel and string */
        size_t chan_len;
        char *chan = get_word_value(&cmd->words[arg_start], &chan_len);

        if (strcmp(chan, "stdout") == 0) {
            channel = stdout;
        } else if (strcmp(chan, "stderr") == 0) {
            channel = stderr;
        } else {
            tcl_error("can not find channel named \"%s\"", chan);
            return 1;
        }
        str = get_word_value(&cmd->words[arg_start + 1], &str_len);
    } else {
        str = get_word_value(&cmd->words[arg_start], &str_len);
    }
    if (error_occurred) return 1;

    /* Output the string */
    fwrite(str, 1, str_len, channel);
    if (newline) fputc('\n', channel);

    set_result("", 0);
    return 0;
}

/* Execute the set command */
static int cmd_set(Command *cmd) {
    if (cmd->count < 2 || cmd->count > 3) {
        tcl_error("wrong # args: should be \"set varName ?newValue?\"");
        return 1;
    }

    /* Get variable name and make a copy (get_word_value uses static buffer) */
    size_t name_len;
    char *name_ptr = get_word_value(&cmd->words[1], &name_len);
    if (error_occurred) return 1;
    char name[MAX_VAR_NAME];
    if (name_len >= MAX_VAR_NAME) name_len = MAX_VAR_NAME - 1;
    memcpy(name, name_ptr, name_len);
    name[name_len] = '\0';

    if (cmd->count == 3) {
        /* Setting a value */
        size_t val_len;
        char *val = get_word_value(&cmd->words[2], &val_len);
        if (error_occurred) return 1;

        var_set(name, name_len, val, val_len);
        set_result(val, val_len);
    } else {
        /* Getting a value */
        size_t val_len;
        char *val = var_get(name, name_len, &val_len);
        if (!val) {
            tcl_error("can't read \"%s\": no such variable", name);
            return 1;
        }
        set_result(val, val_len);
    }

    return 0;
}

/* Execute the string command */
static int cmd_string(Command *cmd) {
    if (cmd->count < 2) {
        tcl_error("wrong # args: should be \"string subcommand ?arg ...?\"");
        return 1;
    }

    size_t subcmd_len;
    char *subcmd = get_word_value(&cmd->words[1], &subcmd_len);
    if (error_occurred) return 1;

    if (strcmp(subcmd, "length") == 0) {
        if (cmd->count != 3) {
            tcl_error("wrong # args: should be \"string length string\"");
            return 1;
        }
        size_t str_len;
        get_word_value(&cmd->words[2], &str_len);
        if (error_occurred) return 1;
        set_result_int((long long)str_len);

    } else if (strcmp(subcmd, "toupper") == 0) {
        if (cmd->count != 3) {
            tcl_error("wrong # args: should be \"string toupper string\"");
            return 1;
        }
        size_t str_len;
        char *str = get_word_value(&cmd->words[2], &str_len);
        if (error_occurred) return 1;

        static char upper[MAX_WORD_LEN];
        for (size_t i = 0; i < str_len && i < MAX_WORD_LEN - 1; i++) {
            upper[i] = (str[i] >= 'a' && str[i] <= 'z') ? str[i] - 32 : str[i];
        }
        upper[str_len] = '\0';
        set_result(upper, str_len);

    } else if (strcmp(subcmd, "tolower") == 0) {
        if (cmd->count != 3) {
            tcl_error("wrong # args: should be \"string tolower string\"");
            return 1;
        }
        size_t str_len;
        char *str = get_word_value(&cmd->words[2], &str_len);
        if (error_occurred) return 1;

        static char lower[MAX_WORD_LEN];
        for (size_t i = 0; i < str_len && i < MAX_WORD_LEN - 1; i++) {
            lower[i] = (str[i] >= 'A' && str[i] <= 'Z') ? str[i] + 32 : str[i];
        }
        lower[str_len] = '\0';
        set_result(lower, str_len);

    } else if (strcmp(subcmd, "repeat") == 0) {
        if (cmd->count != 4) {
            tcl_error("wrong # args: should be \"string repeat string count\"");
            return 1;
        }
        /* Copy string value to local buffer since get_word_value uses shared buffers */
        size_t str_len;
        char *str_ptr = get_word_value(&cmd->words[2], &str_len);
        if (error_occurred) return 1;
        char str[MAX_WORD_LEN];
        memcpy(str, str_ptr, str_len);
        str[str_len] = '\0';

        size_t count_len;
        char *count_str_ptr = get_word_value(&cmd->words[3], &count_len);
        if (error_occurred) return 1;
        char count_str[64];
        memcpy(count_str, count_str_ptr, count_len < 64 ? count_len : 63);
        count_str[count_len < 64 ? count_len : 63] = '\0';
        int count = atoi(count_str);

        static char repeated[MAX_WORD_LEN];
        size_t pos = 0;
        for (int i = 0; i < count && pos + str_len < MAX_WORD_LEN - 1; i++) {
            memcpy(repeated + pos, str, str_len);
            pos += str_len;
        }
        repeated[pos] = '\0';
        set_result(repeated, pos);

    } else {
        tcl_error("unknown or ambiguous subcommand \"%s\"", subcmd);
        return 1;
    }

    return 0;
}

/* Execute the expr command (simple arithmetic) */
static int cmd_expr(Command *cmd) {
    if (cmd->count < 2) {
        tcl_error("wrong # args: should be \"expr arg ?arg ...?\"");
        return 1;
    }

    /* Concatenate all args with spaces */
    static char expr_buf[MAX_WORD_LEN];
    size_t pos = 0;
    for (int i = 1; i < cmd->count; i++) {
        size_t arg_len;
        char *arg = get_word_value(&cmd->words[i], &arg_len);
        if (error_occurred) return 1;
        if (pos > 0 && pos < MAX_WORD_LEN - 1) expr_buf[pos++] = ' ';
        if (pos + arg_len < MAX_WORD_LEN - 1) {
            memcpy(expr_buf + pos, arg, arg_len);
            pos += arg_len;
        }
    }
    expr_buf[pos] = '\0';

    /* Simple expression parser: supports + - * / and integers */
    const char *p = expr_buf;
    while (*p == ' ') p++;

    long long result = 0;
    int have_result = 0;
    char op = '+';

    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;

        /* Parse number */
        int neg = 0;
        if (*p == '-' && !have_result) {
            neg = 1;
            p++;
        } else if (*p == '+' && !have_result) {
            p++;
        }

        long long num = 0;
        while (*p >= '0' && *p <= '9') {
            num = num * 10 + (*p - '0');
            p++;
        }
        if (neg) num = -num;

        if (!have_result) {
            result = num;
            have_result = 1;
        } else {
            switch (op) {
                case '+': result += num; break;
                case '-': result -= num; break;
                case '*': result *= num; break;
                case '/': if (num != 0) result /= num; break;
            }
        }

        while (*p == ' ') p++;
        if (*p == '+' || *p == '-' || *p == '*' || *p == '/') {
            op = *p++;
        }
    }

    set_result_int(result);
    return 0;
}

/* Execute the subst command */
static int cmd_subst(Command *cmd) {
    if (cmd->count < 2) {
        tcl_error("wrong # args: should be \"subst ?-nobackslashes? ?-nocommands? ?-novariables? string\"");
        return 1;
    }

    int do_backslash = 1;
    int do_commands = 1;
    int do_variables = 1;
    int string_idx = 1;

    /* Parse flags */
    for (int i = 1; i < cmd->count - 1; i++) {
        size_t arg_len;
        char *arg = get_word_value(&cmd->words[i], &arg_len);
        if (error_occurred) return 1;

        if (strcmp(arg, "-nobackslashes") == 0) {
            do_backslash = 0;
            string_idx = i + 1;
        } else if (strcmp(arg, "-nocommands") == 0) {
            do_commands = 0;
            string_idx = i + 1;
        } else if (strcmp(arg, "-novariables") == 0) {
            do_variables = 0;
            string_idx = i + 1;
        }
    }

    /* Get the string (without substitution - use raw text) */
    Word *w = &cmd->words[string_idx];
    const char *raw_text = w->text;
    size_t raw_len = w->len;

    /* Perform substitution */
    size_t result_len;
    char *result = do_substitution(raw_text, raw_len, &result_len, do_variables, do_commands, do_backslash);
    if (error_occurred) return 1;

    set_result(result, result_len);
    return 0;
}

/* Execute a command */
static int execute_command(Command *cmd) {
    if (cmd->count == 0) return 0;  /* Empty command */

    size_t len;
    char *cmdname = get_word_value(&cmd->words[0], &len);
    if (error_occurred) return 1;

    if (strcmp(cmdname, "puts") == 0) {
        return cmd_puts(cmd);
    } else if (strcmp(cmdname, "set") == 0) {
        return cmd_set(cmd);
    } else if (strcmp(cmdname, "string") == 0) {
        return cmd_string(cmd);
    } else if (strcmp(cmdname, "expr") == 0) {
        return cmd_expr(cmd);
    } else if (strcmp(cmdname, "subst") == 0) {
        return cmd_subst(cmd);
    } else {
        tcl_error("invalid command name \"%s\"", cmdname);
        return 1;
    }
}

/* Bracketed command buffer pool */
static char bracket_bufs[8][MAX_WORD_LEN];
static int bracket_buf_idx = 0;

/* Execute a bracketed command and return its result */
static int execute_bracketed_command(const char *src, size_t len, char *result, size_t *result_len) {
    /* Use a buffer from the pool */
    if (bracket_buf_idx >= 8) {
        return 1;  /* Too deeply nested */
    }
    char *script_buf = bracket_bufs[bracket_buf_idx++];
    if (len >= MAX_WORD_LEN) len = MAX_WORD_LEN - 1;
    memcpy(script_buf, src, len);
    script_buf[len] = '\0';

    /* Parse and execute */
    Command cmd;
    int line = 1;
    const char *p = script_buf;

    /* Save error state */
    char saved_error_cmd[MAX_WORD_LEN];
    int saved_error_line = error_line;
    strcpy(saved_error_cmd, error_command);

    p = parse_command(p, &cmd, &line);
    if (!p || error_occurred) {
        return 1;
    }

    if (cmd.count > 0) {
        int res = execute_command(&cmd);
        if (res != 0) {
            return 1;
        }
    }

    /* Return the result */
    memcpy(result, cmd_result, cmd_result_len);
    result[cmd_result_len] = '\0';
    *result_len = cmd_result_len;

    /* Restore error state */
    error_line = saved_error_line;
    strcpy(error_command, saved_error_cmd);

    return 0;
}

/* Execute a TCL script */
static int execute_script(const char *script, const char *filename) {
    Command cmd;
    int line = 1;
    const char *p = script;

    script_file = filename;
    error_occurred = 0;
    error_command[0] = '\0';

    while (*p) {
        current_line = line;
        subst_buf_idx = 0;  /* Reset substitution buffers for each command */
        parse_buf_idx = 0;  /* Reset parse buffers for each command */
        bracket_buf_idx = 0;  /* Reset bracket command buffers */
        p = parse_command(p, &cmd, &line);
        if (!p || error_occurred) return 1;

        if (cmd.count > 0) {
            int result = execute_command(&cmd);
            if (result != 0) return 1;
        }
    }

    return 0;
}

/* Read entire file/stdin into buffer */
static char *read_all(FILE *f, size_t *size) {
    static char buf[MAX_SCRIPT_SIZE];
    *size = fread(buf, 1, MAX_SCRIPT_SIZE - 1, f);
    buf[*size] = '\0';
    return buf;
}

int main(int argc, char **argv) {
    const char *filename = NULL;
    FILE *input = stdin;

    if (argc > 1) {
        filename = argv[1];
        input = fopen(filename, "r");
        if (!input) {
            fprintf(stderr, "couldn't read file \"%s\": no such file or directory\n", filename);
            return 1;
        }
    }

    size_t size;
    char *script = read_all(input, &size);

    if (input != stdin) fclose(input);

    int result = execute_script(script, filename);

    return result;
}
