# Test: yield with list value
proc listGen {} {
    yield
    yield [list a b c]
    yield [list 1 2 3]
    return [list x y z]
}
coroutine lg listGen
puts [lg]
puts [lg]
puts [lg]
