/*
 * console.h - In-game TCL console for Feather
 *
 * A simple interactive console overlay that allows executing TCL commands
 * while the game is running. Features:
 * - 32KB scrollback buffer
 * - Multiline input (detects incomplete scripts)
 * - Emacs-style key bindings
 * - Mouse wheel scrolling
 */

#ifndef CONSOLE_H
#define CONSOLE_H

#include <stddef.h>
#include "feather.h"

/* Console configuration */
#define CONSOLE_SCROLLBACK_SIZE 32768
#define CONSOLE_INPUT_SIZE      1024
#define CONSOLE_ACCUMULATED_SIZE 4096
#define CONSOLE_FONT_SIZE       20
#define CONSOLE_LINE_HEIGHT     24
#define CONSOLE_PADDING         10

/* Console state */
typedef struct Console {
    /* Scrollback buffer (ring buffer) */
    char scrollback[CONSOLE_SCROLLBACK_SIZE];
    int scrollback_start;       /* Start index in ring buffer */
    int scrollback_len;         /* Current length of content */

    /* Current input line */
    char input[CONSOLE_INPUT_SIZE];
    int input_len;
    int cursor;                 /* Cursor position in input */

    /* Accumulated input for multiline scripts */
    char accumulated[CONSOLE_ACCUMULATED_SIZE];
    int accumulated_len;
    int continuation;           /* 1 if waiting for more input */

    /* Display state */
    int scroll_offset;          /* Lines scrolled up from bottom */
    int visible;                /* 1 if console is visible */
    int height;                 /* Console height in pixels */

    /* Interpreter reference */
    FeatherInterp interp;
} Console;

/*
 * Create a new console attached to an interpreter.
 * The console will take up the top half of the screen by default.
 */
Console* console_new(FeatherInterp interp);

/*
 * Free console resources.
 */
void console_free(Console* c);

/*
 * Process input for the current frame.
 * Call this every frame when the console is visible.
 * Returns 1 if the console consumed the input, 0 otherwise.
 */
int console_update(Console* c);

/*
 * Render the console.
 * Call this after BeginDrawing() when the console is visible.
 */
void console_render(Console* c);

/*
 * Print text to the console scrollback.
 * Appends a newline automatically.
 */
void console_print(Console* c, const char* text);

/*
 * Print formatted text to the console scrollback.
 */
void console_printf(Console* c, const char* fmt, ...);

/*
 * Toggle console visibility.
 */
void console_toggle(Console* c);

/*
 * Check if console is visible.
 */
int console_is_visible(Console* c);

/*
 * Register the console's custom commands (like puts) with the interpreter.
 * Call this after creating the console.
 */
void console_register_commands(Console* c);

#endif /* CONSOLE_H */
