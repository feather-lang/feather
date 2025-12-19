# Test: expr exponentiation right-to-left associativity
puts [expr {2 ** 3 ** 2}]
puts [expr {(2 ** 3) ** 2}]
puts [expr {2 ** (3 ** 2)}]
