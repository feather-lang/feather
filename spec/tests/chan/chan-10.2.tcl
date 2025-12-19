# Test: chan seek from current
set f [open "/tmp/chan-test-10.2.txt" w]
chan puts -nonewline $f "0123456789"
chan close $f
set f [open "/tmp/chan-test-10.2.txt" r]
chan read $f 3
chan seek $f 2 current
puts [chan read $f]
chan close $f
file delete "/tmp/chan-test-10.2.txt"
