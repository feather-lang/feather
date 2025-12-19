# Test: dict exists - basic checks
set d [dict create a 1 b 2]
puts [dict exists $d a]
puts [dict exists $d b]
puts [dict exists $d c]
