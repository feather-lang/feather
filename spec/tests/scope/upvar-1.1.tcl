# Test: upvar command - basic usage
proc setvar {name value} {
    upvar 1 $name var
    set var $value
}
set x 0
setvar x 42
puts "x = $x"
