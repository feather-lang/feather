# Test: apply - accessing global variable with global command
set globalvar 42
puts [apply {{} {global globalvar; set globalvar}}]
