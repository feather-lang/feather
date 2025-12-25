// Simple TCL language support for CodeMirror 6
import { LanguageSupport, StreamLanguage } from '@codemirror/language'

// TCL syntax highlighting using StreamLanguage
// Token names follow CodeMirror's legacy naming convention
const tclLanguage = StreamLanguage.define({
  name: 'tcl',
  
  startState() {
    return { commandStart: true }
  },

  token(stream, state) {
    // Skip whitespace (but track command boundaries)
    if (stream.eatSpace()) {
      return null
    }

    // Comments only at command start (after ; or at line start)
    if (state.commandStart && stream.match(/^#.*/)) {
      return 'comment'
    }

    // Semicolon marks end of command, next token is command start
    if (stream.eat(';')) {
      state.commandStart = true
      return null
    }

    // After any real token, we're no longer at command start
    state.commandStart = false

    // Double-quoted strings
    if (stream.peek() === '"') {
      stream.next()
      while (!stream.eol()) {
        const ch = stream.next()
        if (ch === '"') break
        if (ch === '\\') stream.next()
      }
      return 'string'
    }

    // Braced strings/blocks  
    if (stream.peek() === '{') {
      let depth = 1
      stream.next()
      while (!stream.eol() && depth > 0) {
        const ch = stream.next()
        if (ch === '{') depth++
        else if (ch === '}') depth--
      }
      return 'string'
    }

    // Variables $name or ${name}
    if (stream.match(/^\$\{[^}]+\}/)) {
      return 'variableName.special'
    }
    if (stream.match(/^\$[a-zA-Z_][a-zA-Z0-9_]*/)) {
      return 'variableName.special'
    }

    // Command substitution brackets
    if (stream.eat('[')) {
      state.commandStart = true  // Inside brackets is a new command
      return 'bracket'
    }
    if (stream.eat(']')) {
      return 'bracket'
    }

    // Numbers
    if (stream.match(/^-?\d+(\.\d+)?([eE][+-]?\d+)?/)) {
      return 'number'
    }

    // Keywords/built-in commands - must be followed by whitespace or end
    if (stream.match(/^(proc|if|else|elseif|while|for|foreach|switch|return|break|continue|set|unset|expr|puts|info|catch|try|throw|error|uplevel|upvar|global|variable|namespace|rename|incr|append|lappend|lindex|llength|lrange|lsort|lsearch|lreplace|linsert|list|dict|string|regexp|regsub|split|join|format|scan|clock|after|apply|tailcall|concat|subst|eval|canvas)\b/)) {
      return 'keyword'
    }

    // Operators
    if (stream.match(/^(==|!=|<=|>=|&&|\|\||[+\-*\/%<>=!&|])/)) {
      return 'operator'
    }

    // Identifiers (procedure/command names)
    if (stream.match(/^[a-zA-Z_][a-zA-Z0-9_]*/)) {
      return 'variableName.function'
    }

    // Any other character
    stream.next()
    return null
  },

  // Reset command start at beginning of each line
  blankLine(state) {
    state.commandStart = true
  }
})

export function tcl() {
  return new LanguageSupport(tclLanguage)
}
