# Test: file executable on non-executable file
set f [open "/tmp/file-test-13.1.txt" w]
close $f
puts [file executable /tmp/file-test-13.1.txt]
file delete /tmp/file-test-13.1.txt
