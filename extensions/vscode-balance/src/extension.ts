import * as vscode from 'vscode';
import * as net from 'net';
import { workspace } from 'vscode';

import {
    Executable,
    LanguageClient,
    LanguageClientOptions,
    ServerOptions,
    StreamInfo,
} from 'vscode-languageclient/node';

let client: LanguageClient;

export async function activate(context: vscode.ExtensionContext) {
    const runExecutable: Executable = {
        command: "/home/jeppe/workspace/balance/_build/balance",
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

    // vscode.workspace.onDidChangeTextDocument(async (event: vscode.TextDocumentChangeEvent) => {
    //     client.sendRequest('textDocument/semanticTokens/full', {
    //         textDocument: {
    //             uri: "file://" + event.document.uri.fsPath
    //         }
    //     });
    // });

    vscode.workspace.onDidOpenTextDocument(async (document: vscode.TextDocument) => {
        if (document.uri.scheme === "file") {
            client.sendRequest('textDocument/semanticTokens/full', {
                textDocument: {
                    uri: "file://" + document.uri.fsPath
                }
            });
        }
    });
}

export function deactivate() { }
