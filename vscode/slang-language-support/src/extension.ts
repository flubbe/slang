/*
 * slang-language-support - VS Code extension for Slang scripting.
 *
 * Configures the LSP client to communicate with the "slang lsp --server"
 * process. The server binary path is read from the "slang.languageServer.path"
 * setting.
 */

import * as path from "path";
import {
    LanguageClient,
    LanguageClientOptions,
    ServerOptions,
    TransportKind
} from "vscode-languageclient/node";
import { ExtensionContext, workspace } from "vscode";

let client: LanguageClient;

const clientId = "slang";
const clientName = "Slang Language Server";

function getServerPath(): string {
    const config = workspace.getConfiguration("slang");
    return config.get<string>("languageServer.path", "slang");
}

function getSearchPaths(): string[] {
    const config = workspace.getConfiguration("slang");
    const configuredPaths = config.get<string[]>("searchPaths", []);
    const workspacePaths = (workspace.workspaceFolders ?? []).map(
        folder => folder.uri.fsPath
    );
    return [...workspacePaths, ...configuredPaths];
}

function syncSearchPaths(): void {
    client.sendNotification("workspace/didChangeConfiguration", {
        settings: {
            slang: {
                searchPaths: getSearchPaths()
            }
        }
    });
}

export function activate(context: ExtensionContext) {
    const serverPath = getServerPath();

    const serverOptions: ServerOptions = {
        command: serverPath,
        args: ["lsp", "--server"],
        options: {
            env: process.env
        }
    };

    const clientOptions: LanguageClientOptions = {
        documentSelector: [
            { scheme: "file", language: "slang-scripting" },
            { scheme: "untitled", language: "slang-scripting" }
        ],
        synchronize: {
            fileEvents: [
                workspace.createFileSystemWatcher("**/*.sl")
            ]
        },
        initializationOptions: {
            capabilities: {
                textDocument: {
                    synchronization: {
                        didSave: true,
                        willSave: false
                    }
                }
            }
        }
    };

    client = new LanguageClient(
        clientId,
        clientName,
        serverOptions,
        clientOptions
    );

    void client.start().then(syncSearchPaths);
    context.subscriptions.push(
        workspace.onDidChangeConfiguration(event => {
            if (event.affectsConfiguration("slang.searchPaths")) {
                syncSearchPaths();
            }
        }),
        workspace.onDidChangeWorkspaceFolders(syncSearchPaths)
    );
    context.subscriptions.push(client);
}

export function deactivate(): Thenable<void> | undefined {
    if (!client) {
        return undefined;
    }
    return client.stop();
}
