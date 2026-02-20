'use strict';
const vscode = require('vscode');

// ── Static keyword completions ─────────────────────────────────────────────

const KEYWORD_COMPLETIONS = [
    {
        label:      'if',
        kind:       vscode.CompletionItemKind.Keyword,
        detail:     'Conditional statement',
        // Expands to a full if-block with tab stops
        insertText: new vscode.SnippetString('if (${1:condition}) {\n\t$0\n}'),
    },
    {
        label:      'out',
        kind:       vscode.CompletionItemKind.Keyword,
        detail:     'Print int64 to stdout followed by newline',
        insertText: new vscode.SnippetString('out ${1:expr};'),
    },
];

// ── Dynamic variable completions ───────────────────────────────────────────
// Scans the document for every left-hand side of an assignment ( name = )
// and surfaces them as Variable completions.

function variablesInDocument(document) {
    const seen   = new Set();
    const items  = [];
    const text   = document.getText();
    // Match: optional whitespace, identifier, optional whitespace, '='
    // Negative lookahead on '=' prevents matching '=='
    const re = /^\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*=(?!=)/gm;
    let m;
    while ((m = re.exec(text)) !== null) {
        const name = m[1];
        if (!seen.has(name)) {
            seen.add(name);
            const item    = new vscode.CompletionItem(name, vscode.CompletionItemKind.Variable);
            item.detail   = 'int64';
            item.documentation = new vscode.MarkdownString(
                `NanoScript variable \`${name}\` (int64)`);
            items.push(item);
        }
    }
    return items;
}

// ── Extension lifecycle ────────────────────────────────────────────────────

function activate(context) {
    const provider = vscode.languages.registerCompletionItemProvider(
        'nanoscript',
        {
            provideCompletionItems(document /*, position, token, context */) {
                // Build keyword items fresh each call (SnippetString must not be shared)
                const keywords = KEYWORD_COMPLETIONS.map(k => {
                    const item        = new vscode.CompletionItem(k.label, k.kind);
                    item.detail       = k.detail;
                    item.insertText   = k.insertText;
                    return item;
                });

                return [...keywords, ...variablesInDocument(document)];
            }
        }
        // No trigger characters — completions fire on the default Ctrl+Space / typing
    );

    context.subscriptions.push(provider);
}

function deactivate() {}

module.exports = { activate, deactivate };
