# Test: expr ni (not in) operator
puts [expr {"a" ni {a b c}}]
puts [expr {"d" ni {a b c}}]
puts [expr {"x" ni [list x y z]}]
