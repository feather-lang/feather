# Test: lset no index (replace entire variable)
set x {a b c}
puts [lset x {} {x y z}]
