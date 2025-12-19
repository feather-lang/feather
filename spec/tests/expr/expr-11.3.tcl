# Test: expr invalid string as number
catch {expr {"abc" + 1}} result
puts $result
