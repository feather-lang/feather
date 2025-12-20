# Test: rename returns empty string
proc foo {} {
    return "hello"
}
set result [rename foo bar]
puts "result: '$result'"
