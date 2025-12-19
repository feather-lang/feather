# Test: dict remove - nonexistent key (no error)
set d [dict create a 1 b 2]
puts [dict remove $d nonexistent]
puts [dict remove $d a nonexistent b]
