# Test: dict unset - nonexistent key (no error)
set d [dict create a 1 b 2]
puts [dict unset d nonexistent]
puts $d
