# Test: file size empty file
set f [open "/tmp/file-test-14.1.txt" w]
close $f
puts [file size /tmp/file-test-14.1.txt]
file delete /tmp/file-test-14.1.txt
