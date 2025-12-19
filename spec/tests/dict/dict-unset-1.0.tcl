# Test: dict unset - basic
set d [dict create a 1 b 2 c 3]
puts [dict unset d b]
puts $d
