# Test: array variable substitution
# Array elements accessed with parentheses

set arr(key) value
puts $arr(key)
set arr(x) hello
set arr(y) world
puts "$arr(x) $arr(y)"
