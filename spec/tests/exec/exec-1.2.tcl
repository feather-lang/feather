# Test: exec strips trailing newline by default
# Output should not have trailing newline

set result [exec echo test]
puts "($result)"
