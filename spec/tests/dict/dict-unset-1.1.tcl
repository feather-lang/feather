# Test: dict unset - nested key path
set d [dict create outer [dict create inner1 a inner2 b]]
puts [dict unset d outer inner1]
puts $d
