# Test: dict exists - nested path
set d [dict create outer [dict create inner value]]
puts [dict exists $d outer]
puts [dict exists $d outer inner]
puts [dict exists $d outer missing]
puts [dict exists $d missing inner]
