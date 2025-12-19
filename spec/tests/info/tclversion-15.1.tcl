# Test: info tclversion - wrong # args
catch {info tclversion extra} result
puts $result
