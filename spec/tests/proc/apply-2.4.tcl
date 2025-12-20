# Test: apply - 3-element lambda with namespace (global namespace)
puts [apply {{x} {expr {$x + 1}} ::} 10]
