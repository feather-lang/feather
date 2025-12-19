# Test: lpop end-N
set x {a b c d e}
set result [lpop x end-1]
puts "$result $x"
