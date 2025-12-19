# Test: array unset with pattern
# Removes matching elements only

array set vals {a1 1 a2 2 b1 3 b2 4}
puts [array size vals]
array unset vals a*
puts [array size vals]
puts [lsort [array names vals]]
