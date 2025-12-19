# Test: lpop index out of range
set x {a b c}
catch {lpop x 10} msg
puts $msg
