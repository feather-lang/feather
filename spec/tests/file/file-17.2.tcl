# Test: file delete -force removes directory
file mkdir /tmp/file-test-17.2-dir
set f [open "/tmp/file-test-17.2-dir/file.txt" w]
close $f
file delete -force /tmp/file-test-17.2-dir
puts [file exists /tmp/file-test-17.2-dir]
