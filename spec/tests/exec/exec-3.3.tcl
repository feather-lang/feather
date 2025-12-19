# Test: exec -ignorestderr
# Ignore stderr output

set result [exec -ignorestderr sh -c "echo stdout; echo stderr >&2"]
puts $result
