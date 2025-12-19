# Test: fileevent writable - set handler
set f [file tempfile tmp]
fileevent $f writable {set done 1}
set handler [fileevent $f writable]
puts [expr {$handler ne ""}]
close $f
file delete $tmp
