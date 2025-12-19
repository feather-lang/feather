# Test: dict info - returns implementation info
set d [dict create a 1 b 2 c 3]
set info [dict info $d]
puts [expr {[string length $info] > 0}]
