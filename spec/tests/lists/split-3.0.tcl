# Test: split produces valid list from string with braces
set result [split "a}b{c" ""]
puts [lindex $result 0]
puts [lindex $result 1]
puts [lindex $result 2]
