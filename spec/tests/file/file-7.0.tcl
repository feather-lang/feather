# Test: file exists on existing file
set f [open "/tmp/file-test-7.0.txt" w]
close $f
puts [file exists /tmp/file-test-7.0.txt]
file delete /tmp/file-test-7.0.txt
