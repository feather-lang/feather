# Test: file type on regular file
set f [open "/tmp/file-test-10.0.txt" w]
close $f
puts [file type /tmp/file-test-10.0.txt]
file delete /tmp/file-test-10.0.txt
