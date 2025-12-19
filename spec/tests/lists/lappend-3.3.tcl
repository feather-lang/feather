# Test: chained lappend calls
set x {}
lappend x a
lappend x b
lappend x c
puts $x
