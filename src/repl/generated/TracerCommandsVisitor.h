
// Generated from src/repl/TracerCommands.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"
#include "TracerCommandsParser.h"



/**
 * This class defines an abstract visitor for a parse tree
 * produced by TracerCommandsParser.
 */
class  TracerCommandsVisitor : public antlr4::tree::AbstractParseTreeVisitor {
public:

  /**
   * Visit parse trees produced by TracerCommandsParser.
   */
    virtual std::any visitLine(TracerCommandsParser::LineContext *context) = 0;

    virtual std::any visitCommand(TracerCommandsParser::CommandContext *context) = 0;

    virtual std::any visitServer(TracerCommandsParser::ServerContext *context) = 0;

    virtual std::any visitServerOption(TracerCommandsParser::ServerOptionContext *context) = 0;

    virtual std::any visitAttach(TracerCommandsParser::AttachContext *context) = 0;

    virtual std::any visitProcesses(TracerCommandsParser::ProcessesContext *context) = 0;

    virtual std::any visitSessions(TracerCommandsParser::SessionsContext *context) = 0;

    virtual std::any visitUse(TracerCommandsParser::UseContext *context) = 0;

    virtual std::any visitStatus(TracerCommandsParser::StatusContext *context) = 0;

    virtual std::any visitDetach(TracerCommandsParser::DetachContext *context) = 0;

    virtual std::any visitModules(TracerCommandsParser::ModulesContext *context) = 0;

    virtual std::any visitModuleIndex(TracerCommandsParser::ModuleIndexContext *context) = 0;

    virtual std::any visitModuleBuildOption(TracerCommandsParser::ModuleBuildOptionContext *context) = 0;

    virtual std::any visitModuleQueryOption(TracerCommandsParser::ModuleQueryOptionContext *context) = 0;

    virtual std::any visitModuleShowOption(TracerCommandsParser::ModuleShowOptionContext *context) = 0;

    virtual std::any visitModuleEmitOption(TracerCommandsParser::ModuleEmitOptionContext *context) = 0;

    virtual std::any visitThreads(TracerCommandsParser::ThreadsContext *context) = 0;

    virtual std::any visitSymbols(TracerCommandsParser::SymbolsContext *context) = 0;

    virtual std::any visitSymbolOption(TracerCommandsParser::SymbolOptionContext *context) = 0;

    virtual std::any visitProbe(TracerCommandsParser::ProbeContext *context) = 0;

    virtual std::any visitPrepareOption(TracerCommandsParser::PrepareOptionContext *context) = 0;

    virtual std::any visitLimitOption(TracerCommandsParser::LimitOptionContext *context) = 0;

    virtual std::any visitResolveOption(TracerCommandsParser::ResolveOptionContext *context) = 0;

    virtual std::any visitValidateOption(TracerCommandsParser::ValidateOptionContext *context) = 0;

    virtual std::any visitDeployOption(TracerCommandsParser::DeployOptionContext *context) = 0;

    virtual std::any visitInterceptor(TracerCommandsParser::InterceptorContext *context) = 0;

    virtual std::any visitEvents(TracerCommandsParser::EventsContext *context) = 0;

    virtual std::any visitCollect(TracerCommandsParser::CollectContext *context) = 0;

    virtual std::any visitRecord(TracerCommandsParser::RecordContext *context) = 0;

    virtual std::any visitStreamOption(TracerCommandsParser::StreamOptionContext *context) = 0;

    virtual std::any visitCpp(TracerCommandsParser::CppContext *context) = 0;

    virtual std::any visitShell(TracerCommandsParser::ShellContext *context) = 0;

    virtual std::any visitComplete(TracerCommandsParser::CompleteContext *context) = 0;

    virtual std::any visitType(TracerCommandsParser::TypeContext *context) = 0;

    virtual std::any visitHelp(TracerCommandsParser::HelpContext *context) = 0;

    virtual std::any visitQuit(TracerCommandsParser::QuitContext *context) = 0;

    virtual std::any visitBulkTrace(TracerCommandsParser::BulkTraceContext *context) = 0;

    virtual std::any visitBulkTraceOption(TracerCommandsParser::BulkTraceOptionContext *context) = 0;

    virtual std::any visitValue(TracerCommandsParser::ValueContext *context) = 0;

    virtual std::any visitKeyword(TracerCommandsParser::KeywordContext *context) = 0;

    virtual std::any visitObjectWatch(TracerCommandsParser::ObjectWatchContext *context) = 0;

    virtual std::any visitObjectOption(TracerCommandsParser::ObjectOptionContext *context) = 0;

    virtual std::any visitMemory(TracerCommandsParser::MemoryContext *context) = 0;

    virtual std::any visitProbeEntry(TracerCommandsParser::ProbeEntryContext *context) = 0;

    virtual std::any visitProbeReload(TracerCommandsParser::ProbeReloadContext *context) = 0;

    virtual std::any visitTargetLaunch(TracerCommandsParser::TargetLaunchContext *context) = 0;

    virtual std::any visitLaunchOption(TracerCommandsParser::LaunchOptionContext *context) = 0;

    virtual std::any visitChildList(TracerCommandsParser::ChildListContext *context) = 0;

    virtual std::any visitTargetCall(TracerCommandsParser::TargetCallContext *context) = 0;

    virtual std::any visitLocalPerf(TracerCommandsParser::LocalPerfContext *context) = 0;

    virtual std::any visitTargetLibrary(TracerCommandsParser::TargetLibraryContext *context) = 0;

    virtual std::any visitInstructions(TracerCommandsParser::InstructionsContext *context) = 0;

    virtual std::any visitInstructionOption(TracerCommandsParser::InstructionOptionContext *context) = 0;

    virtual std::any visitWatchpoint(TracerCommandsParser::WatchpointContext *context) = 0;

    virtual std::any visitStackFilter(TracerCommandsParser::StackFilterContext *context) = 0;

    virtual std::any visitContextEventBytes(TracerCommandsParser::ContextEventBytesContext *context) = 0;

    virtual std::any visitExtraCompileOption(TracerCommandsParser::ExtraCompileOptionContext *context) = 0;

    virtual std::any visitCwdOption(TracerCommandsParser::CwdOptionContext *context) = 0;


};

