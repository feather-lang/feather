# Test: chan open, write, close, read cycle
set f [open "/tmp/chan-test-9.0.txt" w]
chan puts $f "line one"
chan puts $f "line two"
chan close $f
set f [open "/tmp/chan-test-9.0.txt" r]
puts [chan gets $f]
puts [chan gets $f]
chan close $f
file delete "/tmp/chan-test-9.0.txt"
