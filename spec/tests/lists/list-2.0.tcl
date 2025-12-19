# Test: list roundtrip - lindex can extract original elements
set mylist [list a "b c" {d e}]
puts [lindex $mylist 0]
puts [lindex $mylist 1]
puts [lindex $mylist 2]
