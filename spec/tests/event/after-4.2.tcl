# Test: after info id for idle handler
set id [after idle {set x done}]
set info [after info $id]
puts [lindex $info 0]
puts [lindex $info 1]
after cancel $id
