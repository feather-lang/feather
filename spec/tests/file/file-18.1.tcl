# Test: file copy preserves content
set f [open "/tmp/file-test-18.1-src.txt" w]
puts -nonewline $f "hello world"
close $f
file copy /tmp/file-test-18.1-src.txt /tmp/file-test-18.1-dst.txt
set f [open "/tmp/file-test-18.1-dst.txt" r]
puts [read $f]
close $f
file delete /tmp/file-test-18.1-src.txt /tmp/file-test-18.1-dst.txt
