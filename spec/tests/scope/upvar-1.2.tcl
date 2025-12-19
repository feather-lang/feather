# Test: upvar command - default level is 1
proc setvar {name value} {
    upvar $name var
    set var $value
}
set x 0
setvar x 99
puts "x = $x"
