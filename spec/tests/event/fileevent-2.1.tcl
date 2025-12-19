# Test: fileevent writable - get handler
set f [file tempfile tmp]
fileevent $f writable {puts hello}
puts [fileevent $f writable]
close $f
file delete $tmp
