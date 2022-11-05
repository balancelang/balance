import * as vscode from 'vscode';
import { workspace } from 'vscode';

import {
    Executable,
    LanguageClient,
    LanguageClientOptions,
    ServerOptions,
} from 'vscode-languageclient/node';

let client: LanguageClient;

export async function activate(context: vscode.ExtensionContext) {
    const runExecutable: Executable = {
        command: "/home/jeppe/workspace/balance/build/balance",
        args: [ "--language-server" ]
    };

    const serverOptions: ServerOptions = {
        run: runExecutable,
        debug: runExecutable
    };

    // Options to control the language client
    const clientOptions: LanguageClientOptions = {
        // Register the server for plain text documents
        documentSelector: [{ scheme: 'file', language: 'balance' }],
        synchronize: {
            // Notify the server about file changes to '.clientrc files contained in the workspace
            fileEvents: workspace.createFileSystemWatcher('**/.clientrc')
        }
    };

    client = new LanguageClient('balance', 'Balance Language Server', serverOptions, clientOptions);
    client.start();
    await client.onReady();

    vscode.workspace.onDidSaveTextDocument(async (document: vscode.TextDocument) => {
        if (vscode.window.activeTextEditor !== undefined && vscode.window.activeTextEditor.document === document) {
            client.sendRequest('textDocument/semanticTokens/full', {
                textDocument: {
                    uri: "file://" + document.uri.fsPath
                }
            });
        }
    });

    vscode.workspace.onDidChangeTextDocument(event => {
        // client.sendRequest('textDocument/semanticTokens/full');
    });

    vscode.workspace.onDidOpenTextDocument(async event => {
        await client.sendRequest('textDocument/semanticTokens/full').catch(err => {
            console.log(err);
        });
    });
}

export function deactivate() { }
