# Test: append creates variable if needed
# Variable is created with appended value

append newvar "first"
puts $newvar
append newvar " second"
puts $newvar
