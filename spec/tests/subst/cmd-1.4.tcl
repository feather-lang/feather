# Test: command substitution with variable
# Variables work inside command substitution

set x hello
puts [string length $x]
puts [string toupper $x]
