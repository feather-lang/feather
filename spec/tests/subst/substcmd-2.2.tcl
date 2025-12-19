# Test: subst with multiple flags
# Combine multiple substitution disabling flags

set x hello
puts [subst -nocommands -novariables {$x [string length foo]}]
puts [subst -nobackslashes -novariables {$x\tworld}]
puts [subst -nobackslashes -nocommands {hello\t[cmd]}]
