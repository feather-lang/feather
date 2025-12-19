# Test: dict replace - basic
set d [dict create a 1 b 2]
puts [dict replace $d b 20]
puts [dict replace $d c 3]
puts [dict replace $d a 10 b 20 c 30]
