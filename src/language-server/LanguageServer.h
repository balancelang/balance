#ifndef LANGUAGE_SERVER_H
#define LANGUAGE_SERVER_H

#include "../Package.h"
#include "../visitors/TokenVisitor.h"

#include "LibLsp/lsp/textDocument/signature_help.h"
#include "LibLsp/lsp/AbsolutePath.h"
#include "LibLsp/JsonRpc/Condition.h"
#include "LibLsp/lsp/general/exit.h"
#include "LibLsp/lsp/general/initialized.h"
#include "LibLsp/lsp/textDocument/declaration_definition.h"
#include <boost/program_options.hpp>
#include "LibLsp/lsp/textDocument/signature_help.h"
#include "LibLsp/lsp/general/initialize.h"
#include "LibLsp/lsp/ProtocolJsonHandler.h"
#include "LibLsp/lsp/textDocument/typeHierarchy.h"
#include "LibLsp/lsp/AbsolutePath.h"
#include "LibLsp/lsp/textDocument/resolveCompletionItem.h"

#include "LibLsp/JsonRpc/Endpoint.h"
#include "LibLsp/JsonRpc/stream.h"
#include "LibLsp/JsonRpc/TcpServer.h"
#include "LibLsp/lsp/textDocument/document_symbol.h"
#include "LibLsp/lsp/workspace/execute_command.h"
#include <boost/process.hpp>
#include <boost/filesystem.hpp>
#include <boost/asio.hpp>
#include <iostream>

using namespace boost::asio::ip;
using namespace std;

class DummyLog : public lsp::Log {
  public:
    void log(Level level, std::wstring &&msg) { std::wcerr << msg << std::endl; };
    void log(Level level, const std::wstring &msg) { std::wcerr << msg << std::endl; };
    void log(Level level, std::string &&msg) { std::cerr << msg << std::endl; };
    void log(Level level, const std::string &msg) { std::cerr << msg << std::endl; };
};

class StdIOServer {
  public:
    StdIOServer() : remote_end_point_(protocol_json_handler, endpoint, _log) {
        remote_end_point_.registerHandler([&](const td_initialize::request &req) {
            td_initialize::response rsp;
            rsp.id = req.id;

            SemanticTokensLegend legend;
            legend.tokenTypes = {
            	"namespace",
                "type",
                "class",
                "enum",
                "interface",
                "struct",
                "typeParameter",
                "parameter",
                "variable",
                "property",
                "enumMember",
                "event",
                "function",
                "method",
                "macro",
                "keyword",
                "modifier",
                "comment",
                "string",
                "number",
                "regexp",
                "operator"
            };

            SemanticTokensWithRegistrationOptions semanticTokensOptions;
            semanticTokensOptions.legend = legend;
            SemanticTokensServerFull semanticTokensServerFull;
            semanticTokensServerFull.delta = false;
            semanticTokensOptions.full = {true, semanticTokensServerFull};

            rsp.result.capabilities.semanticTokensProvider = semanticTokensOptions;
            return rsp;
        });

        remote_end_point_.registerHandler([&](Notify_InitializedNotification::notify &notify) { });

        // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#semanticTokens_fullRequest
        remote_end_point_.registerHandler([&](const td_semanticTokens_full::request &req) {
            td_semanticTokens_full::response rsp;
            rsp.id = req.id;

            try {
                std::string rootPath = req.params.textDocument.uri.GetAbsolutePath();
                std::string rootPathWithoutExtension = rootPath.substr(0, rootPath.find_last_of("."));

                BalanceModule * bmodule = new BalanceModule(rootPathWithoutExtension, true);
                bmodule->generateASTFromPath(rootPath);

                // Visit entire tree
                TokenVisitor visitor;
                visitor.visit(bmodule->tree);

                SemanticTokens tokens;
                tokens.data = tokens.encodeTokens(visitor.tokens);

                rsp.result = tokens;
            } catch (const std::exception &exc) {
                _log.error(exc.what());
            }

            return rsp;
        });

        remote_end_point_.registerHandler([&](Notify_Exit::notify &notify) {
            remote_end_point_.stop();
            esc_event.notify(std::make_unique<bool>(true));
        });

        remote_end_point_.registerHandler([&](const td_definition::request &req, const CancelMonitor &monitor) {
            td_definition::response rsp;
            rsp.result.first = std::vector<lsLocation>();
            if (monitor && monitor()) {
                _log.info("textDocument/definition request had been cancel.");
            }
            return rsp;
        });

        remote_end_point_.startProcessingMessages(input, output);
    }
    ~StdIOServer() {}

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

int runLanguageServer();


#endif