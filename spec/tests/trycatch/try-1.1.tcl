# Test: try with on error handler
try {
    error "oops"
} on error {msg} {
    puts "caught: $msg"
}
