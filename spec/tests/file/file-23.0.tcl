# Test: file stat returns dict with expected keys
set f [open "/tmp/file-test-23.0.txt" w]
puts $f "content"
close $f
set s [file stat /tmp/file-test-23.0.txt]
puts [dict exists $s size]
puts [dict exists $s mtime]
puts [dict exists $s type]
file delete /tmp/file-test-23.0.txt
