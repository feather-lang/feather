# Test: procedure with args (varargs)
proc printall {args} {
    puts "args: $args"
}
printall
printall one
printall one two three
