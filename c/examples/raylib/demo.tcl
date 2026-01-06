# Feather + Raylib Demo
#
# Demonstrates:
# - Game state management with dicts
# - Spawning entities with mouse clicks
# - Physics-like bouncing animation
# - Drawing primitives
# - Real-time input handling

# -----------------------------------------------------------------------------
# Game State
# -----------------------------------------------------------------------------

set balls {}
set spawn_cooldown 0

# Create a new ball at position with random velocity
proc make_ball {x y} {
    set vx [expr {[random -200 200]}]
    set vy [expr {[random -300 -100]}]
    set radius [random 10 30]
    set r [random 100 255]
    set g [random 100 255]
    set b [random 100 255]

    dict create \
        x $x \
        y $y \
        vx $vx \
        vy $vy \
        radius $radius \
        color [list $r $g $b]
}

# -----------------------------------------------------------------------------
# Update - called each frame before drawing
# -----------------------------------------------------------------------------

proc update {} {
    global balls spawn_cooldown

    set dt [frame_time]
    set gravity 500
    set damping 0.8
    set w [screen_width]
    set h [screen_height]

    # Spawn ball on click (with cooldown)
    if {$spawn_cooldown > 0} {
        set spawn_cooldown [expr {$spawn_cooldown - $dt}]
    }

    if {[mouse_pressed] && $spawn_cooldown <= 0} {
        set mx [mouse_x]
        set my [mouse_y]
        lappend balls [make_ball $mx $my]
        set spawn_cooldown 0.1
    }

    # Update balls
    set updated {}
    foreach ball $balls {
        set x [dict get $ball x]
        set y [dict get $ball y]
        set vx [dict get $ball vx]
        set vy [dict get $ball vy]
        set r [dict get $ball radius]

        # Apply gravity
        set vy [expr {$vy + $gravity * $dt}]

        # Move
        set x [expr {$x + $vx * $dt}]
        set y [expr {$y + $vy * $dt}]

        # Bounce off walls
        if {$x - $r < 0} {
            set x $r
            set vx [expr {-$vx * $damping}]
        }
        if {$x + $r > $w} {
            set x [expr {$w - $r}]
            set vx [expr {-$vx * $damping}]
        }

        # Bounce off floor
        if {$y + $r > $h} {
            set y [expr {$h - $r}]
            set vy [expr {-$vy * $damping}]
            # Friction on floor
            set vx [expr {$vx * 0.95}]
        }

        # Bounce off ceiling
        if {$y - $r < 0} {
            set y $r
            set vy [expr {-$vy * $damping}]
        }

        # Update ball
        dict set ball x $x
        dict set ball y $y
        dict set ball vx $vx
        dict set ball vy $vy

        lappend updated $ball
    }
    set balls $updated

    # Limit number of balls
    if {[llength $balls] > 50} {
        set balls [lrange $balls end-49 end]
    }
}

# -----------------------------------------------------------------------------
# Draw - called each frame inside BeginDrawing/EndDrawing
# -----------------------------------------------------------------------------

proc draw {} {
    global balls

    clear darkblue

    # Draw title
    draw_text "Click to spawn balls!" 10 10 24 white
    draw_text "Balls: [llength $balls]" 10 40 20 lightgray
    draw_text "FPS: [get_fps]" 10 65 20 lightgray

    # Draw balls with shadows
    foreach ball $balls {
        set x [dict get $ball x]
        set y [dict get $ball y]
        set r [dict get $ball radius]
        set color [dict get $ball color]

        # Shadow
        draw_circle [expr {int($x + 3)}] [expr {int($y + 3)}] $r {0 0 0 100}

        # Ball
        draw_circle [expr {int($x)}] [expr {int($y)}] $r $color

        # Highlight
        set hr [expr {$r * 0.3}]
        set hx [expr {int($x - $r * 0.3)}]
        set hy [expr {int($y - $r * 0.3)}]
        draw_circle $hx $hy $hr {255 255 255 80}
    }

    # Draw mouse cursor
    set mx [mouse_x]
    set my [mouse_y]
    draw_ring $mx $my 15 18 white

    if {[mouse_down]} {
        draw_circle $mx $my 12 {255 255 255 150}
    }
}
