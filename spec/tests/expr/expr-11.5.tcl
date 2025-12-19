# Test: expr consecutive operators
catch {expr {1 + + 2}} result
puts $result
