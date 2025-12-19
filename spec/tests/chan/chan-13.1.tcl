# Test: chan configure -translation binary
set f [open "/tmp/chan-test-13.1.txt" w]
chan configure $f -translation binary
puts [chan configure $f -translation]
chan close $f
file delete "/tmp/chan-test-13.1.txt"
