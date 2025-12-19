# Test: expr complex precedence
puts [expr {1 + 2 * 3 - 4 / 2}]
puts [expr {(1 + 2) * (3 - 4) / 2}]
puts [expr {5 & 3 | 2}]
puts [expr {(5 & 3) | 2}]
