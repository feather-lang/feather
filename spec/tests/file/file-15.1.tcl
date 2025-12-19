# Test: file atime returns integer
set f [open "/tmp/file-test-15.1.txt" w]
close $f
puts [string is integer [file atime /tmp/file-test-15.1.txt]]
file delete /tmp/file-test-15.1.txt
