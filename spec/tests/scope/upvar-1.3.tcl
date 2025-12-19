# Test: upvar command - level #0 refers to global
proc setglobal {name value} {
    upvar #0 $name var
    set var $value
}
set x 0
setglobal x 123
puts "x = $x"
