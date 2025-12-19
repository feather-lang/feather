# Test: array exists on non-array variable
# Returns 0 for scalar variables

set scalar "hello"
puts [array exists scalar]
puts [array exists undefined]
