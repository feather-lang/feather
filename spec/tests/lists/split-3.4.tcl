# Test: split string containing backslash
set result [split {a\b\c} "\\"]
puts [llength $result]
