# Test: chan eof after reading entire file
set f [open "/tmp/chan-test-11.0.txt" w]
chan puts $f "data"
chan close $f
set f [open "/tmp/chan-test-11.0.txt" r]
chan read $f
puts [chan eof $f]
chan close $f
file delete "/tmp/chan-test-11.0.txt"
