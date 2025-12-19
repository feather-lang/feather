# Test: chan copy between files
set f1 [open "/tmp/chan-test-14.0-src.txt" w]
chan puts -nonewline $f1 "copy this"
chan close $f1
set f1 [open "/tmp/chan-test-14.0-src.txt" r]
set f2 [open "/tmp/chan-test-14.0-dst.txt" w]
puts [chan copy $f1 $f2]
chan close $f1
chan close $f2
set f2 [open "/tmp/chan-test-14.0-dst.txt" r]
puts [chan read $f2]
chan close $f2
file delete "/tmp/chan-test-14.0-src.txt" "/tmp/chan-test-14.0-dst.txt"
