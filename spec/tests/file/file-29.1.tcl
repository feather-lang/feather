# Test: error on invalid subcommand
catch {file invalid_subcommand foo} err
puts [expr {$err ne ""}]
