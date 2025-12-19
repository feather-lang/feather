# Test: braces don't protect from substitution in subst
# Unlike normal Tcl parsing, braces in subst string are literal

set a 44
puts [subst {xyz {$a}}]
puts [subst {{$a}}]
