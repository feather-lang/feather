# Test: subst command with flags
# Disable specific substitution types

set x hello
puts [subst -nocommands {$x [string length foo]}]
puts [subst -novariables {$x [string length foo]}]
