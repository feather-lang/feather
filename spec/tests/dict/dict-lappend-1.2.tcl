# Test: dict lappend - no values
set d [dict create items {a b c}]
puts [dict lappend d items]
puts $d
