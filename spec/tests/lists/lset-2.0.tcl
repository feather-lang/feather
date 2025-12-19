# Test: lset index out of range
set x {a b c}
catch {lset x 5 foo} msg
puts $msg
