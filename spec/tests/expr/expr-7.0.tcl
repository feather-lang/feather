# Test: expr parentheses and precedence
puts [expr {2 + 3 * 4}]
puts [expr {(2 + 3) * 4}]
puts [expr {10 - 4 - 2}]
puts [expr {10 - (4 - 2)}]
