# Test: lappend with special characters needing quoting
set x {a}
lappend x {$var} {[cmd]}
puts $x
