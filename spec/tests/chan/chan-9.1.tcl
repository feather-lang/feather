# Test: chan gets with variable for line length
set f [open "/tmp/chan-test-9.1.txt" w]
chan puts $f "hello"
chan close $f
set f [open "/tmp/chan-test-9.1.txt" r]
set len [chan gets $f line]
puts $len
puts $line
chan close $f
file delete "/tmp/chan-test-9.1.txt"
