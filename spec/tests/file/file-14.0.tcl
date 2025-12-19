# Test: file size
set f [open "/tmp/file-test-14.0.txt" w]
puts -nonewline $f "hello"
close $f
puts [file size /tmp/file-test-14.0.txt]
file delete /tmp/file-test-14.0.txt
