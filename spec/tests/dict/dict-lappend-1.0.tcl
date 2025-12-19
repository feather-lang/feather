# Test: dict lappend - basic
set d [dict create items {a b}]
puts [dict lappend d items c]
puts $d
