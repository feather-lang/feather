# Test: dict preserves insertion order
set d [dict create z 1 a 2 m 3]
puts [dict keys $d]
dict set d b 4
puts [dict keys $d]
