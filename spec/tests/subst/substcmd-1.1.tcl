# Test: subst command basic usage
# The subst command performs substitutions on a string

set x hello
puts [subst {$x world}]
puts [subst {length is [string length abc]}]
