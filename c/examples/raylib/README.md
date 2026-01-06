# Feather + Raylib Example

This example demonstrates embedding Feather as a scripting language for a raylib game.

## Building

1. Build libfeather from the project root:
   ```
   mise build
   ```

2. Install raylib:
   ```
   # Debian/Ubuntu
   apt install libraylib-dev

   # macOS
   brew install raylib

   # Or build from source: https://github.com/raysan5/raylib
   ```

3. Build the game:
   ```
   make
   ```

## Running

```
make run
# or
./game demo.tcl
```

Click anywhere to spawn bouncing balls!

## API Reference

### Drawing Commands

| Command | Description |
|---------|-------------|
| `clear <color>` | Clear screen with color |
| `draw_circle <x> <y> <radius> <color>` | Draw filled circle |
| `draw_ring <x> <y> <inner> <outer> <color>` | Draw ring/donut |
| `draw_rect <x> <y> <w> <h> <color>` | Draw filled rectangle |
| `draw_rect_lines <x> <y> <w> <h> <color>` | Draw rectangle outline |
| `draw_line <x1> <y1> <x2> <y2> <color>` | Draw line |
| `draw_line_thick <x1> <y1> <x2> <y2> <thick> <color>` | Draw thick line |
| `draw_text <text> <x> <y> <size> <color>` | Draw text |
| `draw_triangle <x1> <y1> <x2> <y2> <x3> <y3> <color>` | Draw filled triangle |
| `draw_poly <x> <y> <sides> <radius> <rotation> <color>` | Draw regular polygon |

### Input Commands

| Command | Description |
|---------|-------------|
| `mouse_x` | Get mouse X position |
| `mouse_y` | Get mouse Y position |
| `mouse_pos` | Get mouse position as `{x y}` list |
| `mouse_down ?button?` | Check if mouse button is held (0=left, 1=right, 2=middle) |
| `mouse_pressed ?button?` | Check if mouse button was just pressed |
| `key_down <key>` | Check if key is held (raylib key code) |
| `key_pressed <key>` | Check if key was just pressed |

### Utility Commands

| Command | Description |
|---------|-------------|
| `screen_width` | Get screen width |
| `screen_height` | Get screen height |
| `frame_time` | Get delta time in seconds |
| `get_time` | Get total elapsed time |
| `get_fps` | Get current FPS |
| `random <min> <max>` | Generate random integer in range |

### Colors

Colors can be specified as:
- Named colors: `red`, `green`, `blue`, `yellow`, `orange`, `pink`, `purple`, `white`, `black`, `gray`, `darkgray`, `lightgray`, `skyblue`, `darkblue`, `darkgreen`
- RGB list: `{255 128 0}`
- RGBA list: `{255 128 0 200}`

## Script Structure

Your script should define two procs:

```tcl
proc update {} {
    # Called each frame before drawing
    # Handle input, update game state
}

proc draw {} {
    # Called each frame inside BeginDrawing/EndDrawing
    # All drawing commands go here
}
```

Global variables persist across frames, so use them for game state.
