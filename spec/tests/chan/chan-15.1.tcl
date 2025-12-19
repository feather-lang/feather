# Test: error on invalid subcommand
catch {chan invalid_subcommand} err
puts [expr {$err ne ""}]
