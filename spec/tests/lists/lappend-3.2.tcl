# Test: lappend with braces in element
set x {a}
lappend x "b{c}d"
puts $x
