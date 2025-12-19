# Test: upvar creates non-existent variable
proc createvar {name value} {
    upvar 1 $name var
    set var $value
}
createvar newvar "hello"
puts "newvar = $newvar"
