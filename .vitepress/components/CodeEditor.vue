<template>
  <div class="code-editor" ref="editorContainer"></div>
</template>

<script setup>
import { ref, onMounted, watch, onBeforeUnmount } from 'vue'
import { EditorView, keymap, lineNumbers, highlightActiveLine, highlightActiveLineGutter } from '@codemirror/view'
import { EditorState, Compartment } from '@codemirror/state'
import { defaultKeymap, history, historyKeymap } from '@codemirror/commands'
import { bracketMatching, HighlightStyle, syntaxHighlighting } from '@codemirror/language'
import { javascript } from '@codemirror/lang-javascript'
import { gruvboxDark } from '@fsegurai/codemirror-theme-gruvbox-dark'
import { gruvboxLight } from '@fsegurai/codemirror-theme-gruvbox-light'
import { tags } from '@lezer/highlight'
import { tcl } from './tcl-lang.js'

const props = defineProps({
  modelValue: {
    type: String,
    default: ''
  },
  language: {
    type: String,
    default: 'tcl' // 'tcl' or 'javascript'
  },
  placeholder: {
    type: String,
    default: ''
  }
})

const emit = defineEmits(['update:modelValue', 'run'])

const editorContainer = ref(null)
let editorView = null
const themeCompartment = new Compartment()

// Custom comment colors
const darkCommentStyle = HighlightStyle.define([
  { tag: tags.comment, color: '#ffffff' }
])

const lightCommentStyle = HighlightStyle.define([
  { tag: tags.comment, color: '#000000' }
])

function isDarkMode() {
  if (typeof document === 'undefined') return false
  return document.documentElement.classList.contains('dark')
}

function getThemeExtensions(dark) {
  if (dark) {
    return [gruvboxDark, syntaxHighlighting(darkCommentStyle)]
  } else {
    return [gruvboxLight, syntaxHighlighting(lightCommentStyle)]
  }
}

onMounted(() => {
  const languageExtension = props.language === 'javascript' ? javascript() : tcl()

  const runKeymap = keymap.of([{
    key: 'Ctrl-Enter',
    mac: 'Cmd-Enter',
    run: () => {
      emit('run')
      return true
    }
  }])

  const updateListener = EditorView.updateListener.of((update) => {
    if (update.docChanged) {
      emit('update:modelValue', update.state.doc.toString())
    }
  })

  const dark = isDarkMode()

  const state = EditorState.create({
    doc: props.modelValue,
    extensions: [
      lineNumbers(),
      highlightActiveLine(),
      highlightActiveLineGutter(),
      history(),
      bracketMatching(),
      keymap.of([...defaultKeymap, ...historyKeymap]),
      runKeymap,
      languageExtension,
      themeCompartment.of(getThemeExtensions(dark)),
      updateListener,
      EditorView.theme({
        '&': {
          height: '100%',
          fontSize: '14px'
        },
        '.cm-scroller': {
          fontFamily: 'var(--vp-font-family-mono), monospace',
          overflow: 'auto'
        },
        '.cm-content': {
          padding: '12px 0'
        },
        '.cm-gutters': {
          backgroundColor: 'transparent',
          borderRight: 'none'
        },
        '.cm-lineNumbers .cm-gutterElement': {
          padding: '0 12px 0 8px',
          minWidth: '32px'
        }
      }),
      EditorView.lineWrapping
    ]
  })

  editorView = new EditorView({
    state,
    parent: editorContainer.value
  })

  // Watch for theme changes
  const observer = new MutationObserver(() => {
    const dark = isDarkMode()
    editorView.dispatch({
      effects: themeCompartment.reconfigure(getThemeExtensions(dark))
    })
  })

  observer.observe(document.documentElement, {
    attributes: true,
    attributeFilter: ['class']
  })

  onBeforeUnmount(() => {
    observer.disconnect()
  })
})

watch(() => props.modelValue, (newValue) => {
  if (editorView && newValue !== editorView.state.doc.toString()) {
    editorView.dispatch({
      changes: {
        from: 0,
        to: editorView.state.doc.length,
        insert: newValue
      }
    })
  }
})

onBeforeUnmount(() => {
  if (editorView) {
    editorView.destroy()
  }
})
</script>

<style scoped>
.code-editor {
  height: 280px;
  overflow: hidden;
}

.code-editor :deep(.cm-editor) {
  height: 100%;
}
</style>
