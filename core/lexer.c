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
        } else if (*p == '"' || *p == '{' || *p == '}') {
            /* These end a bare word unless escaped */
            break;
        } else {
            p++;
        }
    }

    word->text = (char *)start;
    word->len = p - start;
    word->type = WORD_BARE;

    return p;
}

/* Process backslash substitutions in a string */
static char *process_backslashes(const char *src, size_t len, size_t *outlen) {
    static char buf[MAX_WORD_LEN];
    char *dst = buf;
    const char *end = src + len;

    while (src < end) {
        if (*src == '\\' && src + 1 < end) {
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
                    /* Backslash-newline: replace with single space, skip leading whitespace */
                    src++;
                    while (src < end && (*src == ' ' || *src == '\t')) src++;
                    *dst++ = ' ';
                    break;
                default:
                    *dst++ = *src++;
                    break;
            }
        } else {
            *dst++ = *src++;
        }
    }

    *dst = '\0';
    *outlen = dst - buf;
    return buf;
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
        /* Bare words and quotes: process backslash substitutions */
        return process_backslashes(word->text, word->len, len);
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

/* Parse a single command from the input */
static const char *parse_command(const char *p, Command *cmd, int *line_count) {
    static char line_buf[MAX_WORD_LEN * 4];
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

    /* Check for -nonewline flag */
    if (cmd->count >= 2) {
        size_t len;
        char *arg1 = get_word_value(&cmd->words[1], &len);
        if (strcmp(arg1, "-nonewline") == 0) {
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

    /* Output the string */
    fwrite(str, 1, str_len, channel);
    if (newline) fputc('\n', channel);

    return 0;
}

/* Execute a command */
static int execute_command(Command *cmd) {
    if (cmd->count == 0) return 0;  /* Empty command */

    size_t len;
    char *cmdname = get_word_value(&cmd->words[0], &len);

    if (strcmp(cmdname, "puts") == 0) {
        return cmd_puts(cmd);
    } else {
        tcl_error("invalid command name \"%s\"", cmdname);
        return 1;
    }
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
