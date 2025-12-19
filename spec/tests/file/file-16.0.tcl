# Test: file mkdir creates directory
file mkdir /tmp/file-test-16.0-dir
puts [file isdirectory /tmp/file-test-16.0-dir]
file delete /tmp/file-test-16.0-dir
