# Test: chan truncate
set f [open "/tmp/chan-test-12.0.txt" w]
chan puts -nonewline $f "0123456789"
chan close $f
set f [open "/tmp/chan-test-12.0.txt" r+]
chan truncate $f 5
chan close $f
set f [open "/tmp/chan-test-12.0.txt" r]
puts [chan read $f]
chan close $f
file delete "/tmp/chan-test-12.0.txt"
