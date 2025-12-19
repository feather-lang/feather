# Test: dict get - basic retrieval
set d [dict create a 1 b 2 c 3]
puts [dict get $d a]
puts [dict get $d b]
puts [dict get $d c]
