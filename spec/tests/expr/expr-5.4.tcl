# Test: expr string less than or equal (le)
puts [expr {"abc" le "abd"}]
puts [expr {"abc" le "abc"}]
puts [expr {"abc" le "abb"}]
