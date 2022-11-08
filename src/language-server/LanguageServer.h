﻿#ifndef LANGUAGE_SERVER_H
#define LANGUAGE_SERVER_H

#include "../Package.h"
#include "../visitors/TokenVisitor.h"
#include "../visitors/TypeVisitor.h"

#include "LibLsp/JsonRpc/Condition.h"
#include "LibLsp/JsonRpc/Endpoint.h"
#include "LibLsp/JsonRpc/TcpServer.h"
#include "LibLsp/JsonRpc/stream.h"
#include "LibLsp/lsp/AbsolutePath.h"
#include "LibLsp/lsp/ProtocolJsonHandler.h"
#include "LibLsp/lsp/general/exit.h"
#include "LibLsp/lsp/general/initialize.h"
#include "LibLsp/lsp/general/initialized.h"
#include "LibLsp/lsp/general/lsTextDocumentClientCapabilities.h"
#include "LibLsp/lsp/lsPosition.h"
#include "LibLsp/lsp/textDocument/declaration_definition.h"
#include "LibLsp/lsp/textDocument/document_symbol.h"
#include "LibLsp/lsp/textDocument/publishDiagnostics.h"
#include "LibLsp/lsp/textDocument/resolveCompletionItem.h"
#include "LibLsp/lsp/textDocument/signature_help.h"
#include "LibLsp/lsp/textDocument/typeHierarchy.h"
#include "LibLsp/lsp/workspace/execute_command.h"
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/process.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <network/uri.hpp>

using namespace boost::asio::ip;
using namespace std;

extern BalancePackage *currentPackage;
std::string _address = "127.0.0.1";
std::string _port = "9333";

class DummyLog : public lsp::Log {
  public:
    void log(Level level, std::wstring &&msg) { std::wcerr << msg << std::endl; };
    void log(Level level, const std::wstring &msg) { std::wcerr << msg << std::endl; };
    void log(Level level, std::string &&msg) { std::cerr << msg << std::endl; };
    void log(Level level, const std::string &msg) { std::cerr << msg << std::endl; };
};

class LanguageServer {
public:
    virtual RemoteEndPoint * getRemoteEndPoint() = 0;
    virtual void run() = 0;
    virtual void stop() = 0;
    virtual DummyLog * getLogger() = 0;
};

class StdIOServer : public LanguageServer {
private:
    BalancePackage *package = nullptr;

public:
    StdIOServer() : remote_end_point_(protocol_json_handler, endpoint, _log) {
    }
    ~StdIOServer() {}

    RemoteEndPoint * getRemoteEndPoint() {
        return &this->remote_end_point_;
    }

    void run() {
        this->remote_end_point_.startProcessingMessages(input, output);
        this->esc_event.wait();
    }

    void stop() {
        this->esc_event.notify(std::make_unique<bool>(true));
    }

    DummyLog * getLogger() {
        return &_log;
    }

    struct ostream : lsp::base_ostream<std::ostream> {
        explicit ostream(std::ostream &_t) : base_ostream<std::ostream>(_t) {}

        std::string what() override { return {}; }
    };
    struct istream : lsp::base_istream<std::istream> {
        explicit istream(std::istream &_t) : base_istream<std::istream>(_t) {}

        std::string what() override { return {}; }
    };
    std::shared_ptr<lsp::ProtocolJsonHandler> protocol_json_handler = std::make_shared<lsp::ProtocolJsonHandler>();
    DummyLog _log;

    std::shared_ptr<ostream> output = std::make_shared<ostream>(std::cout);
    std::shared_ptr<istream> input = std::make_shared<istream>(std::cin);

    std::shared_ptr<GenericEndpoint> endpoint = std::make_shared<GenericEndpoint>(_log);
    RemoteEndPoint remote_end_point_;
    Condition<bool> esc_event;
};

class TCPServer : public LanguageServer {
  public:
    TCPServer() : server(_address, _port, protocol_json_handler, endpoint, _log) {
    }

    RemoteEndPoint * getRemoteEndPoint() {
        return &this->server.point;
    }

    void run() {
        this->server.run();
    }

    void stop() {
        this->server.stop();
    }

    DummyLog * getLogger() {
        return &_log;
    }

    ~TCPServer() { server.stop(); }
    std::shared_ptr<lsp::ProtocolJsonHandler> protocol_json_handler = std::make_shared<lsp::ProtocolJsonHandler>();
    DummyLog _log;

    std::shared_ptr<GenericEndpoint> endpoint = std::make_shared<GenericEndpoint>(_log);
    lsp::TcpServer server;
};

class BalanceLanguageServer {
private:
    DummyLog * logger;
    RemoteEndPoint * endpoint;

public:
    LanguageServer * server;
    BalanceLanguageServer(LanguageServer * server) {
        this->server = server;
        this->logger = server->getLogger();
        this->endpoint = server->getRemoteEndPoint();
    }

    void initialize() {
        this->endpoint->registerHandler([&](const td_initialize::request &req) -> lsp::ResponseOrError<td_initialize::response> {
            td_initialize::response rsp;
            rsp.id = req.id;

            try {
                this->initializePackage(*req.params.rootPath);
            } catch (const std::exception &exc) {
                this->logger->error("Error initializing package");
                // logger.error(exc.what());
            }

            SemanticTokensLegend legend;
            legend.tokenTypes = {"namespace", "type",     "class",  "enum",  "interface", "struct",   "typeParameter", "parameter", "variable", "property", "enumMember",
                                "event",     "function", "method", "macro", "keyword",   "modifier", "comment",       "string",    "number",   "regexp",   "operator"};

            SemanticTokensWithRegistrationOptions semanticTokensOptions;
            semanticTokensOptions.legend = legend;
            SemanticTokensServerFull semanticTokensServerFull;
            semanticTokensServerFull.delta = false;
            semanticTokensOptions.full = {true, semanticTokensServerFull};

            rsp.result.capabilities.semanticTokensProvider = semanticTokensOptions;

            // lsCompletionOptions completionOptions;
            // rsp.result.capabilities.completionProvider = completionOptions;
            return rsp;
        });

        this->endpoint->registerHandler([&](Notify_InitializedNotification::notify &notify) {});

        this->endpoint->registerHandler([=](Notify_Exit::notify &notify) {
            std::cout << notify.ToJson() << std::endl;
            this->server->stop();
        });

        this->endpoint->registerHandler([&](const td_semanticTokens_full::request &req) {
            td_semanticTokens_full::response rsp;
            rsp.id = req.id;

            SemanticTokens tokens;
            // try {
            //     std::string rootPath = req.params.textDocument.uri.GetAbsolutePath();
            //     std::string rootPathWithoutExtension = rootPath.substr(0, rootPath.find_last_of("."));

            //     logger->info(std::to_string(currentPackage->modules.size()));
            //     std::string modulePath = boost::replace_first_copy(rootPathWithoutExtension, currentPackage->packagePath + "/", "");

            //     logger->info("Before fetching module: " + modulePath);
            //     BalanceModule *bmodule = currentPackage->modules[modulePath];
            //     logger->info("After: " + bmodule->filePath);
            //     bmodule->generateASTFromPath();
            //     // Visit entire tree
            //     TokenVisitor tokenVisitor;
            //     tokenVisitor.visit(bmodule->tree);

            //     tokens.data = tokens.encodeTokens(tokenVisitor.tokens);

            //     // TODO: Clean this up
            //     Notify_TextDocumentPublishDiagnostics::notify notification;
            //     notification.params.uri = req.params.textDocument.uri;

            //     TypeVisitor typeVisitor;
            //     typeVisitor.visit(bmodule->tree);

            //     for (TypeError * typeError : bmodule->typeErrors) {
            //         logger->info("Type error: " + typeError->message);
            //         lsDiagnostic diagnosticItem;
            //         diagnosticItem.severity = lsDiagnosticSeverity::Error;
            //         diagnosticItem.message = typeError->message;
            //         diagnosticItem.source = "balance";
            //         lsRange range;
            //         lsPosition start;
            //         start.line = typeError->range->start->line;
            //         start.character = typeError->range->start->column;
            //         lsPosition end;
            //         end.line = typeError->range->end->line;
            //         end.character = typeError->range->end->column;
            //         range.start = start;
            //         range.end = end;
            //         diagnosticItem.range = range;
            //         notification.params.diagnostics.push_back(diagnosticItem);
            //     }
            //     this->endpoint->sendNotification(notification);

            // } catch (const std::exception &exc) {
            //     logger->error(exc.what());
            // }
            rsp.result = tokens;
            return rsp;
        });
        this->endpoint->registerHandler([&](const td_completion::request &req) {
            td_completion::response rsp;
            rsp.id = req.id;

            return rsp;
        });
    }

    void run() {
        this->server->run();
    }

    void initializePackage(std::string rootPath) {
        currentPackage = new BalancePackage(rootPath + "/package.json", "");
        currentPackage->logger = [&](std::string x) {
            this->logger->info(x);
        };
        currentPackage->isAnalyzeOnly = true;
        currentPackage->execute();
    }
};

int runLanguageServer() {
    bool runAsTcpServer = true;
    if (runAsTcpServer) {
        TCPServer server;
        BalanceLanguageServer languageServer(&server);
        languageServer.initialize();
        languageServer.run();
    } else {
        StdIOServer server;
        BalanceLanguageServer languageServer(&server);
        languageServer.initialize();
        languageServer.run();
    }

    return 0;
}

#endif