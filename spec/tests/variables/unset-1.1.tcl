# Test: unset removes variable
# Variable no longer exists after unset

set x hello
puts $x
unset x
puts [info exists x]
