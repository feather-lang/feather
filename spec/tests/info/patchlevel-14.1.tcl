# Test: info patchlevel - wrong # args
catch {info patchlevel extra} result
puts $result
