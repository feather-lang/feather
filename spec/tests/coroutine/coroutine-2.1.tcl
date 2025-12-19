# Test: error in coroutine propagates to caller
proc failing {} {
    yield
    yield "ok"
    error "something went wrong"
}
coroutine f failing
puts [f]
catch {f} err
puts "caught: $err"
