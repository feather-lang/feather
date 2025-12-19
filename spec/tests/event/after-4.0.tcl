# Test: after info - list all pending handlers
set id1 [after 1000 {set x 1}]
set id2 [after 2000 {set y 2}]
set ids [after info]
puts [expr {[lsearch -exact $ids $id1] >= 0}]
puts [expr {[lsearch -exact $ids $id2] >= 0}]
after cancel $id1
after cancel $id2
