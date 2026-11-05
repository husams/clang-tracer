
// Generated from src/repl/TracerCommands.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"




class  TracerCommandsParser : public antlr4::Parser {
public:
  enum {
    T__0 = 1, T__1 = 2, T__2 = 3, T__3 = 4, T__4 = 5, T__5 = 6, T__6 = 7,
    T__7 = 8, T__8 = 9, T__9 = 10, T__10 = 11, T__11 = 12, T__12 = 13, T__13 = 14,
    T__14 = 15, T__15 = 16, T__16 = 17, T__17 = 18, T__18 = 19, T__19 = 20,
    T__20 = 21, T__21 = 22, T__22 = 23, T__23 = 24, T__24 = 25, T__25 = 26,
    T__26 = 27, T__27 = 28, T__28 = 29, T__29 = 30, T__30 = 31, T__31 = 32,
    T__32 = 33, T__33 = 34, T__34 = 35, T__35 = 36, T__36 = 37, T__37 = 38,
    T__38 = 39, T__39 = 40, T__40 = 41, T__41 = 42, T__42 = 43, T__43 = 44,
    T__44 = 45, T__45 = 46, T__46 = 47, T__47 = 48, T__48 = 49, T__49 = 50,
    T__50 = 51, T__51 = 52, T__52 = 53, T__53 = 54, T__54 = 55, T__55 = 56,
    T__56 = 57, T__57 = 58, T__58 = 59, T__59 = 60, T__60 = 61, T__61 = 62,
    T__62 = 63, T__63 = 64, T__64 = 65, T__65 = 66, T__66 = 67, T__67 = 68,
    T__68 = 69, T__69 = 70, T__70 = 71, T__71 = 72, UINT = 73, STRING = 74,
    WORD = 75, WS = 76
  };

  enum {
    RuleLine = 0, RuleCommand = 1, RuleServer = 2, RuleServerOption = 3,
    RuleAttach = 4, RuleProcesses = 5, RuleSessions = 6, RuleUse = 7, RuleStatus = 8,
    RuleDetach = 9, RuleModules = 10, RuleSymbols = 11, RuleSymbolOption = 12,
    RuleProbe = 13, RuleResolveOption = 14, RuleDeployOption = 15, RuleInterceptor = 16,
    RuleEvents = 17, RuleCollect = 18, RuleRecord = 19, RuleStreamOption = 20,
    RuleCpp = 21, RuleComplete = 22, RuleHelp = 23, RuleQuit = 24, RuleValue = 25,
    RuleKeyword = 26
  };

  explicit TracerCommandsParser(antlr4::TokenStream *input);

  TracerCommandsParser(antlr4::TokenStream *input, const antlr4::atn::ParserATNSimulatorOptions &options);

  ~TracerCommandsParser() override;

  std::string getGrammarFileName() const override;

  const antlr4::atn::ATN& getATN() const override;

  const std::vector<std::string>& getRuleNames() const override;

  const antlr4::dfa::Vocabulary& getVocabulary() const override;

  antlr4::atn::SerializedATNView getSerializedATN() const override;


  class LineContext;
  class CommandContext;
  class ServerContext;
  class ServerOptionContext;
  class AttachContext;
  class ProcessesContext;
  class SessionsContext;
  class UseContext;
  class StatusContext;
  class DetachContext;
  class ModulesContext;
  class SymbolsContext;
  class SymbolOptionContext;
  class ProbeContext;
  class ResolveOptionContext;
  class DeployOptionContext;
  class InterceptorContext;
  class EventsContext;
  class CollectContext;
  class RecordContext;
  class StreamOptionContext;
  class CppContext;
  class CompleteContext;
  class HelpContext;
  class QuitContext;
  class ValueContext;
  class KeywordContext;

  class  LineContext : public antlr4::ParserRuleContext {
  public:
    LineContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    CommandContext *command();
    antlr4::tree::TerminalNode *EOF();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  LineContext* line();

  class  CommandContext : public antlr4::ParserRuleContext {
  public:
    CommandContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    ServerContext *server();
    AttachContext *attach();
    ProcessesContext *processes();
    SessionsContext *sessions();
    UseContext *use();
    StatusContext *status();
    DetachContext *detach();
    ModulesContext *modules();
    SymbolsContext *symbols();
    ProbeContext *probe();
    InterceptorContext *interceptor();
    EventsContext *events();
    CollectContext *collect();
    RecordContext *record();
    CppContext *cpp();
    CompleteContext *complete();
    HelpContext *help();
    QuitContext *quit();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  CommandContext* command();

  class  ServerContext : public antlr4::ParserRuleContext {
  public:
    ServerContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    ValueContext *value();
    std::vector<ServerOptionContext *> serverOption();
    ServerOptionContext* serverOption(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  ServerContext* server();

  class  ServerOptionContext : public antlr4::ParserRuleContext {
  public:
    ServerOptionContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    ValueContext *value();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  ServerOptionContext* serverOption();

  class  AttachContext : public antlr4::ParserRuleContext {
  public:
    AttachContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *UINT();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  AttachContext* attach();

  class  ProcessesContext : public antlr4::ParserRuleContext {
  public:
    ProcessesContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    ValueContext *value();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  ProcessesContext* processes();

  class  SessionsContext : public antlr4::ParserRuleContext {
  public:
    SessionsContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  SessionsContext* sessions();

  class  UseContext : public antlr4::ParserRuleContext {
  public:
    UseContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    ValueContext *value();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  UseContext* use();

  class  StatusContext : public antlr4::ParserRuleContext {
  public:
    StatusContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  StatusContext* status();

  class  DetachContext : public antlr4::ParserRuleContext {
  public:
    DetachContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    ValueContext *value();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  DetachContext* detach();

  class  ModulesContext : public antlr4::ParserRuleContext {
  public:
    ModulesContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  ModulesContext* modules();

  class  SymbolsContext : public antlr4::ParserRuleContext {
  public:
    SymbolsContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<SymbolOptionContext *> symbolOption();
    SymbolOptionContext* symbolOption(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  SymbolsContext* symbols();

  class  SymbolOptionContext : public antlr4::ParserRuleContext {
  public:
    SymbolOptionContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    ValueContext *value();
    antlr4::tree::TerminalNode *UINT();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  SymbolOptionContext* symbolOption();

  class  ProbeContext : public antlr4::ParserRuleContext {
  public:
    ProbeContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<ValueContext *> value();
    ValueContext* value(size_t i);
    antlr4::tree::TerminalNode *UINT();
    std::vector<ResolveOptionContext *> resolveOption();
    ResolveOptionContext* resolveOption(size_t i);
    std::vector<DeployOptionContext *> deployOption();
    DeployOptionContext* deployOption(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  ProbeContext* probe();

  class  ResolveOptionContext : public antlr4::ParserRuleContext {
  public:
    ResolveOptionContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    ValueContext *value();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  ResolveOptionContext* resolveOption();

  class  DeployOptionContext : public antlr4::ParserRuleContext {
  public:
    DeployOptionContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    ResolveOptionContext *resolveOption();
    antlr4::tree::TerminalNode *UINT();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  DeployOptionContext* deployOption();

  class  InterceptorContext : public antlr4::ParserRuleContext {
  public:
    InterceptorContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<ValueContext *> value();
    ValueContext* value(size_t i);
    std::vector<DeployOptionContext *> deployOption();
    DeployOptionContext* deployOption(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  InterceptorContext* interceptor();

  class  EventsContext : public antlr4::ParserRuleContext {
  public:
    EventsContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<ValueContext *> value();
    ValueContext* value(size_t i);
    antlr4::tree::TerminalNode *UINT();
    std::vector<StreamOptionContext *> streamOption();
    StreamOptionContext* streamOption(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  EventsContext* events();

  class  CollectContext : public antlr4::ParserRuleContext {
  public:
    CollectContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<StreamOptionContext *> streamOption();
    StreamOptionContext* streamOption(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  CollectContext* collect();

  class  RecordContext : public antlr4::ParserRuleContext {
  public:
    RecordContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    ValueContext *value();
    std::vector<StreamOptionContext *> streamOption();
    StreamOptionContext* streamOption(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  RecordContext* record();

  class  StreamOptionContext : public antlr4::ParserRuleContext {
  public:
    StreamOptionContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *UINT();
    ValueContext *value();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  StreamOptionContext* streamOption();

  class  CppContext : public antlr4::ParserRuleContext {
  public:
    CppContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<ValueContext *> value();
    ValueContext* value(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  CppContext* cpp();

  class  CompleteContext : public antlr4::ParserRuleContext {
  public:
    CompleteContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    ValueContext *value();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  CompleteContext* complete();

  class  HelpContext : public antlr4::ParserRuleContext {
  public:
    HelpContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    ValueContext *value();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  HelpContext* help();

  class  QuitContext : public antlr4::ParserRuleContext {
  public:
    QuitContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  QuitContext* quit();

  class  ValueContext : public antlr4::ParserRuleContext {
  public:
    ValueContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *STRING();
    antlr4::tree::TerminalNode *WORD();
    antlr4::tree::TerminalNode *UINT();
    KeywordContext *keyword();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  ValueContext* value();

  class  KeywordContext : public antlr4::ParserRuleContext {
  public:
    KeywordContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  KeywordContext* keyword();


  // By default the static state used to implement the parser is lazily initialized during the first
  // call to the constructor. You can call this function if you wish to initialize the static state
  // ahead of time.
  static void initialize();

private:
};

