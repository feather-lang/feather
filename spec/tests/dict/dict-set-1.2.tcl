# Test: dict set - nested key path
set d [dict create]
puts [dict set d a b c value]
puts [dict get $d a b c]
