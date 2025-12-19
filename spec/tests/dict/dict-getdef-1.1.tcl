# Test: dict getdef - missing key returns default
set d [dict create a 1]
puts [dict getdef $d missing default]
puts [dict getdef $d another "not found"]
