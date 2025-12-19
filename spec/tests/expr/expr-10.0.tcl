# Test: expr division by zero (integer)
catch {expr {1 / 0}} result
puts $result
