# Test: file stat type is file
set f [open "/tmp/file-test-23.1.txt" w]
close $f
set s [file stat /tmp/file-test-23.1.txt]
puts [dict get $s type]
file delete /tmp/file-test-23.1.txt
