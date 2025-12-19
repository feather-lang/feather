# Test: try body succeeds, no handler matched
try {
    puts "success"
} on error {msg} {
    puts "should not print"
}
