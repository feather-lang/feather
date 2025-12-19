# Test: array set overwrites existing elements
# Merges with existing array

array set arr {a 1 b 2}
array set arr {b 20 c 3}
puts $arr(a)
puts $arr(b)
puts $arr(c)
