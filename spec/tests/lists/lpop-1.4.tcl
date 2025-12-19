# Test: lpop nested index
set x {{a b c} {d e f} {g h i}}
set result [lpop x 1 1]
puts "$result $x"
