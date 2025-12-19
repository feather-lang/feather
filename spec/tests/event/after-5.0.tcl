# Test: after cancel id - cancel by ID
set id [after 1000 {set x done}]
after cancel $id
puts [expr {[lsearch -exact [after info] $id] < 0}]
