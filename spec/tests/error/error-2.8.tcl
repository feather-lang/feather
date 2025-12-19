# Test: nested proc error
proc inner {} {
    error "inner error"
}
proc outer {} {
    inner
}
catch {outer} msg
puts "msg: $msg"
