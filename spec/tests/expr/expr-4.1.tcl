# Test: expr logical OR
puts [expr {1 || 1}]
puts [expr {1 || 0}]
puts [expr {0 || 1}]
puts [expr {0 || 0}]
puts [expr {5 || 0}]
