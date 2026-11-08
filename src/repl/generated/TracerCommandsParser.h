
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
    T__68 = 69, T__69 = 70, T__70 = 71, T__71 = 72, T__72 = 73, T__73 = 74,
    T__74 = 75, T__75 = 76, T__76 = 77, T__77 = 78, T__78 = 79, T__79 = 80,
    T__80 = 81, T__81 = 82, T__82 = 83, T__83 = 84, T__84 = 85, T__85 = 86,
    T__86 = 87, T__87 = 88, T__88 = 89, T__89 = 90, T__90 = 91, T__91 = 92,
    T__92 = 93, T__93 = 94, T__94 = 95, T__95 = 96, T__96 = 97, T__97 = 98,
    T__98 = 99, T__99 = 100, T__100 = 101, T__101 = 102, T__102 = 103, T__103 = 104,
    T__104 = 105, T__105 = 106, T__106 = 107, T__107 = 108, T__108 = 109,
    T__109 = 110, T__110 = 111, T__111 = 112, T__112 = 113, T__113 = 114,
    T__114 = 115, T__115 = 116, T__116 = 117, T__117 = 118, T__118 = 119,
    T__119 = 120, T__120 = 121, T__121 = 122, T__122 = 123, T__123 = 124,
    T__124 = 125, T__125 = 126, T__126 = 127, T__127 = 128, T__128 = 129,
    T__129 = 130, T__130 = 131, T__131 = 132, T__132 = 133, T__133 = 134,
    T__134 = 135, T__135 = 136, T__136 = 137, T__137 = 138, T__138 = 139,
    T__139 = 140, T__140 = 141, T__141 = 142, T__142 = 143, T__143 = 144,
    T__144 = 145, T__145 = 146, T__146 = 147, T__147 = 148, T__148 = 149,
    T__149 = 150, T__150 = 151, T__151 = 152, T__152 = 153, T__153 = 154,
    T__154 = 155, T__155 = 156, T__156 = 157, T__157 = 158, T__158 = 159,
    T__159 = 160, UINT = 161, STRING = 162, SHELL = 163, METADATA_ONLY = 164,
    SQLITE = 165, STACK = 166, CWD = 167, WORD = 168, WS = 169
  };

  enum {
    RuleLine = 0, RuleCommand = 1, RuleServer = 2, RuleServerOption = 3,
    RuleAttach = 4, RuleProcesses = 5, RuleSessions = 6, RuleUse = 7, RuleStatus = 8,
    RuleDetach = 9, RuleModules = 10, RuleThreads = 11, RuleSymbols = 12,
    RuleSymbolOption = 13, RuleProbe = 14, RulePrepareOption = 15, RuleResolveOption = 16,
    RuleValidateOption = 17, RuleDeployOption = 18, RuleInterceptor = 19,
    RuleEvents = 20, RuleCollect = 21, RuleRecord = 22, RuleStreamOption = 23,
    RuleCpp = 24, RuleShell = 25, RuleComplete = 26, RuleType = 27, RuleHelp = 28,
    RuleQuit = 29, RuleBulkTrace = 30, RuleBulkTraceOption = 31, RuleValue = 32,
    RuleKeyword = 33, RuleMemory = 34, RuleProbeEntry = 35, RuleProbeReload = 36,
    RuleTargetLaunch = 37, RuleLaunchOption = 38, RuleChildList = 39, RuleTargetCall = 40,
    RuleLocalPerf = 41, RuleTargetLibrary = 42, RuleInstructions = 43, RuleInstructionOption = 44,
    RuleWatchpoint = 45, RuleStackFilter = 46, RuleContextEventBytes = 47,
    RuleExtraCompileOption = 48, RuleCwdOption = 49
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
  class ThreadsContext;
  class SymbolsContext;
  class SymbolOptionContext;
  class ProbeContext;
  class PrepareOptionContext;
  class ResolveOptionContext;
  class ValidateOptionContext;
  class DeployOptionContext;
  class InterceptorContext;
  class EventsContext;
  class CollectContext;
  class RecordContext;
  class StreamOptionContext;
  class CppContext;
  class ShellContext;
  class CompleteContext;
  class TypeContext;
  class HelpContext;
  class QuitContext;
  class BulkTraceContext;
  class BulkTraceOptionContext;
  class ValueContext;
  class KeywordContext;
  class MemoryContext;
  class ProbeEntryContext;
  class ProbeReloadContext;
  class TargetLaunchContext;
  class LaunchOptionContext;
  class ChildListContext;
  class TargetCallContext;
  class LocalPerfContext;
  class TargetLibraryContext;
  class InstructionsContext;
  class InstructionOptionContext;
  class WatchpointContext;
  class StackFilterContext;
  class ContextEventBytesContext;
  class ExtraCompileOptionContext;
  class CwdOptionContext;

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
    ThreadsContext *threads();
    SymbolsContext *symbols();
    ProbeContext *probe();
    InterceptorContext *interceptor();
    EventsContext *events();
    CollectContext *collect();
    RecordContext *record();
    CppContext *cpp();
    ShellContext *shell();
    CompleteContext *complete();
    TypeContext *type();
    HelpContext *help();
    QuitContext *quit();
    BulkTraceContext *bulkTrace();
    MemoryContext *memory();
    ProbeEntryContext *probeEntry();
    ProbeReloadContext *probeReload();
    TargetLaunchContext *targetLaunch();
    TargetCallContext *targetCall();
    LocalPerfContext *localPerf();
    InstructionsContext *instructions();
    WatchpointContext *watchpoint();
    TargetLibraryContext *targetLibrary();
    ChildListContext *childList();


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
    ContextEventBytesContext *contextEventBytes();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  ServerOptionContext* serverOption();

  class  AttachContext : public antlr4::ParserRuleContext {
  public:
    AttachContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<antlr4::tree::TerminalNode *> UINT();
    antlr4::tree::TerminalNode* UINT(size_t i);


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
    ValueContext *value();
    std::vector<antlr4::tree::TerminalNode *> UINT();
    antlr4::tree::TerminalNode* UINT(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  ModulesContext* modules();

  class  ThreadsContext : public antlr4::ParserRuleContext {
  public:
    ThreadsContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  ThreadsContext* threads();

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
    std::vector<PrepareOptionContext *> prepareOption();
    PrepareOptionContext* prepareOption(size_t i);
    std::vector<ResolveOptionContext *> resolveOption();
    ResolveOptionContext* resolveOption(size_t i);
    std::vector<CwdOptionContext *> cwdOption();
    CwdOptionContext* cwdOption(size_t i);
    std::vector<DeployOptionContext *> deployOption();
    DeployOptionContext* deployOption(size_t i);
    std::vector<ValidateOptionContext *> validateOption();
    ValidateOptionContext* validateOption(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  ProbeContext* probe();

  class  PrepareOptionContext : public antlr4::ParserRuleContext {
  public:
    PrepareOptionContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    ValueContext *value();
    antlr4::tree::TerminalNode *METADATA_ONLY();
    ExtraCompileOptionContext *extraCompileOption();
    CwdOptionContext *cwdOption();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  PrepareOptionContext* prepareOption();

  class  ResolveOptionContext : public antlr4::ParserRuleContext {
  public:
    ResolveOptionContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    ValueContext *value();
    ExtraCompileOptionContext *extraCompileOption();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  ResolveOptionContext* resolveOption();

  class  ValidateOptionContext : public antlr4::ParserRuleContext {
  public:
    ValidateOptionContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    ResolveOptionContext *resolveOption();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  ValidateOptionContext* validateOption();

  class  DeployOptionContext : public antlr4::ParserRuleContext {
  public:
    DeployOptionContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    ValidateOptionContext *validateOption();
    antlr4::tree::TerminalNode *UINT();
    antlr4::tree::TerminalNode *WORD();
    StackFilterContext *stackFilter();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  DeployOptionContext* deployOption();

  class  InterceptorContext : public antlr4::ParserRuleContext {
  public:
    InterceptorContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<ValueContext *> value();
    ValueContext* value(size_t i);
    std::vector<ValidateOptionContext *> validateOption();
    ValidateOptionContext* validateOption(size_t i);
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
    antlr4::tree::TerminalNode *STACK();
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
    std::vector<antlr4::tree::TerminalNode *> SQLITE();
    antlr4::tree::TerminalNode* SQLITE(size_t i);


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
    std::vector<antlr4::tree::TerminalNode *> SHELL();
    antlr4::tree::TerminalNode* SHELL(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  CppContext* cpp();

  class  ShellContext : public antlr4::ParserRuleContext {
  public:
    ShellContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    ValueContext *value();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  ShellContext* shell();

  class  CompleteContext : public antlr4::ParserRuleContext {
  public:
    CompleteContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    ValueContext *value();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  CompleteContext* complete();

  class  TypeContext : public antlr4::ParserRuleContext {
  public:
    TypeContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    ValueContext *value();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  TypeContext* type();

  class  HelpContext : public antlr4::ParserRuleContext {
  public:
    HelpContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<ValueContext *> value();
    ValueContext* value(size_t i);


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

  class  BulkTraceContext : public antlr4::ParserRuleContext {
  public:
    BulkTraceContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<ValueContext *> value();
    ValueContext* value(size_t i);
    std::vector<BulkTraceOptionContext *> bulkTraceOption();
    BulkTraceOptionContext* bulkTraceOption(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  BulkTraceContext* bulkTrace();

  class  BulkTraceOptionContext : public antlr4::ParserRuleContext {
  public:
    BulkTraceOptionContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    ValueContext *value();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  BulkTraceOptionContext* bulkTraceOption();

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
    antlr4::tree::TerminalNode *STACK();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  KeywordContext* keyword();

  class  MemoryContext : public antlr4::ParserRuleContext {
  public:
    MemoryContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<ValueContext *> value();
    ValueContext* value(size_t i);
    std::vector<antlr4::tree::TerminalNode *> UINT();
    antlr4::tree::TerminalNode* UINT(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  MemoryContext* memory();

  class  ProbeEntryContext : public antlr4::ParserRuleContext {
  public:
    ProbeEntryContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<ValueContext *> value();
    ValueContext* value(size_t i);
    CwdOptionContext *cwdOption();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  ProbeEntryContext* probeEntry();

  class  ProbeReloadContext : public antlr4::ParserRuleContext {
  public:
    ProbeReloadContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<antlr4::tree::TerminalNode *> UINT();
    antlr4::tree::TerminalNode* UINT(size_t i);
    ValueContext *value();
    std::vector<CwdOptionContext *> cwdOption();
    CwdOptionContext* cwdOption(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  ProbeReloadContext* probeReload();

  class  TargetLaunchContext : public antlr4::ParserRuleContext {
  public:
    TargetLaunchContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    ValueContext *value();
    std::vector<LaunchOptionContext *> launchOption();
    LaunchOptionContext* launchOption(size_t i);
    antlr4::tree::TerminalNode *UINT();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  TargetLaunchContext* targetLaunch();

  class  LaunchOptionContext : public antlr4::ParserRuleContext {
  public:
    LaunchOptionContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    ValueContext *value();
    antlr4::tree::TerminalNode *CWD();
    antlr4::tree::TerminalNode *UINT();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  LaunchOptionContext* launchOption();

  class  ChildListContext : public antlr4::ParserRuleContext {
  public:
    ChildListContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  ChildListContext* childList();

  class  TargetCallContext : public antlr4::ParserRuleContext {
  public:
    TargetCallContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<ValueContext *> value();
    ValueContext* value(size_t i);
    std::vector<antlr4::tree::TerminalNode *> UINT();
    antlr4::tree::TerminalNode* UINT(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  TargetCallContext* targetCall();

  class  LocalPerfContext : public antlr4::ParserRuleContext {
  public:
    LocalPerfContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<antlr4::tree::TerminalNode *> UINT();
    antlr4::tree::TerminalNode* UINT(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  LocalPerfContext* localPerf();

  class  TargetLibraryContext : public antlr4::ParserRuleContext {
  public:
    TargetLibraryContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    ValueContext *value();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  TargetLibraryContext* targetLibrary();

  class  InstructionsContext : public antlr4::ParserRuleContext {
  public:
    InstructionsContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    ValueContext *value();
    std::vector<InstructionOptionContext *> instructionOption();
    InstructionOptionContext* instructionOption(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  InstructionsContext* instructions();

  class  InstructionOptionContext : public antlr4::ParserRuleContext {
  public:
    InstructionOptionContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *UINT();
    ValueContext *value();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  InstructionOptionContext* instructionOption();

  class  WatchpointContext : public antlr4::ParserRuleContext {
  public:
    WatchpointContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<antlr4::tree::TerminalNode *> UINT();
    antlr4::tree::TerminalNode* UINT(size_t i);
    std::vector<ValueContext *> value();
    ValueContext* value(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  WatchpointContext* watchpoint();

  class  StackFilterContext : public antlr4::ParserRuleContext {
  public:
    StackFilterContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    ValueContext *value();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  StackFilterContext* stackFilter();

  class  ContextEventBytesContext : public antlr4::ParserRuleContext {
  public:
    ContextEventBytesContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *UINT();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  ContextEventBytesContext* contextEventBytes();

  class  ExtraCompileOptionContext : public antlr4::ParserRuleContext {
  public:
    ExtraCompileOptionContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<ValueContext *> value();
    ValueContext* value(size_t i);
    std::vector<antlr4::tree::TerminalNode *> SHELL();
    antlr4::tree::TerminalNode* SHELL(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  ExtraCompileOptionContext* extraCompileOption();

  class  CwdOptionContext : public antlr4::ParserRuleContext {
  public:
    CwdOptionContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *CWD();
    ValueContext *value();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;

  };

  CwdOptionContext* cwdOption();


  // By default the static state used to implement the parser is lazily initialized during the first
  // call to the constructor. You can call this function if you wish to initialize the static state
  // ahead of time.
  static void initialize();

private:
};

