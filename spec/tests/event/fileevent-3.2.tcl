# Test: fileevent - invalid event type
set f [open [info script] r]
catch {fileevent $f invalid} result
close $f
puts $result
