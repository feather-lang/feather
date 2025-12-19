# Test: vwait basic - wait for variable to be set by timer
set x ""
after 10 {set x done}
vwait x
puts $x
