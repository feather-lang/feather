# Test: file tempdir creates directory
set dir [file tempdir]
puts [file isdirectory $dir]
file delete $dir
