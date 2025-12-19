# Test: file rename moves file
set f [open "/tmp/file-test-19.0-src.txt" w]
puts $f "content"
close $f
file rename /tmp/file-test-19.0-src.txt /tmp/file-test-19.0-dst.txt
puts [file exists /tmp/file-test-19.0-src.txt]
puts [file exists /tmp/file-test-19.0-dst.txt]
file delete /tmp/file-test-19.0-dst.txt
