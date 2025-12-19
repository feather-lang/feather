# Test: file delete removes file
set f [open "/tmp/file-test-17.0.txt" w]
close $f
file delete /tmp/file-test-17.0.txt
puts [file exists /tmp/file-test-17.0.txt]
