# Test: dict append - multiple strings
set d [dict create a x]
puts [dict append d a y z]
puts $d
