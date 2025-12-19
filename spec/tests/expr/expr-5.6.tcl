# Test: expr list containment (in)
puts [expr {"a" in {a b c}}]
puts [expr {"d" in {a b c}}]
puts [expr {"" in {a b c}}]
puts [expr {"b" in {a b c}}]
