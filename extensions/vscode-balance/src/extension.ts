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

export interface BalanceConfiguration {
    compilerPath: string;
    tcpServer: boolean;
}

let client: LanguageClient;

const config: vscode.WorkspaceConfiguration = vscode.workspace.getConfiguration('balance');

function getConfiguration(): BalanceConfiguration {
    return {
        compilerPath: config.get<string>('compilerPath', "/usr/bin/balance"),
        tcpServer: config.get<boolean>('tcpServer', false)
    };
}

export async function activate(context: vscode.ExtensionContext) {
    const configuration = getConfiguration();

    let runExecutable: Executable = {
        command: configuration.compilerPath,
        args: [ "--language-server" ]
    };

    let serverOptions: ServerOptions = {
        run: runExecutable,
        debug: runExecutable
    };

    if (configuration.tcpServer) {
        serverOptions = () => {
            const socket = net.connect({ port: 9333 });
            const result: StreamInfo = {
                writer: socket,
                reader: socket
            };
            return Promise.resolve(result);
        };
    }

    // Options to control the language client
    const clientOptions: LanguageClientOptions = {
        // Register the server for plain text documents
        // documentSelector: [{ scheme: 'file', language: 'balance', pattern: '*.bl' }],
        documentSelector: [ { pattern: "**/*.bl" } ],
        synchronize: {
            // Notify the server about file changes to '.clientrc files contained in the workspace
            fileEvents: workspace.createFileSystemWatcher('**/.clientrc')
        }
    };

    client = new LanguageClient('balance', 'Balance Language Server', serverOptions, clientOptions);
    client.start();
    await client.onReady();

    // vscode.workspace.onDidSaveTextDocument(async (document: vscode.TextDocument) => {
    //     if (vscode.window.activeTextEditor !== undefined && vscode.window.activeTextEditor.document === document) {
    //         const result = await client.sendRequest<SemanticTokens>('textDocument/semanticTokens/full', {
    //             textDocument: {
    //                 uri: "file://" + document.uri.fsPath
    //             }
    //         });
    //         const t = 123;
    //     }
    // });

    // vscode.workspace.onDidChangeTextDocument(async (event: vscode.TextDocumentChangeEvent) => {
    //     client.sendRequest('textDocument/semanticTokens/full', {
    //         textDocument: {
    //             uri: "file://" + event.document.uri.fsPath
    //         }
    //     });
    // });

    // vscode.workspace.onDidOpenTextDocument(async (document: vscode.TextDocument) => {
    //     if (document.uri.scheme === "file") {
    //         client.sendRequest('textDocument/semanticTokens/full', {
    //             textDocument: {
    //                 uri: "file://" + document.uri.fsPath
    //             }
    //         });
    //     }
    // });
}

export function deactivate() { }
