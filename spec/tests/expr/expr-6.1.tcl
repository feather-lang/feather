# Test: expr ternary with numeric results
puts [expr {1 ? 10 : 20}]
puts [expr {0 ? 10 : 20}]
puts [expr {5 > 3 ? 100 : 0}]
