# Test: string insert - edge cases
puts [string insert "" 0 "hello"]
puts [string insert "hello" -1 "X"]
puts [string insert "hello" 100 "X"]
puts [string insert "abc" end-1 "X"]
