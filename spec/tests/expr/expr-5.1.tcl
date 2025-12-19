# Test: expr string not equal (ne)
puts [expr {"abc" ne "def"}]
puts [expr {"abc" ne "abc"}]
puts [expr {"" ne "x"}]
puts [expr {"hello" ne "Hello"}]
