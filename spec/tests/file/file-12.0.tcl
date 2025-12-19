# Test: file writable on writable file
set f [open "/tmp/file-test-12.0.txt" w]
close $f
puts [file writable /tmp/file-test-12.0.txt]
file delete /tmp/file-test-12.0.txt
