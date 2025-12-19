# Test: file copy copies file
set f [open "/tmp/file-test-18.0-src.txt" w]
puts $f "content"
close $f
file copy /tmp/file-test-18.0-src.txt /tmp/file-test-18.0-dst.txt
puts [file exists /tmp/file-test-18.0-dst.txt]
file delete /tmp/file-test-18.0-src.txt /tmp/file-test-18.0-dst.txt
