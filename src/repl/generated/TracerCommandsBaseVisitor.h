
// Generated from src/repl/TracerCommands.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"
#include "TracerCommandsVisitor.h"


/**
 * This class provides an empty implementation of TracerCommandsVisitor, which can be
 * extended to create a visitor which only needs to handle a subset of the available methods.
 */
class  TracerCommandsBaseVisitor : public TracerCommandsVisitor {
public:

  virtual std::any visitLine(TracerCommandsParser::LineContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCommand(TracerCommandsParser::CommandContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitServer(TracerCommandsParser::ServerContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitServerOption(TracerCommandsParser::ServerOptionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAttach(TracerCommandsParser::AttachContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitProcesses(TracerCommandsParser::ProcessesContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSessions(TracerCommandsParser::SessionsContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitUse(TracerCommandsParser::UseContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStatus(TracerCommandsParser::StatusContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitDetach(TracerCommandsParser::DetachContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitModules(TracerCommandsParser::ModulesContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSymbols(TracerCommandsParser::SymbolsContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSymbolOption(TracerCommandsParser::SymbolOptionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitProbe(TracerCommandsParser::ProbeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitResolveOption(TracerCommandsParser::ResolveOptionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitDeployOption(TracerCommandsParser::DeployOptionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitInterceptor(TracerCommandsParser::InterceptorContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitEvents(TracerCommandsParser::EventsContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCollect(TracerCommandsParser::CollectContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitRecord(TracerCommandsParser::RecordContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStreamOption(TracerCommandsParser::StreamOptionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCpp(TracerCommandsParser::CppContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitComplete(TracerCommandsParser::CompleteContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitHelp(TracerCommandsParser::HelpContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitQuit(TracerCommandsParser::QuitContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitValue(TracerCommandsParser::ValueContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitKeyword(TracerCommandsParser::KeywordContext *ctx) override {
    return visitChildren(ctx);
  }


};

