# Test: chan copy with -size limit
set f1 [open "/tmp/chan-test-14.1-src.txt" w]
chan puts -nonewline $f1 "0123456789"
chan close $f1
set f1 [open "/tmp/chan-test-14.1-src.txt" r]
set f2 [open "/tmp/chan-test-14.1-dst.txt" w]
puts [chan copy $f1 $f2 -size 5]
chan close $f1
chan close $f2
set f2 [open "/tmp/chan-test-14.1-dst.txt" r]
puts [chan read $f2]
chan close $f2
file delete "/tmp/chan-test-14.1-src.txt" "/tmp/chan-test-14.1-dst.txt"
