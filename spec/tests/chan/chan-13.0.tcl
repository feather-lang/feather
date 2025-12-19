# Test: chan configure set and query -translation
set f [open "/tmp/chan-test-13.0.txt" w]
chan configure $f -translation lf
puts [chan configure $f -translation]
chan close $f
file delete "/tmp/chan-test-13.0.txt"
