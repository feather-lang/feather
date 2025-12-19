# Test: append adds to variable
# Concatenates values to existing variable

set str "hello"
append str " world"
puts $str
append str "!" "!"
puts $str
