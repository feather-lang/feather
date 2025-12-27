# tailcall

Replace the current procedure frame with a call to another command.

## Syntax

```tcl
tailcall command ?arg ...?
```

## Parameters

- **command**: The command to call
- **arg ...**: Arguments to pass to the command

## Description

`tailcall` replaces the current procedure's stack frame with a call to the specified command. This enables tail call optimization, preventing stack overflow in deeply recursive procedures.

The command behaves as if the current procedure returned and then the specified command was called. This is more efficient than a regular call followed by return.

## Examples

<script setup>
import WasmPlayground from '../../.vitepress/components/WasmPlayground.vue'

const tailRecursiveFactorial = `proc factorial {n {acc 1}} {
    if {$n <= 1} {
        return $acc
    }
    tailcall factorial [expr {$n - 1}] [expr {$n * $acc}]
}

puts "10! = [factorial 10]"`

const tailRecursiveSum = `proc sum_list {lst {acc 0}} {
    if {[llength $lst] == 0} {
        return $acc
    }
    set first [lindex $lst 0]
    set rest [lrange $lst 1 end]
    tailcall sum_list $rest [expr {$acc + $first}]
}

puts "Sum: [sum_list {1 2 3 4 5}]"`

const stateMachineWithTailcall = `proc state_a {count} {
    puts "State A, count=$count"
    if {$count >= 3} { return "done" }
    tailcall state_b [expr {$count + 1}]
}

proc state_b {count} {
    puts "State B, count=$count"
    tailcall state_a $count
}

state_a 0`

const delegationPattern = `proc handler {type args} {
    switch $type {
        add { tailcall handle_add {*}$args }
        mul { tailcall handle_mul {*}$args }
        default { return "unknown type" }
    }
}

proc handle_add {a b} { expr {$a + $b} }
proc handle_mul {a b} { expr {$a * $b} }

puts "Add: [handler add 3 4]"
puts "Mul: [handler mul 3 4]"`
</script>

### Tail-recursive factorial

<WasmPlayground :tcl="tailRecursiveFactorial" />

### Tail-recursive sum

<WasmPlayground :tcl="tailRecursiveSum" />

### State machine with tailcall

<WasmPlayground :tcl="stateMachineWithTailcall" />

### Delegation pattern

<WasmPlayground :tcl="delegationPattern" />

## See Also

- [proc](./proc) - Define procedures
- [return](./return) - Return from procedure
- [apply](./apply) - Anonymous functions

