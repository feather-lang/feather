# Test: chan read with size limit
set f [open "/tmp/chan-test-9.3.txt" w]
chan puts -nonewline $f "0123456789"
chan close $f
set f [open "/tmp/chan-test-9.3.txt" r]
puts [chan read $f 5]
chan close $f
file delete "/tmp/chan-test-9.3.txt"
