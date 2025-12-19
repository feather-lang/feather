# Test: file isfile on regular file
set f [open "/tmp/file-test-8.0.txt" w]
close $f
puts [file isfile /tmp/file-test-8.0.txt]
file delete /tmp/file-test-8.0.txt
