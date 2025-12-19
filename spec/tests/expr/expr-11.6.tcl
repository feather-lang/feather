# Test: expr leading operator (except unary)
catch {expr {* 2}} result
puts $result
