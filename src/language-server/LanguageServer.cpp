#include "LanguageServer.h"

int runLanguageServer() {
    cout << "Running language server" << endl;
    StdIOServer server;
    server.esc_event.wait();

    return 0;
}