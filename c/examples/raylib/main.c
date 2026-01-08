// Raylib + Feather integration example
//
// Game state is managed in C, TCL is used for drawing commands.
// This avoids state persistence issues across evals.
//
// Press ` (backtick) to toggle the in-game console.

#include "feather.h"
#include "console.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_BALLS 50

typedef struct {
    float x, y;
    float vx, vy;
    float radius;
    unsigned char r, g, b;
} Ball;

static Ball balls[MAX_BALLS];
static int ball_count = 0;
static float spawn_cooldown = 0;
static Console *console = NULL;
static FeatherInterp g_interp = 0;

// Custom draw script (set via console, runs each frame)
static char custom_draw_script[4096] = "";

// -----------------------------------------------------------------------------
// Variable helpers - use TCL's set command via FeatherCall
// -----------------------------------------------------------------------------

static void set_var_double(FeatherInterp interp, const char *name, double val) {
    // Use global namespace (::name) so variable persists across frames
    char global_name[128];
    snprintf(global_name, sizeof(global_name), "::%s", name);
    FeatherObj argv[3];
    argv[0] = FeatherString(interp, "set", 3);
    argv[1] = FeatherString(interp, global_name, strlen(global_name));
    argv[2] = FeatherDouble(interp, val);
    FeatherObj result;
    FeatherCall(interp, 3, argv, &result);
}

static double get_var_double(FeatherInterp interp, const char *name, double def) {
    // Use global namespace (::name)
    char global_name[128];
    snprintf(global_name, sizeof(global_name), "::%s", name);
    FeatherObj argv[2];
    argv[0] = FeatherString(interp, "set", 3);
    argv[1] = FeatherString(interp, global_name, strlen(global_name));
    FeatherObj result;
    if (FeatherCall(interp, 2, argv, &result) == FEATHER_OK && result != 0) {
        return FeatherAsDouble(interp, result, def);
    }
    return def;
}

// -----------------------------------------------------------------------------
// Draw Command with Subcommands
// -----------------------------------------------------------------------------

static int str_eq(FeatherInterp interp, FeatherObj obj, const char *s) {
    char buf[64];
    size_t len = FeatherCopy(interp, obj, buf, sizeof(buf) - 1);
    buf[len] = '\0';
    return strcmp(buf, s) == 0;
}

static int cmd_draw(void *data, FeatherInterp interp,
                    size_t argc, FeatherObj *argv,
                    FeatherObj *result, FeatherObj *err) {
    (void)data;
    if (argc < 1) {
        *err = FeatherString(interp, "draw: missing subcommand", 24);
        return 1;
    }

    FeatherObj subcmd = argv[0];
    FeatherObj *args = argv + 1;
    size_t nargs = argc - 1;

    if (str_eq(interp, subcmd, "clear")) {
        ClearBackground(DARKBLUE);
        *result = 0;
        return 0;
    }

    if (str_eq(interp, subcmd, "circle")) {
        if (nargs < 7) {
            *err = FeatherString(interp, "draw circle: x y radius r g b a", 32);
            return 1;
        }
        int x = (int)FeatherAsInt(interp, args[0], 0);
        int y = (int)FeatherAsInt(interp, args[1], 0);
        float radius = (float)FeatherAsDouble(interp, args[2], 10.0);
        int r = (int)FeatherAsInt(interp, args[3], 255);
        int g = (int)FeatherAsInt(interp, args[4], 255);
        int b = (int)FeatherAsInt(interp, args[5], 255);
        int a = (int)FeatherAsInt(interp, args[6], 255);
        DrawCircle(x, y, radius, (Color){r, g, b, a});
        *result = 0;
        return 0;
    }

    if (str_eq(interp, subcmd, "ring")) {
        if (nargs < 8) {
            *err = FeatherString(interp, "draw ring: x y inner outer r g b a", 35);
            return 1;
        }
        int x = (int)FeatherAsInt(interp, args[0], 0);
        int y = (int)FeatherAsInt(interp, args[1], 0);
        float inner = (float)FeatherAsDouble(interp, args[2], 10.0);
        float outer = (float)FeatherAsDouble(interp, args[3], 20.0);
        int r = (int)FeatherAsInt(interp, args[4], 255);
        int g = (int)FeatherAsInt(interp, args[5], 255);
        int b = (int)FeatherAsInt(interp, args[6], 255);
        int a = (int)FeatherAsInt(interp, args[7], 255);
        DrawRing((Vector2){x, y}, inner, outer, 0, 360, 36, (Color){r, g, b, a});
        *result = 0;
        return 0;
    }

    if (str_eq(interp, subcmd, "text")) {
        if (nargs < 8) {
            *err = FeatherString(interp, "draw text: str x y size r g b a", 31);
            return 1;
        }
        char text[1024];
        size_t n = FeatherCopy(interp, args[0], text, sizeof(text) - 1);
        text[n] = '\0';
        int x = (int)FeatherAsInt(interp, args[1], 0);
        int y = (int)FeatherAsInt(interp, args[2], 0);
        int size = (int)FeatherAsInt(interp, args[3], 20);
        int r = (int)FeatherAsInt(interp, args[4], 255);
        int g = (int)FeatherAsInt(interp, args[5], 255);
        int b = (int)FeatherAsInt(interp, args[6], 255);
        int a = (int)FeatherAsInt(interp, args[7], 255);
        DrawText(text, x, y, size, (Color){r, g, b, a});
        *result = 0;
        return 0;
    }

    if (str_eq(interp, subcmd, "rect")) {
        if (nargs < 8) {
            *err = FeatherString(interp, "draw rect: x y w h r g b a", 26);
            return 1;
        }
        int x = (int)FeatherAsInt(interp, args[0], 0);
        int y = (int)FeatherAsInt(interp, args[1], 0);
        int w = (int)FeatherAsInt(interp, args[2], 10);
        int h = (int)FeatherAsInt(interp, args[3], 10);
        int r = (int)FeatherAsInt(interp, args[4], 255);
        int g = (int)FeatherAsInt(interp, args[5], 255);
        int b = (int)FeatherAsInt(interp, args[6], 255);
        int a = (int)FeatherAsInt(interp, args[7], 255);
        DrawRectangle(x, y, w, h, (Color){r, g, b, a});
        *result = 0;
        return 0;
    }

    if (str_eq(interp, subcmd, "line")) {
        if (nargs < 8) {
            *err = FeatherString(interp, "draw line: x1 y1 x2 y2 r g b a", 31);
            return 1;
        }
        int x1 = (int)FeatherAsInt(interp, args[0], 0);
        int y1 = (int)FeatherAsInt(interp, args[1], 0);
        int x2 = (int)FeatherAsInt(interp, args[2], 0);
        int y2 = (int)FeatherAsInt(interp, args[3], 0);
        int r = (int)FeatherAsInt(interp, args[4], 255);
        int g = (int)FeatherAsInt(interp, args[5], 255);
        int b = (int)FeatherAsInt(interp, args[6], 255);
        int a = (int)FeatherAsInt(interp, args[7], 255);
        DrawLine(x1, y1, x2, y2, (Color){r, g, b, a});
        *result = 0;
        return 0;
    }

    char errbuf[128];
    snprintf(errbuf, sizeof(errbuf), "draw: unknown subcommand");
    *err = FeatherString(interp, errbuf, strlen(errbuf));
    return 1;
}

// -----------------------------------------------------------------------------
// Game State Commands
// -----------------------------------------------------------------------------

static int cmd_get_ball_count(void *data, FeatherInterp interp,
                              size_t argc, FeatherObj *argv,
                              FeatherObj *result, FeatherObj *err) {
    (void)data; (void)argc; (void)argv; (void)err;
    *result = FeatherInt(interp, ball_count);
    return 0;
}

static int cmd_get_ball(void *data, FeatherInterp interp,
                        size_t argc, FeatherObj *argv,
                        FeatherObj *result, FeatherObj *err) {
    (void)data; (void)err;
    if (argc < 1) {
        *result = 0;
        return 0;
    }
    int idx = (int)FeatherAsInt(interp, argv[0], 0);
    if (idx < 0 || idx >= ball_count) {
        *result = 0;
        return 0;
    }
    Ball *b = &balls[idx];
    // Return as list: x y radius r g b
    FeatherObj items[6];
    items[0] = FeatherDouble(interp, b->x);
    items[1] = FeatherDouble(interp, b->y);
    items[2] = FeatherDouble(interp, b->radius);
    items[3] = FeatherInt(interp, b->r);
    items[4] = FeatherInt(interp, b->g);
    items[5] = FeatherInt(interp, b->b);
    *result = FeatherList(interp, 6, items);
    return 0;
}

static int cmd_get_fps(void *data, FeatherInterp interp,
                       size_t argc, FeatherObj *argv,
                       FeatherObj *result, FeatherObj *err) {
    (void)data; (void)argc; (void)argv; (void)err;
    *result = FeatherInt(interp, GetFPS());
    return 0;
}

static int cmd_mouse_x(void *data, FeatherInterp interp,
                       size_t argc, FeatherObj *argv,
                       FeatherObj *result, FeatherObj *err) {
    (void)data; (void)argc; (void)argv; (void)err;
    *result = FeatherInt(interp, GetMouseX());
    return 0;
}

static int cmd_mouse_y(void *data, FeatherInterp interp,
                       size_t argc, FeatherObj *argv,
                       FeatherObj *result, FeatherObj *err) {
    (void)data; (void)argc; (void)argv; (void)err;
    *result = FeatherInt(interp, GetMouseY());
    return 0;
}

static int cmd_mouse_down(void *data, FeatherInterp interp,
                          size_t argc, FeatherObj *argv,
                          FeatherObj *result, FeatherObj *err) {
    (void)data; (void)argc; (void)argv; (void)err;
    *result = FeatherInt(interp, IsMouseButtonDown(0) ? 1 : 0);
    return 0;
}

static int cmd_frame_time(void *data, FeatherInterp interp,
                          size_t argc, FeatherObj *argv,
                          FeatherObj *result, FeatherObj *err) {
    (void)data; (void)argc; (void)argv; (void)err;
    *result = FeatherDouble(interp, GetFrameTime());
    return 0;
}

static int cmd_elapsed_time(void *data, FeatherInterp interp,
                            size_t argc, FeatherObj *argv,
                            FeatherObj *result, FeatherObj *err) {
    (void)data; (void)argc; (void)argv; (void)err;
    *result = FeatherDouble(interp, GetTime());
    return 0;
}

// -----------------------------------------------------------------------------
// Console-accessible Game Commands
// -----------------------------------------------------------------------------

static int cmd_spawn_ball(void *data, FeatherInterp interp,
                          size_t argc, FeatherObj *argv,
                          FeatherObj *result, FeatherObj *err) {
    (void)data; (void)err;
    int x = (argc > 0) ? (int)FeatherAsInt(interp, argv[0], GetScreenWidth()/2) : GetScreenWidth()/2;
    int y = (argc > 1) ? (int)FeatherAsInt(interp, argv[1], GetScreenHeight()/2) : GetScreenHeight()/2;

    if (ball_count >= MAX_BALLS) {
        for (int i = 0; i < MAX_BALLS - 1; i++) {
            balls[i] = balls[i + 1];
        }
        ball_count = MAX_BALLS - 1;
    }
    Ball *b = &balls[ball_count++];
    b->x = (float)x;
    b->y = (float)y;
    b->vx = (float)(GetRandomValue(-200, 200));
    b->vy = (float)(GetRandomValue(-300, -100));
    b->radius = (float)(GetRandomValue(10, 30));
    b->r = (unsigned char)GetRandomValue(100, 255);
    b->g = (unsigned char)GetRandomValue(100, 255);
    b->b = (unsigned char)GetRandomValue(100, 255);

    *result = FeatherInt(interp, ball_count);
    return 0;
}

static int cmd_clear_balls(void *data, FeatherInterp interp,
                           size_t argc, FeatherObj *argv,
                           FeatherObj *result, FeatherObj *err) {
    (void)data; (void)argc; (void)argv; (void)err;
    ball_count = 0;
    *result = FeatherInt(interp, 0);
    return 0;
}

static int cmd_screen_width(void *data, FeatherInterp interp,
                            size_t argc, FeatherObj *argv,
                            FeatherObj *result, FeatherObj *err) {
    (void)data; (void)argc; (void)argv; (void)err;
    *result = FeatherInt(interp, GetScreenWidth());
    return 0;
}

static int cmd_screen_height(void *data, FeatherInterp interp,
                             size_t argc, FeatherObj *argv,
                             FeatherObj *result, FeatherObj *err) {
    (void)data; (void)argc; (void)argv; (void)err;
    *result = FeatherInt(interp, GetScreenHeight());
    return 0;
}

static int cmd_run_each_frame(void *data, FeatherInterp interp,
                              size_t argc, FeatherObj *argv,
                              FeatherObj *result, FeatherObj *err) {
    (void)data; (void)err;
    if (argc > 0) {
        size_t len = FeatherCopy(interp, argv[0], custom_draw_script, sizeof(custom_draw_script) - 1);
        custom_draw_script[len] = '\0';
    } else {
        custom_draw_script[0] = '\0';
    }
    *result = FeatherString(interp, custom_draw_script, strlen(custom_draw_script));
    return 0;
}

static int cmd_get_draw_script(void *data, FeatherInterp interp,
                               size_t argc, FeatherObj *argv,
                               FeatherObj *result, FeatherObj *err) {
    (void)data; (void)argc; (void)argv; (void)err;
    *result = FeatherString(interp, custom_draw_script, strlen(custom_draw_script));
    return 0;
}

// -----------------------------------------------------------------------------
// Registration
// -----------------------------------------------------------------------------

static void register_commands(FeatherInterp interp) {
    // Drawing command with subcommands
    FeatherRegister(interp, "draw", cmd_draw, NULL);

    // Game state queries
    FeatherRegister(interp, "get_ball_count", cmd_get_ball_count, NULL);
    FeatherRegister(interp, "get_ball", cmd_get_ball, NULL);
    FeatherRegister(interp, "get_fps", cmd_get_fps, NULL);
    FeatherRegister(interp, "mouse_x", cmd_mouse_x, NULL);
    FeatherRegister(interp, "mouse_y", cmd_mouse_y, NULL);
    FeatherRegister(interp, "mouse_down", cmd_mouse_down, NULL);
    FeatherRegister(interp, "frame_time", cmd_frame_time, NULL);
    FeatherRegister(interp, "elapsed_time", cmd_elapsed_time, NULL);
    FeatherRegister(interp, "screen_width", cmd_screen_width, NULL);
    FeatherRegister(interp, "screen_height", cmd_screen_height, NULL);

    // Game manipulation (usable from console)
    FeatherRegister(interp, "spawn_ball", cmd_spawn_ball, NULL);
    FeatherRegister(interp, "clear_balls", cmd_clear_balls, NULL);

    // Custom draw script (runs each frame during drawing)
    FeatherRegister(interp, "run_each_frame", cmd_run_each_frame, NULL);
    FeatherRegister(interp, "get_draw_script", cmd_get_draw_script, NULL);
}

// -----------------------------------------------------------------------------
// Game Logic (in C)
// -----------------------------------------------------------------------------

static void spawn_ball(int x, int y) {
    if (ball_count >= MAX_BALLS) {
        // Remove oldest ball
        for (int i = 0; i < MAX_BALLS - 1; i++) {
            balls[i] = balls[i + 1];
        }
        ball_count = MAX_BALLS - 1;
    }
    Ball *b = &balls[ball_count++];
    b->x = (float)x;
    b->y = (float)y;
    b->vx = (float)(GetRandomValue(-200, 200));
    b->vy = (float)(GetRandomValue(-300, -100));
    b->radius = (float)(GetRandomValue(10, 30));
    b->r = (unsigned char)GetRandomValue(100, 255);
    b->g = (unsigned char)GetRandomValue(100, 255);
    b->b = (unsigned char)GetRandomValue(100, 255);
}

static void update_game(void) {
    float dt = GetFrameTime();
    int w = GetScreenWidth();
    int h = GetScreenHeight();

    // Read physics parameters from interpreter variables
    float gravity = (float)get_var_double(g_interp, "gravity", 500.0);
    float damping = (float)get_var_double(g_interp, "damping", 0.8);
    float friction = (float)get_var_double(g_interp, "friction", 0.95);

    // Spawn on click (only when console is not visible)
    if (spawn_cooldown > 0) {
        spawn_cooldown -= dt;
    }
    if (!console_is_visible(console) && IsMouseButtonPressed(0) && spawn_cooldown <= 0) {
        spawn_ball(GetMouseX(), GetMouseY());
        spawn_cooldown = 0.1f;
    }

    // Update balls
    for (int i = 0; i < ball_count; i++) {
        Ball *b = &balls[i];

        // Gravity
        b->vy += gravity * dt;

        // Move
        b->x += b->vx * dt;
        b->y += b->vy * dt;

        // Bounce off walls
        if (b->x - b->radius < 0) {
            b->x = b->radius;
            b->vx = -b->vx * damping;
        }
        if (b->x + b->radius > w) {
            b->x = (float)w - b->radius;
            b->vx = -b->vx * damping;
        }

        // Bounce off floor
        if (b->y + b->radius > h) {
            b->y = (float)h - b->radius;
            b->vy = -b->vy * damping;
            b->vx *= friction;
        }

        // Bounce off ceiling
        if (b->y - b->radius < 0) {
            b->y = b->radius;
            b->vy = -b->vy * damping;
        }
    }
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    // Initialize raylib
    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(800, 600, "Feather + Raylib");
    SetTargetFPS(60);

    // Initialize Feather
    FeatherInterp interp = FeatherNew();
    g_interp = interp;
    register_commands(interp);

    // Initialize physics variables (can be changed with `set` from console)
    set_var_double(interp, "gravity", 500.0);
    set_var_double(interp, "damping", 0.8);
    set_var_double(interp, "friction", 0.95);

    // Initialize console
    console = console_new(interp);
    console_register_commands(console);

    // Draw script - called each frame
    const char *draw_script =
        "draw clear\n"
        "draw text \"Click to spawn balls!\" 10 10 24 255 255 255 255\n"
        "draw text \"Balls: [get_ball_count]\" 10 40 20 200 200 200 255\n"
        "draw text \"FPS: [get_fps]\" 10 65 20 200 200 200 255\n"
        "for {set i 0} {$i < [get_ball_count]} {incr i} {\n"
        "    set b [get_ball $i]\n"
        "    set x [lindex $b 0]\n"
        "    set y [lindex $b 1]\n"
        "    set radius [lindex $b 2]\n"
        "    set r [lindex $b 3]\n"
        "    set g [lindex $b 4]\n"
        "    set bcolor [lindex $b 5]\n"
        "    draw circle [expr {int($x + 3)}] [expr {int($y + 3)}] $radius 0 0 0 100\n"
        "    draw circle [expr {int($x)}] [expr {int($y)}] $radius $r $g $bcolor 255\n"
        "    set hr [expr {$radius * 0.3}]\n"
        "    set hx [expr {int($x - $radius * 0.3)}]\n"
        "    set hy [expr {int($y - $radius * 0.3)}]\n"
        "    draw circle $hx $hy $hr 255 255 255 80\n"
        "}\n"
        "draw ring [mouse_x] [mouse_y] 15 18 255 255 255 255\n"
        "if {[mouse_down]} {\n"
        "    draw circle [mouse_x] [mouse_y] 12 255 255 255 150\n"
        "}\n";

    FeatherObj result;
    int status;

    while (!WindowShouldClose()) {
        // Toggle console with backtick key
        if (IsKeyPressed(KEY_GRAVE)) {
            console_toggle(console);
        }

        // Update console (handles input when visible)
        console_update(console);

        // Update game state in C
        update_game();

        // Draw using TCL
        BeginDrawing();

        status = FeatherEval(interp, (char *)draw_script, strlen(draw_script), &result);
        if (status != 0) {
            // On error, draw fallback
            ClearBackground(RED);
            char errbuf[512];
            FeatherCopy(interp, result, errbuf, sizeof(errbuf) - 1);
            errbuf[511] = '\0';
            DrawText(errbuf, 10, 10, 20, WHITE);
        }

        // Execute custom draw script (set via run_each_frame command)
        if (custom_draw_script[0] != '\0') {
            FeatherObj custom_result;
            int custom_status = FeatherEval(interp, custom_draw_script, strlen(custom_draw_script), &custom_result);
            if (custom_status != 0) {
                // Show error in console
                char errbuf[512];
                FeatherCopy(interp, custom_result, errbuf, sizeof(errbuf) - 1);
                errbuf[511] = '\0';
                console_printf(console, "draw script error: %s", errbuf);
                // Clear the broken script
                custom_draw_script[0] = '\0';
            }
        }

        // Draw console overlay
        console_render(console);

        EndDrawing();
    }

    console_free(console);
    CloseWindow();
    FeatherClose(interp);

    return 0;
}
