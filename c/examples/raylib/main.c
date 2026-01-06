// Raylib + Feather integration example
//
// This demonstrates embedding Feather as a scripting language for a raylib game.
// All drawing commands are exposed to Feather scripts.
//
// Build:
//   cc -o game main.c -I../../../bin -L../../../bin -lfeather -lraylib -lm -Wl,-rpath,'$ORIGIN/../../../bin'
//
// Run:
//   ./game demo.tcl

#include "libfeather.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

static FeatherObj make_error(FeatherInterp interp, const char *msg) {
    return FeatherString(interp, msg, strlen(msg));
}

static Color obj_to_color(FeatherInterp interp, FeatherObj obj) {
    // Color can be: "red", "blue", etc. or {r g b} or {r g b a}
    size_t len = FeatherListLen(interp, obj);

    if (len >= 3) {
        int r = (int)FeatherAsInt(interp, FeatherListAt(interp, obj, 0), 0);
        int g = (int)FeatherAsInt(interp, FeatherListAt(interp, obj, 1), 0);
        int b = (int)FeatherAsInt(interp, FeatherListAt(interp, obj, 2), 0);
        int a = (len >= 4) ? (int)FeatherAsInt(interp, FeatherListAt(interp, obj, 3), 255) : 255;
        return (Color){r, g, b, a};
    }

    // Named colors
    char name[32];
    FeatherCopy(interp, obj, name, sizeof(name) - 1);
    name[31] = '\0';

    if (strcmp(name, "white") == 0) return WHITE;
    if (strcmp(name, "black") == 0) return BLACK;
    if (strcmp(name, "red") == 0) return RED;
    if (strcmp(name, "green") == 0) return GREEN;
    if (strcmp(name, "blue") == 0) return BLUE;
    if (strcmp(name, "yellow") == 0) return YELLOW;
    if (strcmp(name, "orange") == 0) return ORANGE;
    if (strcmp(name, "pink") == 0) return PINK;
    if (strcmp(name, "purple") == 0) return PURPLE;
    if (strcmp(name, "skyblue") == 0) return SKYBLUE;
    if (strcmp(name, "darkblue") == 0) return DARKBLUE;
    if (strcmp(name, "darkgreen") == 0) return DARKGREEN;
    if (strcmp(name, "gray") == 0) return GRAY;
    if (strcmp(name, "darkgray") == 0) return DARKGRAY;
    if (strcmp(name, "lightgray") == 0) return LIGHTGRAY;

    return MAGENTA; // fallback for unknown colors
}

// -----------------------------------------------------------------------------
// Drawing Commands
// -----------------------------------------------------------------------------

// clear <color>
static int cmd_clear(void *data, FeatherInterp interp,
                     size_t argc, FeatherObj *argv,
                     FeatherObj *result, FeatherObj *err) {
    (void)data;
    Color c = (argc >= 1) ? obj_to_color(interp, argv[0]) : BLACK;
    ClearBackground(c);
    *result = FeatherString(interp, "", 0);
    return 0;
}

// draw_circle <x> <y> <radius> <color>
static int cmd_draw_circle(void *data, FeatherInterp interp,
                           size_t argc, FeatherObj *argv,
                           FeatherObj *result, FeatherObj *err) {
    (void)data;
    if (argc < 4) {
        *err = make_error(interp, "usage: draw_circle x y radius color");
        return 1;
    }

    int x = (int)FeatherAsInt(interp, argv[0], 0);
    int y = (int)FeatherAsInt(interp, argv[1], 0);
    float r = (float)FeatherAsDouble(interp, argv[2], 10.0);
    Color c = obj_to_color(interp, argv[3]);

    DrawCircle(x, y, r, c);
    *result = FeatherString(interp, "", 0);
    return 0;
}

// draw_ring <x> <y> <inner> <outer> <color>
static int cmd_draw_ring(void *data, FeatherInterp interp,
                         size_t argc, FeatherObj *argv,
                         FeatherObj *result, FeatherObj *err) {
    (void)data;
    if (argc < 5) {
        *err = make_error(interp, "usage: draw_ring x y inner_radius outer_radius color");
        return 1;
    }

    int x = (int)FeatherAsInt(interp, argv[0], 0);
    int y = (int)FeatherAsInt(interp, argv[1], 0);
    float inner = (float)FeatherAsDouble(interp, argv[2], 10.0);
    float outer = (float)FeatherAsDouble(interp, argv[3], 20.0);
    Color c = obj_to_color(interp, argv[4]);

    DrawRing((Vector2){x, y}, inner, outer, 0, 360, 36, c);
    *result = FeatherString(interp, "", 0);
    return 0;
}

// draw_rect <x> <y> <width> <height> <color>
static int cmd_draw_rect(void *data, FeatherInterp interp,
                         size_t argc, FeatherObj *argv,
                         FeatherObj *result, FeatherObj *err) {
    (void)data;
    if (argc < 5) {
        *err = make_error(interp, "usage: draw_rect x y width height color");
        return 1;
    }

    int x = (int)FeatherAsInt(interp, argv[0], 0);
    int y = (int)FeatherAsInt(interp, argv[1], 0);
    int w = (int)FeatherAsInt(interp, argv[2], 10);
    int h = (int)FeatherAsInt(interp, argv[3], 10);
    Color c = obj_to_color(interp, argv[4]);

    DrawRectangle(x, y, w, h, c);
    *result = FeatherString(interp, "", 0);
    return 0;
}

// draw_rect_lines <x> <y> <width> <height> <color>
static int cmd_draw_rect_lines(void *data, FeatherInterp interp,
                               size_t argc, FeatherObj *argv,
                               FeatherObj *result, FeatherObj *err) {
    (void)data;
    if (argc < 5) {
        *err = make_error(interp, "usage: draw_rect_lines x y width height color");
        return 1;
    }

    int x = (int)FeatherAsInt(interp, argv[0], 0);
    int y = (int)FeatherAsInt(interp, argv[1], 0);
    int w = (int)FeatherAsInt(interp, argv[2], 10);
    int h = (int)FeatherAsInt(interp, argv[3], 10);
    Color c = obj_to_color(interp, argv[4]);

    DrawRectangleLines(x, y, w, h, c);
    *result = FeatherString(interp, "", 0);
    return 0;
}

// draw_line <x1> <y1> <x2> <y2> <color>
static int cmd_draw_line(void *data, FeatherInterp interp,
                         size_t argc, FeatherObj *argv,
                         FeatherObj *result, FeatherObj *err) {
    (void)data;
    if (argc < 5) {
        *err = make_error(interp, "usage: draw_line x1 y1 x2 y2 color");
        return 1;
    }

    int x1 = (int)FeatherAsInt(interp, argv[0], 0);
    int y1 = (int)FeatherAsInt(interp, argv[1], 0);
    int x2 = (int)FeatherAsInt(interp, argv[2], 0);
    int y2 = (int)FeatherAsInt(interp, argv[3], 0);
    Color c = obj_to_color(interp, argv[4]);

    DrawLine(x1, y1, x2, y2, c);
    *result = FeatherString(interp, "", 0);
    return 0;
}

// draw_line_thick <x1> <y1> <x2> <y2> <thickness> <color>
static int cmd_draw_line_thick(void *data, FeatherInterp interp,
                               size_t argc, FeatherObj *argv,
                               FeatherObj *result, FeatherObj *err) {
    (void)data;
    if (argc < 6) {
        *err = make_error(interp, "usage: draw_line_thick x1 y1 x2 y2 thickness color");
        return 1;
    }

    float x1 = (float)FeatherAsDouble(interp, argv[0], 0);
    float y1 = (float)FeatherAsDouble(interp, argv[1], 0);
    float x2 = (float)FeatherAsDouble(interp, argv[2], 0);
    float y2 = (float)FeatherAsDouble(interp, argv[3], 0);
    float thick = (float)FeatherAsDouble(interp, argv[4], 1.0);
    Color c = obj_to_color(interp, argv[5]);

    DrawLineEx((Vector2){x1, y1}, (Vector2){x2, y2}, thick, c);
    *result = FeatherString(interp, "", 0);
    return 0;
}

// draw_text <text> <x> <y> <size> <color>
static int cmd_draw_text(void *data, FeatherInterp interp,
                         size_t argc, FeatherObj *argv,
                         FeatherObj *result, FeatherObj *err) {
    (void)data;
    if (argc < 5) {
        *err = make_error(interp, "usage: draw_text text x y size color");
        return 1;
    }

    char text[1024];
    FeatherCopy(interp, argv[0], text, sizeof(text) - 1);
    text[1023] = '\0';

    int x = (int)FeatherAsInt(interp, argv[1], 0);
    int y = (int)FeatherAsInt(interp, argv[2], 0);
    int size = (int)FeatherAsInt(interp, argv[3], 20);
    Color c = obj_to_color(interp, argv[4]);

    DrawText(text, x, y, size, c);
    *result = FeatherString(interp, "", 0);
    return 0;
}

// draw_triangle <x1> <y1> <x2> <y2> <x3> <y3> <color>
static int cmd_draw_triangle(void *data, FeatherInterp interp,
                             size_t argc, FeatherObj *argv,
                             FeatherObj *result, FeatherObj *err) {
    (void)data;
    if (argc < 7) {
        *err = make_error(interp, "usage: draw_triangle x1 y1 x2 y2 x3 y3 color");
        return 1;
    }

    Vector2 v1 = {(float)FeatherAsDouble(interp, argv[0], 0),
                  (float)FeatherAsDouble(interp, argv[1], 0)};
    Vector2 v2 = {(float)FeatherAsDouble(interp, argv[2], 0),
                  (float)FeatherAsDouble(interp, argv[3], 0)};
    Vector2 v3 = {(float)FeatherAsDouble(interp, argv[4], 0),
                  (float)FeatherAsDouble(interp, argv[5], 0)};
    Color c = obj_to_color(interp, argv[6]);

    DrawTriangle(v1, v2, v3, c);
    *result = FeatherString(interp, "", 0);
    return 0;
}

// draw_poly <x> <y> <sides> <radius> <rotation> <color>
static int cmd_draw_poly(void *data, FeatherInterp interp,
                         size_t argc, FeatherObj *argv,
                         FeatherObj *result, FeatherObj *err) {
    (void)data;
    if (argc < 6) {
        *err = make_error(interp, "usage: draw_poly x y sides radius rotation color");
        return 1;
    }

    float x = (float)FeatherAsDouble(interp, argv[0], 0);
    float y = (float)FeatherAsDouble(interp, argv[1], 0);
    int sides = (int)FeatherAsInt(interp, argv[2], 3);
    float radius = (float)FeatherAsDouble(interp, argv[3], 10);
    float rotation = (float)FeatherAsDouble(interp, argv[4], 0);
    Color c = obj_to_color(interp, argv[5]);

    DrawPoly((Vector2){x, y}, sides, radius, rotation, c);
    *result = FeatherString(interp, "", 0);
    return 0;
}

// -----------------------------------------------------------------------------
// Input Commands
// -----------------------------------------------------------------------------

// mouse_x
static int cmd_mouse_x(void *data, FeatherInterp interp,
                       size_t argc, FeatherObj *argv,
                       FeatherObj *result, FeatherObj *err) {
    (void)data; (void)argc; (void)argv; (void)err;
    *result = FeatherInt(interp, GetMouseX());
    return 0;
}

// mouse_y
static int cmd_mouse_y(void *data, FeatherInterp interp,
                       size_t argc, FeatherObj *argv,
                       FeatherObj *result, FeatherObj *err) {
    (void)data; (void)argc; (void)argv; (void)err;
    *result = FeatherInt(interp, GetMouseY());
    return 0;
}

// mouse_pos -> {x y}
static int cmd_mouse_pos(void *data, FeatherInterp interp,
                         size_t argc, FeatherObj *argv,
                         FeatherObj *result, FeatherObj *err) {
    (void)data; (void)argc; (void)argv; (void)err;
    FeatherObj pos[2] = {
        FeatherInt(interp, GetMouseX()),
        FeatherInt(interp, GetMouseY())
    };
    *result = FeatherList(interp, 2, pos);
    return 0;
}

// mouse_down ?button? -> 0/1
static int cmd_mouse_down(void *data, FeatherInterp interp,
                          size_t argc, FeatherObj *argv,
                          FeatherObj *result, FeatherObj *err) {
    (void)data; (void)err;
    int button = (argc >= 1) ? (int)FeatherAsInt(interp, argv[0], 0) : 0;
    *result = FeatherInt(interp, IsMouseButtonDown(button) ? 1 : 0);
    return 0;
}

// mouse_pressed ?button? -> 0/1
static int cmd_mouse_pressed(void *data, FeatherInterp interp,
                             size_t argc, FeatherObj *argv,
                             FeatherObj *result, FeatherObj *err) {
    (void)data; (void)err;
    int button = (argc >= 1) ? (int)FeatherAsInt(interp, argv[0], 0) : 0;
    *result = FeatherInt(interp, IsMouseButtonPressed(button) ? 1 : 0);
    return 0;
}

// key_down <key> -> 0/1
static int cmd_key_down(void *data, FeatherInterp interp,
                        size_t argc, FeatherObj *argv,
                        FeatherObj *result, FeatherObj *err) {
    (void)data;
    if (argc < 1) {
        *err = make_error(interp, "usage: key_down key");
        return 1;
    }
    int key = (int)FeatherAsInt(interp, argv[0], 0);
    *result = FeatherInt(interp, IsKeyDown(key) ? 1 : 0);
    return 0;
}

// key_pressed <key> -> 0/1
static int cmd_key_pressed(void *data, FeatherInterp interp,
                           size_t argc, FeatherObj *argv,
                           FeatherObj *result, FeatherObj *err) {
    (void)data;
    if (argc < 1) {
        *err = make_error(interp, "usage: key_pressed key");
        return 1;
    }
    int key = (int)FeatherAsInt(interp, argv[0], 0);
    *result = FeatherInt(interp, IsKeyPressed(key) ? 1 : 0);
    return 0;
}

// -----------------------------------------------------------------------------
// Utility Commands
// -----------------------------------------------------------------------------

// screen_width
static int cmd_screen_width(void *data, FeatherInterp interp,
                            size_t argc, FeatherObj *argv,
                            FeatherObj *result, FeatherObj *err) {
    (void)data; (void)argc; (void)argv; (void)err;
    *result = FeatherInt(interp, GetScreenWidth());
    return 0;
}

// screen_height
static int cmd_screen_height(void *data, FeatherInterp interp,
                             size_t argc, FeatherObj *argv,
                             FeatherObj *result, FeatherObj *err) {
    (void)data; (void)argc; (void)argv; (void)err;
    *result = FeatherInt(interp, GetScreenHeight());
    return 0;
}

// frame_time -> delta time in seconds
static int cmd_frame_time(void *data, FeatherInterp interp,
                          size_t argc, FeatherObj *argv,
                          FeatherObj *result, FeatherObj *err) {
    (void)data; (void)argc; (void)argv; (void)err;
    *result = FeatherDouble(interp, GetFrameTime());
    return 0;
}

// get_time -> total time in seconds
static int cmd_get_time(void *data, FeatherInterp interp,
                        size_t argc, FeatherObj *argv,
                        FeatherObj *result, FeatherObj *err) {
    (void)data; (void)argc; (void)argv; (void)err;
    *result = FeatherDouble(interp, GetTime());
    return 0;
}

// get_fps
static int cmd_get_fps(void *data, FeatherInterp interp,
                       size_t argc, FeatherObj *argv,
                       FeatherObj *result, FeatherObj *err) {
    (void)data; (void)argc; (void)argv; (void)err;
    *result = FeatherInt(interp, GetFPS());
    return 0;
}

// random <min> <max>
static int cmd_random(void *data, FeatherInterp interp,
                      size_t argc, FeatherObj *argv,
                      FeatherObj *result, FeatherObj *err) {
    (void)data;
    if (argc < 2) {
        *err = make_error(interp, "usage: random min max");
        return 1;
    }
    int min = (int)FeatherAsInt(interp, argv[0], 0);
    int max = (int)FeatherAsInt(interp, argv[1], 100);
    *result = FeatherInt(interp, GetRandomValue(min, max));
    return 0;
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------

static void register_commands(FeatherInterp interp) {
    // Drawing
    FeatherRegister(interp, "clear", cmd_clear, NULL);
    FeatherRegister(interp, "draw_circle", cmd_draw_circle, NULL);
    FeatherRegister(interp, "draw_ring", cmd_draw_ring, NULL);
    FeatherRegister(interp, "draw_rect", cmd_draw_rect, NULL);
    FeatherRegister(interp, "draw_rect_lines", cmd_draw_rect_lines, NULL);
    FeatherRegister(interp, "draw_line", cmd_draw_line, NULL);
    FeatherRegister(interp, "draw_line_thick", cmd_draw_line_thick, NULL);
    FeatherRegister(interp, "draw_text", cmd_draw_text, NULL);
    FeatherRegister(interp, "draw_triangle", cmd_draw_triangle, NULL);
    FeatherRegister(interp, "draw_poly", cmd_draw_poly, NULL);

    // Input
    FeatherRegister(interp, "mouse_x", cmd_mouse_x, NULL);
    FeatherRegister(interp, "mouse_y", cmd_mouse_y, NULL);
    FeatherRegister(interp, "mouse_pos", cmd_mouse_pos, NULL);
    FeatherRegister(interp, "mouse_down", cmd_mouse_down, NULL);
    FeatherRegister(interp, "mouse_pressed", cmd_mouse_pressed, NULL);
    FeatherRegister(interp, "key_down", cmd_key_down, NULL);
    FeatherRegister(interp, "key_pressed", cmd_key_pressed, NULL);

    // Utility
    FeatherRegister(interp, "screen_width", cmd_screen_width, NULL);
    FeatherRegister(interp, "screen_height", cmd_screen_height, NULL);
    FeatherRegister(interp, "frame_time", cmd_frame_time, NULL);
    FeatherRegister(interp, "get_time", cmd_get_time, NULL);
    FeatherRegister(interp, "get_fps", cmd_get_fps, NULL);
    FeatherRegister(interp, "random", cmd_random, NULL);
}

static char* read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Could not open file: %s\n", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(size + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);

    return buf;
}

static void print_error(FeatherInterp interp, FeatherObj err) {
    char buf[1024];
    size_t n = FeatherCopy(interp, err, buf, sizeof(buf) - 1);
    buf[n] = '\0';
    fprintf(stderr, "Script error: %s\n", buf);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <script.tcl>\n", argv[0]);
        return 1;
    }

    // Load script
    char *script = read_file(argv[1]);
    if (!script) return 1;

    // Initialize raylib
    InitWindow(800, 600, "Feather + Raylib");
    SetTargetFPS(60);

    // Initialize Feather
    FeatherInterp interp = FeatherNew();
    register_commands(interp);

    // Run initialization script (defines procs, sets up state)
    FeatherObj result;
    int status = FeatherEval(interp, script, strlen(script), &result);
    if (status != 0) {
        print_error(interp, result);
        free(script);
        CloseWindow();
        FeatherClose(interp);
        return 1;
    }

    // Game loop - call update and draw procs each frame
    const char *update_script = "if {[info commands update] ne {}} { update }";
    const char *draw_script = "if {[info commands draw] ne {}} { draw }";

    while (!WindowShouldClose()) {
        // Update
        status = FeatherEval(interp, update_script, strlen(update_script), &result);
        if (status != 0) {
            print_error(interp, result);
            break;
        }

        // Draw
        BeginDrawing();

        status = FeatherEval(interp, draw_script, strlen(draw_script), &result);
        if (status != 0) {
            print_error(interp, result);
            EndDrawing();
            break;
        }

        EndDrawing();
    }

    // Cleanup
    free(script);
    CloseWindow();
    FeatherClose(interp);

    return 0;
}
