# Test: expr unbalanced parentheses (missing close)
catch {expr {(1 + 2}} result
puts $result
