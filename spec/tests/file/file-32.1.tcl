# Test: file readlink returns target
set f [open "/tmp/file-test-32.1-target.txt" w]
close $f
file link -symbolic /tmp/file-test-32.1-link.txt /tmp/file-test-32.1-target.txt
puts [file readlink /tmp/file-test-32.1-link.txt]
file delete /tmp/file-test-32.1-link.txt /tmp/file-test-32.1-target.txt
