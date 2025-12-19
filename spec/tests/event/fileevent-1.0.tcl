# Test: fileevent readable - set handler
set f [open [info script] r]
fileevent $f readable {set done 1}
set handler [fileevent $f readable]
puts [expr {$handler ne ""}]
close $f
