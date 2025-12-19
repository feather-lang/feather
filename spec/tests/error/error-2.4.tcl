# Test: error in proc
proc fail {} {
    error "proc failed"
}
catch {fail} msg
puts "msg: $msg"
