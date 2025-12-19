# Test: nested braces
# Braces can be nested and preserve internal braces

puts {a{b}c}
puts {{nested}}
puts {a{b{c}d}e}
puts {open { close }}
