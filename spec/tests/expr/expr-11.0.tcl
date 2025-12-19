# Test: expr undefined variable
catch {expr {$undefined_var + 1}} result
puts $result
