# Test: file readable on readable file
set f [open "/tmp/file-test-11.0.txt" w]
close $f
puts [file readable /tmp/file-test-11.0.txt]
file delete /tmp/file-test-11.0.txt
