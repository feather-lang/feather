# Test: error with all three arguments
catch {error "test message" "custom info" {CUSTOM CODE}} msg
puts "msg: $msg"
