# Test: global creates variable that doesn't exist yet
proc test {} {
    global newvar
    set newvar "created"
}
test
puts "newvar = $newvar"
