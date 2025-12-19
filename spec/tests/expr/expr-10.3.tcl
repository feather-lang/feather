# Test: expr missing operand
catch {expr {1 +}} result
puts $result
