# NanoScript Autocomplete Notes

This document is a living record of autocomplete implementation decisions, phase by phase.

---

## Phase 1 — VS Code CompletionItemProvider (Static + Document-Scoped)

### Approach Chosen

There are three tiers of autocomplete complexity for a custom language in VS Code:

| Tier | Mechanism | Dynamic? | Cross-file? | Complexity |
|---|---|---|---|---|
| 1 | Snippets JSON in `package.json` | No | No | Trivial |
| 2 | `CompletionItemProvider` in `extension.js` | Yes (scans document) | No | Low |
| 3 | Language Server Protocol (LSP) | Yes | Yes | High |

Phase 1 uses **Tier 2** — a `CompletionItemProvider` registered directly in the VS Code extension, with no separate server process. This is sufficient for NanoScript's current scope: a single-file, no-module language where all symbols visible at any point are either keywords or variables assigned earlier in the same file.

### What Gets Completed

**Keywords** (`if`, `out`) are static. They are defined as `CompletionItem` objects with `insertText` set to a `SnippetString` so that selecting them expands into a full template with tab stops:

- `if` → `if (|) {\n\t\n}` (cursor placed in condition)
- `out` → `out |;` (cursor placed on expression)

**Variables** are discovered dynamically by scanning the full document text with a regex that matches the left-hand side of every assignment statement:

```
/^\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*=(?!=)/gm
```

The negative lookahead `(?!=)` prevents `==` from being matched as an assignment. Each unique name found is returned as a `Variable` completion item labelled `int64`.

### Why Not Tier 3 (LSP) Yet

A Language Server runs as a separate process and communicates via JSON-RPC. For NanoScript it would enable cross-file symbol lookup, real-time diagnostics (errors without compiling), and hover documentation — but NanoScript currently has no `import` or module system, so cross-file resolution has no meaning. The document-scoped provider delivers the same user-visible result with a fraction of the infrastructure.

LSP becomes the right choice when:
- NanoScript gains multi-file support or an `import` keyword
- We want inline error squiggles driven by the parser (without running the compiler)
- We want hover cards showing variable types or declaration lines

### Key VS Code API Details

**Registration** — the provider is registered with no trigger characters, so completions fire on the default `Ctrl+Space` / typing gesture. Adding trigger characters (e.g. `=`) would also fire completions immediately after an assignment operator, which is not useful here.

**Activation** — `"main": "./extension.js"` in `package.json` tells VS Code to load the extension as a Node.js module. VS Code infers the `onLanguage:nanoscript` activation event automatically from the `contributes.languages` entry, so no explicit `activationEvents` array is needed (VS Code 1.74+).

**Breakpoint gutter** — autocomplete (a `CompletionItemProvider`) and breakpoint gutter support (`"breakpoints": [{"language": "nanoscript"}]`) are completely independent contribution points. Both must be declared; neither implies the other.

**Symlink install** — the extension lives in the project repo and is symlinked into `~/.vscode/extensions/nanoscript.nanoscript-support-0.0.1`. Editing `extension.js` or `syntaxes/nanoscript.tmLanguage.json` takes effect after a VS Code **Reload Window** with no reinstall step.

### Limitations of the Current Implementation

- Variables assigned *after* the cursor are still offered as completions (the scan is whole-document, not position-aware). This is intentional — it mirrors how NanoScript itself works, where a variable exists for the lifetime of the program once assigned anywhere.
- No ranking or sorting — keywords and variables appear in the list in definition order. VS Code's fuzzy matcher handles narrowing as the user types.
- No signature help for `out` (showing the expected expression type). This would require a `SignatureHelpProvider`, deferred to a later phase.
