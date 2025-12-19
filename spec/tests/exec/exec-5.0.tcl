# Test: exec command not found
# Error when command doesn't exist

catch {exec nonexistent_command_xyz} result
puts [expr {$result ne ""}]
