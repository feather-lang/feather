# Test: dict getdef - basic with existing key
set d [dict create a 1 b 2]
puts [dict getdef $d a default]
puts [dict getdef $d b default]
