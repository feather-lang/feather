# Test: lappend properly quotes elements with spaces
set x {a b}
lappend x "c d"
puts $x
