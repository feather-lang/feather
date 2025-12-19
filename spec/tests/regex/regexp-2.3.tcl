# Test: regexp with special regex characters
puts [regexp {\d+} "abc123def"]
puts [regexp {\s+} "hello world"]
puts [regexp {\.} "file.txt"]
