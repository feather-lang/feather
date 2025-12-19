# Test: exec non-zero exit code
# Command failure raises error

catch {exec false} result
puts [expr {$result ne ""}]
