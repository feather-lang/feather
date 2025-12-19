# Test: calling finished coroutine is error
proc once {} {
    return "only once"
}
puts [coroutine o once]
catch {o} err
puts "error: [string match {*invalid command*} $err]"
