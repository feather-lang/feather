# Test: expr list non-containment (ni)
puts [expr {"d" ni {a b c}}]
puts [expr {"a" ni {a b c}}]
puts [expr {"" ni {a b c}}]
puts [expr {"x" ni {}}]
