# Test: file link creates symlink
set f [open "/tmp/file-test-32.0-target.txt" w]
close $f
file link -symbolic /tmp/file-test-32.0-link.txt /tmp/file-test-32.0-target.txt
puts [file type /tmp/file-test-32.0-link.txt]
file delete /tmp/file-test-32.0-link.txt /tmp/file-test-32.0-target.txt
