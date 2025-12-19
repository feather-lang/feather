# Test: try with error and finally
try {
    error "fail"
} on error {msg} {
    puts "error: $msg"
} finally {
    puts "cleanup"
}
