# Test: chan seek from end
set f [open "/tmp/chan-test-10.1.txt" w]
chan puts -nonewline $f "0123456789"
chan close $f
set f [open "/tmp/chan-test-10.1.txt" r]
chan seek $f -3 end
puts [chan read $f]
chan close $f
file delete "/tmp/chan-test-10.1.txt"
