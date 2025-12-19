# Test: expr invalid operator
catch {expr {1 @ 2}} result
puts $result
