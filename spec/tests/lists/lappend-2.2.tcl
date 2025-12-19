# Test: lappend to non-existent variable with no values
lappend newvar
puts "[llength $newvar]."
