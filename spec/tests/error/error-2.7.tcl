# Test: error with special characters in message
catch {error "error with \{braces\} and \$dollar"} msg
puts "msg: $msg"
