# Test: file owned on own file
set f [open "/tmp/file-test-26.0.txt" w]
close $f
puts [file owned /tmp/file-test-26.0.txt]
file delete /tmp/file-test-26.0.txt
