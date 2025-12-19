# Test: expr command substitution
puts [expr {[string length "hello"] + 1}]
puts [expr {[llength {a b c}] * 2}]
puts [expr {[expr {2 + 3}] * 2}]
