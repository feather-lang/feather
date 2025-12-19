# Test: array with empty key
# Empty string is valid key

array set data {"" empty a 1}
puts [array size data]
puts $data()
puts $data(a)
