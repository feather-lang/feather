# Test: expr nested ternary
puts [expr {1 ? (2 ? "a" : "b") : "c"}]
puts [expr {0 ? "a" : (1 ? "b" : "c")}]
