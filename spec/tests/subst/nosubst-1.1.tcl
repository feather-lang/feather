# Test: no substitution in braces
# Braces prevent all substitution

set x hello
puts {$x}
puts {[string length foo]}
puts {\n\t}
