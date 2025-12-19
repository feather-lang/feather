# Test: lappend builds up a list incrementally
set var 1
lappend var 2
puts $var
lappend var 3 4 5
puts $var
