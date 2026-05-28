const path = require('path');
const vscode = require('vscode');
const { LanguageClient } = require('vscode-languageclient/node');

let client;

function activate(context) {
  const config = vscode.workspace.getConfiguration('cToolsLsp');
  let serverPath = config.get('serverPath', '');

  if (!serverPath) {
    serverPath = path.resolve(__dirname, '..', 'build', 'src', 'c_tools_lsp');
  }

  const serverOptions = {
    command: serverPath,
    args: [],
  };

  const clientOptions = {
    documentSelector: [{ scheme: 'file', language: 'c' }],
    trace: 'verbose',
    initializationOptions: {
      includePaths: vscode.workspace.getConfiguration('cToolsLsp').get('includePaths', [])
    }
  };

  client = new LanguageClient('cToolsLsp', 'C Tools LSP', serverOptions, clientOptions);
  context.subscriptions.push(client.start());

  const command = vscode.commands.registerCommand(
    'cToolsLsp.calculateComplexity',
    async () => {
      const editor = vscode.window.activeTextEditor;
      if (!editor) {
        vscode.window.showErrorMessage('No active editor');
        return;
      }
      const doc = editor.document;
      const pos = editor.selection.active;
      const wordRange = doc.getWordRangeAtPosition(pos);
      if (!wordRange) {
        vscode.window.showErrorMessage('No word at cursor position');
        return;
      }
      const funcName = doc.getText(wordRange);
      try {
        const result = await client.sendRequest(
          'textDocument/functionComplexity',
          {
            textDocument: { uri: doc.uri.toString() },
            functionName: funcName,
          }
        );
        if (result.complexity >= 0) {
          vscode.window.showInformationMessage(
            `Cyclomatic complexity of '${result.function_name || funcName}': ${result.complexity}`
          );
        } else {
          vscode.window.showErrorMessage(
            `Function '${funcName}' not found or could not be analyzed`
          );
        }
      } catch (err) {
        vscode.window.showErrorMessage(
          `Complexity check failed: ${err.message}`
        );
      }
    }
  );
  context.subscriptions.push(command);
}

function deactivate() {
  if (client) {
    return client.stop();
  }
}

module.exports = { activate, deactivate };
