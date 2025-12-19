# Test: file mtime returns integer
set f [open "/tmp/file-test-15.0.txt" w]
close $f
puts [string is integer [file mtime /tmp/file-test-15.0.txt]]
file delete /tmp/file-test-15.0.txt
