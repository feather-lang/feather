# Test: after info id - get info about specific handler
set id [after 1000 {set x done}]
set info [after info $id]
puts [lindex $info 0]
puts [lindex $info 1]
after cancel $id
