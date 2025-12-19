# Test: fileevent readable - no handler returns empty
set f [open [info script] r]
puts [fileevent $f readable]
close $f
