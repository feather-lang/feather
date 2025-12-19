# Test: regexp with multiple submatches
regexp {(\w+)@(\w+)\.(\w+)} "user@example.com" match user domain tld
puts $match
puts $user
puts $domain
puts $tld
