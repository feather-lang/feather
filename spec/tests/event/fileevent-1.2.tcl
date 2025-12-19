# Test: fileevent readable - clear handler with empty script
set f [open [info script] r]
fileevent $f readable {set done 1}
fileevent $f readable {}
puts [fileevent $f readable]
close $f
