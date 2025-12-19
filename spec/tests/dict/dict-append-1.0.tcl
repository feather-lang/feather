# Test: dict append - basic
set d [dict create a hello]
puts [dict append d a " world"]
puts $d
