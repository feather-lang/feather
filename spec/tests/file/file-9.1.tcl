# Test: file isdirectory on file
set f [open "/tmp/file-test-9.1.txt" w]
close $f
puts [file isdirectory /tmp/file-test-9.1.txt]
file delete /tmp/file-test-9.1.txt
