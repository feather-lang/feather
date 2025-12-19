# Test: fileevent readable - get handler
set f [open [info script] r]
fileevent $f readable {puts hello}
puts [fileevent $f readable]
close $f
