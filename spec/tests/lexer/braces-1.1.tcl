# Test: brace quoting preserves literal content
# Braces prevent substitution and preserve whitespace

puts {hello world}
puts {hello   world}
puts {$notavar}
puts {[notacmd]}
