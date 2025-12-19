# Test: procedure with required args and varargs
proc printf {fmt args} {
    puts "format: $fmt"
    puts "values: $args"
}
printf "%s %d"
printf "%s %d" hello 42
