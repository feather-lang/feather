# Test: dict set - update existing key
set d [dict create a 1 b 2]
puts [dict set d a 10]
puts $d
