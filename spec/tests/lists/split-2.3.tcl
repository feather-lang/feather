# Test: split handles special list characters - creates valid list
set result [split "a{b}c" ""]
puts [llength $result]
