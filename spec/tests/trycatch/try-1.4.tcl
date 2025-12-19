# Test: try with on ok handler
try {
    expr 1 + 2
} on ok {result} {
    puts "result: $result"
}
