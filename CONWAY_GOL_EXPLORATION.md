# Conway's Game of Life - Feather TCL Implementation Guide

## Executive Summary

Based on comprehensive exploration of the `help` command, Feather TCL provides all necessary primitives to implement Conway's Game of Life. This document catalogs available commands and provides an implementation strategy.

## Available Commands by Category

### 1. Data Structures

#### Lists (Primary data structure for grid representation)
- `list` - Create a list
- `lindex <list> <index>` - Retrieve an element from a list
- `llength <list>` - Return the number of elements in a list
- `lappend <varName> ?value...?` - Append list elements onto a variable
- `lset <varName> <index> <value>` - Change an element in a list
- `linsert <list> <index> ?element...?` - Insert elements into a list
- `lrange <list> <first> <last>` - Return one or more adjacent elements from a list
- `lreplace <list> <first> <last> ?element...?` - Replace elements in a list
- `lrepeat <count> ?element...?` - Build a list by repeating elements
- `lreverse <list>` - Reverse the elements of a list
- `lsort <list>` - Sort the elements of a list
- `lsearch <list> <pattern>` - See if a list contains a particular element
- `lmap <varname> <list> <body>` - Map a script over one or more lists
- `lassign <list> <varName> ?varName...?` - Assign list elements to variables

#### Dictionaries (For configuration/state)
- `dict create ?key value...?` - Manipulate dictionaries
- `dict get <dict> <key>` - Get value from dictionary
- `dict set <dictVar> <key> <value>` - Set value in dictionary
- `dict exists <dict> <key>` - Check if key exists
- `dict keys <dict>` - Get all keys
- `dict values <dict>` - Get all values
- `dict size <dict>` - Get number of entries
- `dict for <keyVar> <valueVar> <dict> <body>` - Iterate over dictionary

### 2. Control Flow

#### Loops
- `for <start> <test> <next> <body>` - 'For' loop
  - Example: `for {set i 0} {$i < 10} {incr i} { ... }`
- `while <test> <body>` - Execute script repeatedly as long as a condition is met
- `foreach <varname> <list> <body>` - Iterate over all elements in one or more lists
- `break` - Abort looping command
- `continue` - Skip to the next iteration of a loop

#### Conditionals
- `if <expr> <body> ?elseif <expr> <body>...? ?else <body>?` - Execute scripts conditionally
- `switch <string> ?pattern body...?` - Evaluate one of several scripts, depending on a given value

### 3. Procedures and Functions

- `proc <name> <args> <body>` - Create a TCL procedure
- `return ?-code code? ?value?` - Return from a procedure
- `apply <lambda> ?arg...?` - Apply an anonymous function
  - Lambda format: `{args body ?namespace?}`
- `uplevel ?level? <script>` - Execute a script in a different stack frame
- `upvar ?level? <otherVar> <myVar>` - Create link to variable in a different stack frame

### 4. Variables

- `set <varName> ?value?` - Read and write variables
- `global <varName> ?varName...?` - Access global variables
- `variable <name> ?value?` - Create and initialize a namespace variable
- `unset ?-nocomplain? ?varName...?` - Delete variables
- `incr <varName> ?increment?` - Increment the value of a variable
- `append <varName> ?value...?` - Append to variable

### 5. Mathematical Operations

- `expr <arg> ?arg...?` - Evaluate an expression
  - Supports: `+`, `-`, `*`, `/`, `%` (modulo)
  - Comparisons: `<`, `<=`, `>`, `>=`, `==`, `!=`
  - Logical: `&&`, `||`, `!`
  - Bitwise: `&`, `|`, `^`, `~`, `<<`, `>>`
  - Parentheses for grouping
  - Mathematical functions available via `tcl::mathfunc::`

- `incr <varName> ?increment?` - Increment the value of a variable (efficient for +1/-1)

### 6. String Manipulation

- `string length <string>` - Get string length
- `string index <string> <index>` - Get character at index
- `string range <string> <first> <last>` - Get substring
- `string repeat <string> <count>` - Repeat string
- `string replace <string> <first> <last> ?newstring?` - Replace substring
- `string cat ?string...?` - Concatenate strings
- `string match <pattern> <string>` - Pattern matching with wildcards
- `string equal <str1> <str2>` - String equality
- `string compare <str1> <str2>` - String comparison
- `string map <mapping> <string>` - Map characters/substrings
- `format <formatString> ?arg...?` - Format a string in the style of sprintf
- `scan <string> <format> ?varName...?` - Parse string using conversion specifiers
- `join <list> ?separator?` - Create a string by joining list elements
- `split <string> ?splitChars?` - Split string into list elements

### 7. Utility Commands

- `eval <arg> ?arg...?` - Evaluate a Tcl script
- `subst ?-nobackslashes? ?-nocommands? ?-novariables? <string>` - Perform substitutions
- `catch <script> ?varName?` - Evaluate script and trap exceptional returns
- `error <message> ?info? ?code?` - Generate an error
- `try <body> ?on code varList handler...? ?finally script?` - Trap and process errors

### 8. Information and Introspection

- `info commands ?pattern?` - List available commands
- `info exists <varName>` - Check if variable exists
- `info globals ?pattern?` - List global variables
- `info locals ?pattern?` - List local variables
- `info vars ?pattern?` - List all visible variables
- `info procs ?pattern?` - List defined procedures
- `info args <procname>` - Get procedure arguments
- `info body <procname>` - Get procedure body

## Implementation Strategy for Conway's Game of Life

### Data Representation

**Grid as List of Lists**
```tcl
# 10x10 grid initialized to 0 (dead cells)
proc create_grid {width height} {
    set grid [list]
    for {set y 0} {$y < $height} {incr y} {
        set row [lrepeat $width 0]
        lappend grid $row
    }
    return $grid
}

# Get cell value
proc get_cell {grid x y} {
    set row [lindex $grid $y]
    return [lindex $row $x]
}

# Set cell value
proc set_cell {gridVar x y value} {
    upvar $gridVar g
    set row [lindex $g $y]
    set row [lreplace $row $x $x $value]
    set g [lreplace $g $y $y $row]
}
```

### Core Game of Life Logic

```tcl
# Count live neighbors (8-directional)
proc count_neighbors {grid x y width height} {
    set count 0
    for {set dy -1} {$dy <= 1} {incr dy} {
        for {set dx -1} {$dx <= 1} {incr dx} {
            if {$dx == 0 && $dy == 0} continue

            set nx [expr {$x + $dx}]
            set ny [expr {$y + $dy}]

            # Boundary check
            if {$nx >= 0 && $nx < $width && $ny >= 0 && $ny < $height} {
                set cell [get_cell $grid $nx $ny]
                set count [expr {$count + $cell}]
            }
        }
    }
    return $count
}

# Apply Conway's rules
proc next_generation {grid width height} {
    set new_grid [create_grid $width $height]

    for {set y 0} {$y < $height} {incr y} {
        for {set x 0} {$x < $width} {incr x} {
            set cell [get_cell $grid $x $y]
            set neighbors [count_neighbors $grid $x $y $width $height]

            # Conway's rules:
            # 1. Live cell with 2-3 neighbors survives
            # 2. Dead cell with exactly 3 neighbors becomes alive
            # 3. Otherwise dies/stays dead

            if {$cell == 1} {
                # Cell is alive
                if {$neighbors == 2 || $neighbors == 3} {
                    set_cell new_grid $x $y 1
                }
            } else {
                # Cell is dead
                if {$neighbors == 3} {
                    set_cell new_grid $x $y 1
                }
            }
        }
    }
    return $new_grid
}
```

### Display/Output

```tcl
# Print grid to screen (no puts command, but we have echo)
proc display_grid {grid} {
    set output ""
    foreach row $grid {
        set line ""
        foreach cell $row {
            if {$cell == 1} {
                set line "${line}█"
            } else {
                set line "${line}·"
            }
        }
        echo $line
    }
}

# Alternative using format
proc display_grid_formatted {grid} {
    foreach row $grid {
        set line [string map {0 "· " 1 "█ "} [join $row ""]]
        echo $line
    }
}
```

### Main Simulation Loop

```tcl
proc run_game_of_life {initial_grid width height generations} {
    set grid $initial_grid

    for {set gen 0} {$gen < $generations} {incr gen} {
        echo "Generation: $gen"
        display_grid $grid
        echo ""
        set grid [next_generation $grid $width $height]
    }
}
```

### Example Patterns

```tcl
# Glider pattern
proc create_glider {width height} {
    set grid [create_grid $width $height]
    # Glider at position (1,0)
    set_cell grid 2 0 1
    set_cell grid 3 1 1
    set_cell grid 1 2 1
    set_cell grid 2 2 1
    set_cell grid 3 2 1
    return $grid
}

# Blinker pattern
proc create_blinker {width height} {
    set grid [create_grid $width $height]
    set mid [expr {$width / 2}]
    set_cell grid $mid [expr {$height / 2 - 1}] 1
    set_cell grid $mid [expr {$height / 2}] 1
    set_cell grid $mid [expr {$height / 2 + 1}] 1
    return $grid
}
```

## Key Features Available

### ✅ Present in Feather
1. **Lists** - Primary data structure (nested lists for 2D grid)
2. **Dictionaries** - For configuration/metadata
3. **Arithmetic expressions** - Full expr support with all operators
4. **Loops** - for, while, foreach
5. **Conditionals** - if/elseif/else, switch
6. **Procedures** - First-class procedures with upvar for pass-by-reference
7. **String manipulation** - Comprehensive string operations
8. **Increment** - Efficient incr command
9. **List operations** - lindex, lset, lreplace, etc.

### ❌ Limitations Identified

1. **No `puts` command** - Must use `echo` for output (test-specific command)
   - In production, would need to use string building and return values
2. **No arrays** - Feather explicitly doesn't support TCL-style arrays
   - Must use lists or dicts instead
3. **No file I/O visible** - No open/close/read/write commands in help
   - Likely handled by host, not exposed to TCL level
4. **No time/sleep commands** - Can't add delays between generations
   - Would need to be driven externally
5. **No random** - Can't generate random initial states
   - Would need to pass in patterns

### Workarounds

1. **Output**: Build strings and return them, or use `echo` if available
2. **Arrays**: Use list of lists (works well for 2D grids)
3. **File I/O**: Not needed for basic simulation
4. **Animation**: External driver can call the simulation repeatedly
5. **Random**: Pre-define patterns or accept them as input

## Recommended Implementation Approach

### Phase 1: Core Engine (Pure Functions)
```tcl
# All procedures are pure functions that take input and return output
proc create_grid {width height} { ... }
proc get_cell {grid x y} { ... }
proc set_cell_value {grid x y value} { ... }  # Returns new grid
proc count_neighbors {grid x y width height} { ... }
proc next_generation {grid width height} { ... }
```

### Phase 2: Patterns Library
```tcl
proc pattern_glider {} { ... }
proc pattern_blinker {} { ... }
proc pattern_toad {} { ... }
proc pattern_beacon {} { ... }
proc pattern_pulsar {} { ... }
```

### Phase 3: Display
```tcl
proc grid_to_string {grid} { ... }  # Returns formatted string
proc display_grid {grid} { ... }   # Uses echo if available
```

### Phase 4: Simulation Driver
```tcl
proc run_simulation {pattern_name width height generations} { ... }
```

## Complete Minimal Example

```tcl
# Complete Conway's Game of Life in Feather TCL

proc create_grid {width height} {
    set grid [list]
    for {set y 0} {$y < $height} {incr y} {
        lappend grid [lrepeat $width 0]
    }
    return $grid
}

proc get_cell {grid x y} {
    lindex [lindex $grid $y] $x
}

proc set_cell_immutable {grid x y value} {
    set row [lindex $grid $y]
    set row [lreplace $row $x $x $value]
    lreplace $grid $y $y $row
}

proc count_neighbors {grid x y width height} {
    set count 0
    foreach dy {-1 0 1} {
        foreach dx {-1 0 1} {
            if {$dx == 0 && $dy == 0} continue
            set nx [expr {$x + $dx}]
            set ny [expr {$y + $dy}]
            if {$nx >= 0 && $nx < $width && $ny >= 0 && $ny < $height} {
                incr count [get_cell $grid $nx $ny]
            }
        }
    }
    return $count
}

proc next_gen {grid width height} {
    set new_grid [create_grid $width $height]
    for {set y 0} {$y < $height} {incr y} {
        for {set x 0} {$x < $width} {incr x} {
            set alive [get_cell $grid $x $y]
            set n [count_neighbors $grid $x $y $width $height]
            if {($alive && ($n == 2 || $n == 3)) || (!$alive && $n == 3)} {
                set new_grid [set_cell_immutable $new_grid $x $y 1]
            }
        }
    }
    return $new_grid
}

# Test it
set grid [create_grid 5 5]
set grid [set_cell_immutable $grid 1 2 1]
set grid [set_cell_immutable $grid 2 2 1]
set grid [set_cell_immutable $grid 3 2 1]

# Run 3 generations
for {set i 0} {$i < 3} {incr i} {
    echo "Generation $i:"
    foreach row $grid {
        echo [string map {0 "· " 1 "█ "} [join $row " "]]
    }
    set grid [next_gen $grid 5 5]
}
```

## Conclusion

Feather TCL has **all the necessary primitives** to implement Conway's Game of Life:
- ✅ 2D data structures (list of lists)
- ✅ Arithmetic and comparisons
- ✅ Loops and conditionals
- ✅ Procedures and state management
- ✅ String formatting for display

The main limitation is output (no `puts`), but `echo` works for testing, and production code can return formatted strings for the host to display.

The implementation is straightforward and idiomatic TCL, using immutable operations where possible and mutable operations (via upvar) where performance is needed.
