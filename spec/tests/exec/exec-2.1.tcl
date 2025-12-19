# Test: exec with multiple pipes
# Chain multiple commands

puts [exec echo "3 1 2" | tr " " "\n" | sort]
