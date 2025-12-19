# Test: subst -nobackslashes flag
# Disable backslash substitution

puts [subst -nobackslashes {hello\tworld}]
puts [subst -nobackslashes {line1\nline2}]
puts [subst -nobackslashes {back\\slash}]
