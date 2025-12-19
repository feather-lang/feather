# Test: dict remove - basic
set d [dict create a 1 b 2 c 3]
puts [dict remove $d b]
puts [dict remove $d a c]
puts [dict remove $d]
