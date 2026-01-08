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

// -----------------------------------------------------------------------------
// Drawing Commands
// -----------------------------------------------------------------------------

static int cmd_clear(void *data, FeatherInterp interp,
                     size_t argc, FeatherObj *argv,
                     FeatherObj *result, FeatherObj *err) {
    (void)data; (void)err; (void)argc; (void)argv; (void)interp;
    ClearBackground(DARKBLUE);
    *result = 0;
    return 0;
}

static int cmd_draw_circle(void *data, FeatherInterp interp,
                           size_t argc, FeatherObj *argv,
                           FeatherObj *result, FeatherObj *err) {
    (void)data; (void)err;
    if (argc < 7) {
        *result = 0;
        return 0;
    }
    int x = (int)FeatherAsInt(interp, argv[0], 0);
    int y = (int)FeatherAsInt(interp, argv[1], 0);
    float radius = (float)FeatherAsDouble(interp, argv[2], 10.0);
    int r = (int)FeatherAsInt(interp, argv[3], 255);
    int g = (int)FeatherAsInt(interp, argv[4], 255);
    int b = (int)FeatherAsInt(interp, argv[5], 255);
    int a = (int)FeatherAsInt(interp, argv[6], 255);
    DrawCircle(x, y, radius, (Color){r, g, b, a});
    *result = 0;
    return 0;
}

static int cmd_draw_ring(void *data, FeatherInterp interp,
                         size_t argc, FeatherObj *argv,
                         FeatherObj *result, FeatherObj *err) {
    (void)data; (void)err;
    if (argc < 8) {
        *result = 0;
        return 0;
    }
    int x = (int)FeatherAsInt(interp, argv[0], 0);
    int y = (int)FeatherAsInt(interp, argv[1], 0);
    float inner = (float)FeatherAsDouble(interp, argv[2], 10.0);
    float outer = (float)FeatherAsDouble(interp, argv[3], 20.0);
    int r = (int)FeatherAsInt(interp, argv[4], 255);
    int g = (int)FeatherAsInt(interp, argv[5], 255);
    int b = (int)FeatherAsInt(interp, argv[6], 255);
    int a = (int)FeatherAsInt(interp, argv[7], 255);
    DrawRing((Vector2){x, y}, inner, outer, 0, 360, 36, (Color){r, g, b, a});
    *result = 0;
    return 0;
}

static int cmd_draw_text(void *data, FeatherInterp interp,
                         size_t argc, FeatherObj *argv,
                         FeatherObj *result, FeatherObj *err) {
    (void)data; (void)err;
    if (argc < 8) {
        *result = 0;
        return 0;
    }
    char text[1024];
    size_t n = FeatherCopy(interp, argv[0], text, sizeof(text) - 1);
    text[n] = '\0';
    int x = (int)FeatherAsInt(interp, argv[1], 0);
    int y = (int)FeatherAsInt(interp, argv[2], 0);
    int size = (int)FeatherAsInt(interp, argv[3], 20);
    int r = (int)FeatherAsInt(interp, argv[4], 255);
    int g = (int)FeatherAsInt(interp, argv[5], 255);
    int b = (int)FeatherAsInt(interp, argv[6], 255);
    int a = (int)FeatherAsInt(interp, argv[7], 255);
    DrawText(text, x, y, size, (Color){r, g, b, a});
    *result = 0;
    return 0;
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

// -----------------------------------------------------------------------------
// Registration
// -----------------------------------------------------------------------------

static void register_commands(FeatherInterp interp) {
    FeatherRegister(interp, "clear", cmd_clear, NULL);
    FeatherRegister(interp, "draw_circle", cmd_draw_circle, NULL);
    FeatherRegister(interp, "draw_ring", cmd_draw_ring, NULL);
    FeatherRegister(interp, "draw_text", cmd_draw_text, NULL);
    FeatherRegister(interp, "get_ball_count", cmd_get_ball_count, NULL);
    FeatherRegister(interp, "get_ball", cmd_get_ball, NULL);
    FeatherRegister(interp, "get_fps", cmd_get_fps, NULL);
    FeatherRegister(interp, "mouse_x", cmd_mouse_x, NULL);
    FeatherRegister(interp, "mouse_y", cmd_mouse_y, NULL);
    FeatherRegister(interp, "mouse_down", cmd_mouse_down, NULL);
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
    float gravity = 500.0f;
    float damping = 0.8f;
    int w = GetScreenWidth();
    int h = GetScreenHeight();

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
            b->vx *= 0.95f; // Friction
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
    register_commands(interp);

    // Initialize console
    console = console_new(interp);
    console_register_commands(console);

    // Draw script - called each frame
    const char *draw_script =
        "clear\n"
        "draw_text \"Click to spawn balls!\" 10 10 24 255 255 255 255\n"
        "draw_text \"Balls: [get_ball_count]\" 10 40 20 200 200 200 255\n"
        "draw_text \"FPS: [get_fps]\" 10 65 20 200 200 200 255\n"
        "for {set i 0} {$i < [get_ball_count]} {incr i} {\n"
        "    set b [get_ball $i]\n"
        "    set x [lindex $b 0]\n"
        "    set y [lindex $b 1]\n"
        "    set radius [lindex $b 2]\n"
        "    set r [lindex $b 3]\n"
        "    set g [lindex $b 4]\n"
        "    set bcolor [lindex $b 5]\n"
        "    draw_circle [expr {int($x + 3)}] [expr {int($y + 3)}] $radius 0 0 0 100\n"
        "    draw_circle [expr {int($x)}] [expr {int($y)}] $radius $r $g $bcolor 255\n"
        "    set hr [expr {$radius * 0.3}]\n"
        "    set hx [expr {int($x - $radius * 0.3)}]\n"
        "    set hy [expr {int($y - $radius * 0.3)}]\n"
        "    draw_circle $hx $hy $hr 255 255 255 80\n"
        "}\n"
        "draw_ring [mouse_x] [mouse_y] 15 18 255 255 255 255\n"
        "if {[mouse_down]} {\n"
        "    draw_circle [mouse_x] [mouse_y] 12 255 255 255 150\n"
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

        // Draw console overlay
        console_render(console);

        EndDrawing();
    }

    console_free(console);
    CloseWindow();
    FeatherClose(interp);

    return 0;
}
