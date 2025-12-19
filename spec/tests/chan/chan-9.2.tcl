# Test: chan read entire file
set f [open "/tmp/chan-test-9.2.txt" w]
chan puts -nonewline $f "content"
chan close $f
set f [open "/tmp/chan-test-9.2.txt" r]
puts [chan read $f]
chan close $f
file delete "/tmp/chan-test-9.2.txt"
