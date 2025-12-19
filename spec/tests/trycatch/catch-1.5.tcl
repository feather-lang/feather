# Test: catch in if condition
if {[catch {error "fail"}]} {
    puts "error caught"
} else {
    puts "no error"
}
