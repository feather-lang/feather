# Test: chan seek and tell
set f [open "/tmp/chan-test-10.0.txt" w]
chan puts -nonewline $f "0123456789"
chan close $f
set f [open "/tmp/chan-test-10.0.txt" r]
puts [chan tell $f]
chan seek $f 5
puts [chan tell $f]
puts [chan read $f]
chan close $f
file delete "/tmp/chan-test-10.0.txt"
