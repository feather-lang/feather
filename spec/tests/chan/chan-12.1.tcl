# Test: chan truncate at current position
set f [open "/tmp/chan-test-12.1.txt" w]
chan puts -nonewline $f "0123456789"
chan close $f
set f [open "/tmp/chan-test-12.1.txt" r+]
chan seek $f 3
chan truncate $f
chan close $f
set f [open "/tmp/chan-test-12.1.txt" r]
puts [chan read $f]
chan close $f
file delete "/tmp/chan-test-12.1.txt"
