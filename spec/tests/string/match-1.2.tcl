# Test: string match - with -nocase
puts [string match -nocase "HELLO" "hello"]
puts [string match -nocase "h*" "HELLO"]
puts [string match -nocase "*WORLD*" "hello world"]
