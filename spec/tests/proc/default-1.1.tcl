# Test: procedure with default argument
proc greet {{name World}} {
    puts "Hello, $name!"
}
greet
greet Alice
