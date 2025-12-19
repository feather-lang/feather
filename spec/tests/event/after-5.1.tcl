# Test: after cancel script - cancel by script
set id [after 1000 {set x done}]
after cancel {set x done}
puts [expr {[lsearch -exact [after info] $id] < 0}]
