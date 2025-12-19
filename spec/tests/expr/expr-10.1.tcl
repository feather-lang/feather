# Test: expr modulo by zero
catch {expr {10 % 0}} result
puts $result
