# Test: dict merge - overlapping keys (last wins)
puts [dict merge {a 1 b 2} {b 20 c 3}]
puts [dict merge {a 1} {a 2} {a 3}]
