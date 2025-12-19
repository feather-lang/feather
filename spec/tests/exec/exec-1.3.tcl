# Test: exec -keepnewline preserves trailing newline
# Trailing newline retained

set result [exec -keepnewline echo test]
puts "($result)"
