# Test: expr unbalanced parentheses (extra close)
catch {expr {1 + 2)}} result
puts $result
