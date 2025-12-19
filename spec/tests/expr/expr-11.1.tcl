# Test: expr shift on non-integer
catch {expr {1.5 << 2}} result
puts $result
