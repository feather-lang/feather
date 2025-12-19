# Test: array with special characters in keys
# Keys can contain special chars

array set data [list "key with spaces" 1 "key\twith\ttabs" 2]
puts $data(key with spaces)
puts $data(key	with	tabs)
