# Test: dict get - nested dictionary access
set d [dict create outer [dict create inner value]]
puts [dict get $d outer inner]
puts [dict get $d outer]
