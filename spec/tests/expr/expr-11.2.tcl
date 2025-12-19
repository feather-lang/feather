# Test: expr incomplete ternary
catch {expr {1 ? 2}} result
puts $result
