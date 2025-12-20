# Test: error when renaming non-existent command
catch {rename nonexistent newname} err
puts $err
