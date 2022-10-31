import * as vscode from 'vscode';
import { workspace } from 'vscode';

import {
    Executable,
    LanguageClient,
    LanguageClientOptions,
    ServerOptions,
    TransportKind
} from 'vscode-languageclient/node';

let client: LanguageClient;

class T {
    asdkmskm() {

    }
}

export async function activate(context: vscode.ExtensionContext) {
    const runExecutable: Executable = {
        // command: "/home/jeppe/workspace/balance/build/balance",
        command: "/home/jeppe/workspace/balance/language-server/LspCpp/_build/StdIOServerExample",
        args: []
    };

    const serverOptions: ServerOptions = {
        run: runExecutable,
        debug: runExecutable
    };

    // Options to control the language client
    const clientOptions: LanguageClientOptions = {
        // Register the server for plain text documents
        documentSelector: [{ scheme: 'file', language: 'plaintext' }],
        synchronize: {
            // Notify the server about file changes to '.clientrc files contained in the workspace
            fileEvents: workspace.createFileSystemWatcher('**/.clientrc')
        }
    };

    // Create the language client and start the client.
    client = new LanguageClient(
        'balance',
        'Balance Language Server',
        serverOptions,
        clientOptions
    );

    // Start the client. This will also launch the server
    client.start();
    console.log("After start");

    await client.onReady();
    console.log("On ready");
    client.sendRequest('textDocument/semanticTokens/full', '');

}

export function deactivate() { }
