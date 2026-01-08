/*
 * console.c - In-game TCL console implementation
 */

#include "console.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* Forward declarations */
static void console_submit(Console* c);
static void console_handle_char(Console* c, int ch);
static void console_insert_char(Console* c, char ch);
static void console_delete_char(Console* c);
static void console_backspace(Console* c);
static void console_move_cursor(Console* c, int delta);
static void console_kill_line(Console* c);
static void console_kill_to_end(Console* c);
static int console_count_lines(Console* c);
static const char* console_get_line(Console* c, int line_from_bottom, int* out_len);

/* Custom puts command that prints to console and stdout */
static int cmd_puts(void *data, FeatherInterp interp,
                    size_t argc, FeatherObj *argv,
                    FeatherObj *result, FeatherObj *err) {
    Console* c = (Console*)data;

    if (argc < 1) {
        *err = FeatherString(interp, "wrong # args: should be \"puts string\"", 38);
        return 1;
    }

    char buf[1024];
    size_t len = FeatherCopy(interp, argv[0], buf, sizeof(buf) - 1);
    buf[len] = '\0';

    /* Print to console scrollback */
    console_print(c, buf);

    /* Also print to stdout */
    printf("%s\n", buf);
    fflush(stdout);

    *result = 0;
    return 0;
}

Console* console_new(FeatherInterp interp) {
    Console* c = (Console*)malloc(sizeof(Console));
    if (!c) return NULL;

    memset(c, 0, sizeof(Console));
    c->interp = interp;
    c->height = GetScreenHeight() / 2;  /* Half screen height */
    c->visible = 0;

    /* Print welcome message */
    console_print(c, "Feather Console");
    console_print(c, "");
    console_print(c, "Drawing: draw_circle draw_rect draw_line draw_ring draw_text clear");
    console_print(c, "Physics: set_gravity set_damping set_friction spawn_ball clear_balls");
    console_print(c, "Query: get_ball get_ball_count mouse_x mouse_y frame_time elapsed_time");
    console_print(c, "Custom: run_each_frame {script} - runs script every frame");
    console_print(c, "");

    return c;
}

void console_free(Console* c) {
    if (c) {
        free(c);
    }
}

void console_register_commands(Console* c) {
    FeatherRegister(c->interp, "puts", cmd_puts, c);
}

void console_toggle(Console* c) {
    c->visible = !c->visible;
}

int console_is_visible(Console* c) {
    return c->visible;
}

void console_print(Console* c, const char* text) {
    int len = strlen(text);

    /* Add text to scrollback */
    for (int i = 0; i < len && c->scrollback_len < CONSOLE_SCROLLBACK_SIZE - 1; i++) {
        int idx = (c->scrollback_start + c->scrollback_len) % CONSOLE_SCROLLBACK_SIZE;
        c->scrollback[idx] = text[i];
        c->scrollback_len++;
    }

    /* Add newline */
    if (c->scrollback_len < CONSOLE_SCROLLBACK_SIZE - 1) {
        int idx = (c->scrollback_start + c->scrollback_len) % CONSOLE_SCROLLBACK_SIZE;
        c->scrollback[idx] = '\n';
        c->scrollback_len++;
    }

    /* If buffer is full, advance start */
    if (c->scrollback_len >= CONSOLE_SCROLLBACK_SIZE - 1) {
        /* Find next newline to trim a full line */
        int trim = 0;
        for (int i = 0; i < c->scrollback_len && i < 256; i++) {
            int idx = (c->scrollback_start + i) % CONSOLE_SCROLLBACK_SIZE;
            if (c->scrollback[idx] == '\n') {
                trim = i + 1;
                break;
            }
        }
        if (trim > 0) {
            c->scrollback_start = (c->scrollback_start + trim) % CONSOLE_SCROLLBACK_SIZE;
            c->scrollback_len -= trim;
        }
    }
}

void console_printf(Console* c, const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    console_print(c, buf);
}

int console_update(Console* c) {
    if (!c->visible) return 0;

    /* Handle mouse wheel scrolling */
    float wheel = GetMouseWheelMove();
    if (wheel != 0) {
        c->scroll_offset += (int)(wheel * 3);
        int max_scroll = console_count_lines(c) - (c->height / CONSOLE_LINE_HEIGHT) + 2;
        if (c->scroll_offset < 0) c->scroll_offset = 0;
        if (c->scroll_offset > max_scroll) c->scroll_offset = max_scroll;
        if (c->scroll_offset < 0) c->scroll_offset = 0;
    }

    /* Handle text input */
    int ch;
    while ((ch = GetCharPressed()) != 0) {
        console_handle_char(c, ch);
    }

    /* Handle special keys */
    if (IsKeyPressed(KEY_ENTER)) {
        console_submit(c);
        return 1;
    }

    if (IsKeyPressed(KEY_BACKSPACE)) {
        console_backspace(c);
        return 1;
    }

    /* Emacs-style key bindings with Ctrl */
    if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
        if (IsKeyPressed(KEY_A)) {
            /* Ctrl-A: beginning of line */
            c->cursor = 0;
            return 1;
        }
        if (IsKeyPressed(KEY_E)) {
            /* Ctrl-E: end of line */
            c->cursor = c->input_len;
            return 1;
        }
        if (IsKeyPressed(KEY_B)) {
            /* Ctrl-B: back one char */
            console_move_cursor(c, -1);
            return 1;
        }
        if (IsKeyPressed(KEY_F)) {
            /* Ctrl-F: forward one char */
            console_move_cursor(c, 1);
            return 1;
        }
        if (IsKeyPressed(KEY_D)) {
            /* Ctrl-D: delete char at cursor */
            console_delete_char(c);
            return 1;
        }
        if (IsKeyPressed(KEY_K)) {
            /* Ctrl-K: kill to end of line */
            console_kill_to_end(c);
            return 1;
        }
        if (IsKeyPressed(KEY_U)) {
            /* Ctrl-U: kill entire line */
            console_kill_line(c);
            return 1;
        }
    }

    /* Arrow keys */
    if (IsKeyPressed(KEY_LEFT)) {
        console_move_cursor(c, -1);
        return 1;
    }
    if (IsKeyPressed(KEY_RIGHT)) {
        console_move_cursor(c, 1);
        return 1;
    }
    if (IsKeyPressed(KEY_HOME)) {
        c->cursor = 0;
        return 1;
    }
    if (IsKeyPressed(KEY_END)) {
        c->cursor = c->input_len;
        return 1;
    }

    return 1; /* Console consumed input focus */
}

void console_render(Console* c) {
    if (!c->visible) return;

    int screen_width = GetScreenWidth();

    /* Draw semi-transparent background */
    DrawRectangle(0, 0, screen_width, c->height, (Color){0, 0, 0, 200});

    /* Draw border line */
    DrawRectangle(0, c->height - 2, screen_width, 2, (Color){100, 100, 100, 255});

    /* Calculate visible area */
    int text_area_height = c->height - CONSOLE_LINE_HEIGHT - CONSOLE_PADDING * 2;
    int visible_lines = text_area_height / CONSOLE_LINE_HEIGHT;

    /* Draw scrollback lines from bottom up */
    int total_lines = console_count_lines(c);
    int y = c->height - CONSOLE_LINE_HEIGHT - CONSOLE_PADDING - CONSOLE_LINE_HEIGHT;

    for (int i = c->scroll_offset; i < total_lines && visible_lines > 0; i++) {
        int line_len;
        const char* line = console_get_line(c, i, &line_len);
        if (line && line_len > 0) {
            /* Copy line to null-terminated buffer */
            char buf[256];
            int copy_len = line_len < 255 ? line_len : 255;
            memcpy(buf, line, copy_len);
            buf[copy_len] = '\0';
            DrawText(buf, CONSOLE_PADDING, y, CONSOLE_FONT_SIZE, (Color){200, 200, 200, 255});
        }
        y -= CONSOLE_LINE_HEIGHT;
        visible_lines--;
    }

    /* Draw input line at bottom */
    int input_y = c->height - CONSOLE_LINE_HEIGHT - CONSOLE_PADDING;
    const char* prompt = c->continuation ? "... " : "> ";

    DrawText(prompt, CONSOLE_PADDING, input_y, CONSOLE_FONT_SIZE, (Color){100, 255, 100, 255});

    /* Draw input text */
    int prompt_width = MeasureText(prompt, CONSOLE_FONT_SIZE);
    DrawText(c->input, CONSOLE_PADDING + prompt_width, input_y, CONSOLE_FONT_SIZE, (Color){255, 255, 255, 255});

    /* Draw cursor */
    char cursor_prefix[CONSOLE_INPUT_SIZE];
    strncpy(cursor_prefix, c->input, c->cursor);
    cursor_prefix[c->cursor] = '\0';
    int cursor_x = CONSOLE_PADDING + prompt_width + MeasureText(cursor_prefix, CONSOLE_FONT_SIZE);

    /* Blinking cursor */
    if (((int)(GetTime() * 2)) % 2 == 0) {
        DrawRectangle(cursor_x, input_y, 2, CONSOLE_FONT_SIZE, (Color){255, 255, 255, 255});
    }

    /* Draw scroll indicator if needed */
    if (c->scroll_offset > 0) {
        DrawText("[scroll]", screen_width - 80, CONSOLE_PADDING, 16, (Color){150, 150, 150, 255});
    }
}

/* Internal functions */

static void console_submit(Console* c) {
    if (c->input_len == 0 && c->accumulated_len == 0) {
        return;
    }

    /* Echo input to scrollback */
    const char* prompt = c->continuation ? "... " : "> ";
    console_printf(c, "%s%s", prompt, c->input);

    /* Append input to accumulated buffer */
    if (c->accumulated_len + c->input_len + 1 < CONSOLE_ACCUMULATED_SIZE) {
        memcpy(c->accumulated + c->accumulated_len, c->input, c->input_len);
        c->accumulated_len += c->input_len;
        c->accumulated[c->accumulated_len++] = '\n';
        c->accumulated[c->accumulated_len] = '\0';
    }

    /* Check if script is complete */
    FeatherParseStatus status = FeatherParse(c->interp, c->accumulated, c->accumulated_len);

    if (status == FEATHER_PARSE_INCOMPLETE) {
        /* Need more input */
        c->continuation = 1;
    } else if (status == FEATHER_PARSE_OK) {
        /* Evaluate the script */
        FeatherObj result = 0;
        FeatherResult eval_status = FeatherEval(c->interp, c->accumulated, c->accumulated_len, &result);

        if (eval_status == FEATHER_OK) {
            /* Show result if non-empty */
            if (result != 0) {
                char buf[1024];
                size_t len = FeatherCopy(c->interp, result, buf, sizeof(buf) - 1);
                buf[len] = '\0';
                if (len > 0) {
                    console_print(c, buf);
                }
            }
        } else {
            /* Show error */
            if (result != 0) {
                char buf[1024];
                size_t len = FeatherCopy(c->interp, result, buf, sizeof(buf) - 1);
                buf[len] = '\0';
                console_printf(c, "Error: %s", buf);
            } else {
                console_print(c, "Error: evaluation failed");
            }
        }

        /* Clear accumulated */
        c->accumulated[0] = '\0';
        c->accumulated_len = 0;
        c->continuation = 0;
    } else {
        /* Parse error */
        console_print(c, "Syntax error");
        c->accumulated[0] = '\0';
        c->accumulated_len = 0;
        c->continuation = 0;
    }

    /* Clear input */
    c->input[0] = '\0';
    c->input_len = 0;
    c->cursor = 0;
}

static void console_handle_char(Console* c, int ch) {
    if (ch >= 32 && ch < 127) {
        console_insert_char(c, (char)ch);
    }
}

static void console_insert_char(Console* c, char ch) {
    if (c->input_len >= CONSOLE_INPUT_SIZE - 1) return;

    /* Shift characters after cursor right */
    for (int i = c->input_len; i > c->cursor; i--) {
        c->input[i] = c->input[i - 1];
    }

    c->input[c->cursor] = ch;
    c->input_len++;
    c->cursor++;
    c->input[c->input_len] = '\0';
}

static void console_delete_char(Console* c) {
    if (c->cursor >= c->input_len) return;

    /* Shift characters after cursor left */
    for (int i = c->cursor; i < c->input_len - 1; i++) {
        c->input[i] = c->input[i + 1];
    }

    c->input_len--;
    c->input[c->input_len] = '\0';
}

static void console_backspace(Console* c) {
    if (c->cursor <= 0) return;

    c->cursor--;
    console_delete_char(c);
}

static void console_move_cursor(Console* c, int delta) {
    c->cursor += delta;
    if (c->cursor < 0) c->cursor = 0;
    if (c->cursor > c->input_len) c->cursor = c->input_len;
}

static void console_kill_line(Console* c) {
    c->input[0] = '\0';
    c->input_len = 0;
    c->cursor = 0;
}

static void console_kill_to_end(Console* c) {
    c->input[c->cursor] = '\0';
    c->input_len = c->cursor;
}

static int console_count_lines(Console* c) {
    int count = 0;
    for (int i = 0; i < c->scrollback_len; i++) {
        int idx = (c->scrollback_start + i) % CONSOLE_SCROLLBACK_SIZE;
        if (c->scrollback[idx] == '\n') {
            count++;
        }
    }
    return count;
}

static const char* console_get_line(Console* c, int line_from_bottom, int* out_len) {
    /* Find the nth line from the bottom of the scrollback */
    int total_lines = console_count_lines(c);
    int target_line = total_lines - 1 - line_from_bottom;

    if (target_line < 0 || target_line >= total_lines) {
        *out_len = 0;
        return NULL;
    }

    /* Find the start of the target line */
    int current_line = 0;
    int line_start = 0;

    for (int i = 0; i < c->scrollback_len; i++) {
        int idx = (c->scrollback_start + i) % CONSOLE_SCROLLBACK_SIZE;
        if (current_line == target_line) {
            line_start = i;
            break;
        }
        if (c->scrollback[idx] == '\n') {
            current_line++;
            if (current_line == target_line) {
                line_start = i + 1;
                break;
            }
        }
    }

    /* Find the end of the line */
    int line_end = line_start;
    for (int i = line_start; i < c->scrollback_len; i++) {
        int idx = (c->scrollback_start + i) % CONSOLE_SCROLLBACK_SIZE;
        if (c->scrollback[idx] == '\n') {
            line_end = i;
            break;
        }
        line_end = i + 1;
    }

    *out_len = line_end - line_start;

    /* Return pointer to the line (may wrap around ring buffer, so copy needed) */
    static char line_buf[256];
    int len = *out_len < 255 ? *out_len : 255;
    for (int i = 0; i < len; i++) {
        int idx = (c->scrollback_start + line_start + i) % CONSOLE_SCROLLBACK_SIZE;
        line_buf[i] = c->scrollback[idx];
    }
    line_buf[len] = '\0';
    *out_len = len;

    return line_buf;
}
