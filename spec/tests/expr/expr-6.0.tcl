# Test: expr ternary operator
puts [expr {1 ? "yes" : "no"}]
puts [expr {0 ? "yes" : "no"}]
puts [expr {5 > 3 ? "greater" : "less"}]
puts [expr {2 < 1 ? "true" : "false"}]
