# Test: lappend treats list value as single element
set x {a}
lappend x {b c d}
puts $x
puts [llength $x]
