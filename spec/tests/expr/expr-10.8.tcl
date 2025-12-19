# Test: expr bitwise NOT on non-integer
catch {expr {~1.5}} result
puts $result
