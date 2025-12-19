# Test: lappend with empty string element
set x {a b}
lappend x ""
puts $x
puts [llength $x]
