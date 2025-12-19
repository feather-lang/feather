# Test: dict getdef - nested path
set d [dict create outer [dict create inner value]]
puts [dict getdef $d outer inner default]
puts [dict getdef $d outer missing default]
