# Test: dict create - special characters in keys/values
puts [dict create "key with spaces" "value with spaces"]
puts [dict create {key{brace}} {val{brace}}]
puts [dict create "a\tb" "c\nd"]
