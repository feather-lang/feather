# Test: expr string less than (lt)
puts [expr {"abc" lt "abd"}]
puts [expr {"abc" lt "abc"}]
puts [expr {"abc" lt "abb"}]
puts [expr {"A" lt "a"}]
