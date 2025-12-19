# Test: dict lappend - new key
set d [dict create]
puts [dict lappend d newlist x y z]
puts $d
