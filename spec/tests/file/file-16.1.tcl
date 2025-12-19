# Test: file mkdir creates nested directories
file mkdir /tmp/file-test-16.1-dir/nested/deep
puts [file isdirectory /tmp/file-test-16.1-dir/nested/deep]
file delete -force /tmp/file-test-16.1-dir
