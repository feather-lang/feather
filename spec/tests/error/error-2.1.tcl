# Test: error caught by catch
set result [catch {error "test error"} msg]
puts "result: $result"
puts "msg: $msg"
