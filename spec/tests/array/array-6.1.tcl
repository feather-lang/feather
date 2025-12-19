# Test: array on scalar variable
# Most operations error on scalar

set scalar "hello"
catch {array names scalar} result
puts [expr {$result ne ""}]
