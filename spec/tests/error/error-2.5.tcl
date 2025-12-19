# Test: errorInfo variable after error
catch {error "test error"}
puts "errorInfo exists: [info exists ::errorInfo]"
