# Test: split with dollar sign character
set result [split {a$b$c} {$}]
puts [llength $result]
puts [lindex $result 1]
