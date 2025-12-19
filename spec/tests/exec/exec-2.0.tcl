# Test: exec with pipeline
# Pipe output between commands

puts [exec echo "hello world" | tr a-z A-Z]
