
// Generated from src/repl/TracerCommands.g4 by ANTLR 4.13.2


#include "TracerCommandsVisitor.h"

#include "TracerCommandsParser.h"


using namespace antlrcpp;

using namespace antlr4;

namespace {

struct TracerCommandsParserStaticData final {
  TracerCommandsParserStaticData(std::vector<std::string> ruleNames,
                        std::vector<std::string> literalNames,
                        std::vector<std::string> symbolicNames)
      : ruleNames(std::move(ruleNames)), literalNames(std::move(literalNames)),
        symbolicNames(std::move(symbolicNames)),
        vocabulary(this->literalNames, this->symbolicNames) {}

  TracerCommandsParserStaticData(const TracerCommandsParserStaticData&) = delete;
  TracerCommandsParserStaticData(TracerCommandsParserStaticData&&) = delete;
  TracerCommandsParserStaticData& operator=(const TracerCommandsParserStaticData&) = delete;
  TracerCommandsParserStaticData& operator=(TracerCommandsParserStaticData&&) = delete;

  std::vector<antlr4::dfa::DFA> decisionToDFA;
  antlr4::atn::PredictionContextCache sharedContextCache;
  const std::vector<std::string> ruleNames;
  const std::vector<std::string> literalNames;
  const std::vector<std::string> symbolicNames;
  const antlr4::dfa::Vocabulary vocabulary;
  antlr4::atn::SerializedATNView serializedATN;
  std::unique_ptr<antlr4::atn::ATN> atn;
};

::antlr4::internal::OnceFlag tracercommandsParserOnceFlag;
#if ANTLR4_USE_THREAD_LOCAL_CACHE
static thread_local
#endif
std::unique_ptr<TracerCommandsParserStaticData> tracercommandsParserStaticData = nullptr;

void tracercommandsParserInitialize() {
#if ANTLR4_USE_THREAD_LOCAL_CACHE
  if (tracercommandsParserStaticData != nullptr) {
    return;
  }
#else
  assert(tracercommandsParserStaticData == nullptr);
#endif
  auto staticData = std::make_unique<TracerCommandsParserStaticData>(
    std::vector<std::string>{
      "line", "command", "server", "serverOption", "attach", "processes",
      "sessions", "use", "status", "detach", "modules", "moduleIndex", "moduleBuildOption",
      "moduleQueryOption", "moduleShowOption", "moduleEmitOption", "threads",
      "symbols", "symbolOption", "probe", "prepareOption", "limitOption",
      "resolveOption", "validateOption", "deployOption", "interceptor",
      "events", "collect", "record", "streamOption", "cpp", "shell", "complete",
      "type", "help", "quit", "bulkTrace", "bulkTraceOption", "value", "keyword",
      "memory", "probeEntry", "probeReload", "targetLaunch", "launchOption",
      "childList", "targetCall", "localPerf", "targetLibrary", "instructions",
      "instructionOption", "watchpoint", "stackFilter", "contextEventBytes",
      "extraCompileOption", "cwdOption"
    },
    std::vector<std::string>{
      "", "'%'", "'server'", "'start'", "'connect'", "'status'", "'stop'",
      "'--executable'", "'--agent'", "'attach'", "'--start-time'", "'processes'",
      "'--filter'", "'sessions'", "'use'", "'detach'", "'modules'", "'--details'",
      "'--elf'", "'--watch'", "'--timeout'", "'--count'", "'module'", "'index'",
      "'types'", "'symbols'", "'show'", "'emit'", "'--file'", "'--module'",
      "'--mapping'", "'--output'", "'--debug-file'", "'--max-entries'",
      "'--max-depth'", "'--index'", "'--page'", "'--limit'", "'--format'",
      "'json'", "'--exported'", "'--type'", "'--symbol'", "'--id'", "'threads'",
      "'--address'", "'--addresses'", "'probe'", "'prepare'", "'resolve'",
      "'inject'", "'--callback'", "'validate'", "'remove'", "'probes'",
      "'--include'", "'--compdb'", "'--translation-unit'", "'--module-index'",
      "'--generated'", "'--no-automatic-members'", "'--follow-depth'", "'--follow-members'",
      "'--record-members'", "'--nested-depth'", "'--string-bytes'", "'--vector-elements'",
      "'--checked-reads'", "'--snapshot-reserve'", "'--snapshot-arena'",
      "'--snapshot-slice'", "'--compile-option'", "'--prepared'", "'--prepared-mode'",
      "'--allow-arg-overrides'", "'--allow-result-override'", "'--allow-replacement'",
      "'--reclaimable'", "'--allow-call-original'", "'--trace-exceptions'",
      "'--stack-depth'", "'--print-arguments'", "'--print-result'", "'interceptor'",
      "'load'", "'--as'", "'edit'", "'--target'", "'upload'", "'events'",
      "'list'", "'calls'", "'stats'", "'clear'", "'open'", "'export'", "'collect'",
      "'record'", "'--after'", "'--probe'", "'--phase'", "'PRE'", "'POST'",
      "'EXCEPTION'", "'--name'", "'--session'", "'--all'", "'cpp'", "'reset'",
      "'import'", "'generate'", "'options'", "'add'", "'compdb'", "'shell'",
      "'complete'", "'type'", "'help'", "'quit'", "'exit'", "'trace'", "'preview'",
      "'apply'", "'--manifest'", "'--exclude'", "'--exact'", "'--save'",
      "'memory'", "'maps'", "'read'", "'scan'", "'patch'", "'restore'",
      "'import-hook'", "'monitor-start'", "'monitor-stop'", "'entry'", "'reload'",
      "'launch'", "'resume'", "'terminate'", "'call'", "'call-result'",
      "'syscalls'", "'sample'", "'cpu'", "'instructions'", "'watchpoint'",
      "'library'", "'allocate'", "'free'", "'write'", "'replay'", "'children'",
      "'faults'", "'allocations'", "'--pattern'", "'--elf-address'", "'--expect'",
      "'--replace'", "'--tid'", "'--size'", "'--original'", "'--abi'", "'--arg'",
      "'--env'", "'--stdin'", "'--stdout'", "'--stderr'", "'--preload'",
      "'--supervise'", "'--gate-children'", "'--memory-size'", "'--registers'",
      "'--discover'", "'--callback-library'", "'--memory-address'", "'--access'",
      "'--stack-application-only'", "'--context-event-bytes'", "'--context-transport-memory-bytes'",
      "'--extra-arg'", "'--extra-args'", "", "", "", "'--metadata-only'",
      "'--sqlite'", "'stack'", "'--cwd'", "'--signature'", "'--member'",
      "'--snapshot-return-pointee'"
    },
    std::vector<std::string>{
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
      "", "", "", "", "", "", "", "", "", "", "", "", "", "UINT", "STRING",
      "SHELL", "METADATA_ONLY", "SQLITE", "STACK", "CWD", "SIGNATURE", "MEMBER",
      "SNAPSHOT_RETURN_POINTEE", "WORD", "WS"
    }
  );
  static const int32_t serializedATNSegment[] = {
    4,1,194,795,2,0,7,0,2,1,7,1,2,2,7,2,2,3,7,3,2,4,7,4,2,5,7,5,2,6,7,6,2,
    7,7,7,2,8,7,8,2,9,7,9,2,10,7,10,2,11,7,11,2,12,7,12,2,13,7,13,2,14,7,
    14,2,15,7,15,2,16,7,16,2,17,7,17,2,18,7,18,2,19,7,19,2,20,7,20,2,21,7,
    21,2,22,7,22,2,23,7,23,2,24,7,24,2,25,7,25,2,26,7,26,2,27,7,27,2,28,7,
    28,2,29,7,29,2,30,7,30,2,31,7,31,2,32,7,32,2,33,7,33,2,34,7,34,2,35,7,
    35,2,36,7,36,2,37,7,37,2,38,7,38,2,39,7,39,2,40,7,40,2,41,7,41,2,42,7,
    42,2,43,7,43,2,44,7,44,2,45,7,45,2,46,7,46,2,47,7,47,2,48,7,48,2,49,7,
    49,2,50,7,50,2,51,7,51,2,52,7,52,2,53,7,53,2,54,7,54,2,55,7,55,1,0,1,
    0,1,0,1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,3,1,150,8,1,1,2,1,2,1,2,5,2,155,8,2,10,2,12,2,158,9,2,1,2,1,2,1,2,1,
    2,3,2,164,8,2,1,3,1,3,1,3,3,3,169,8,3,1,4,1,4,1,4,1,4,3,4,175,8,4,3,4,
    177,8,4,1,5,1,5,1,5,3,5,182,8,5,1,6,1,6,1,7,1,7,1,7,1,8,1,8,1,9,1,9,3,
    9,193,8,9,1,10,1,10,1,10,1,10,5,10,199,8,10,10,10,12,10,202,9,10,1,10,
    1,10,1,10,1,10,1,10,5,10,209,8,10,10,10,12,10,212,9,10,3,10,214,8,10,
    1,11,1,11,1,11,4,11,219,8,11,11,11,12,11,220,1,11,1,11,1,11,1,11,5,11,
    227,8,11,10,11,12,11,230,9,11,1,11,1,11,4,11,234,8,11,11,11,12,11,235,
    1,11,1,11,4,11,240,8,11,11,11,12,11,241,3,11,244,8,11,1,12,1,12,1,12,
    1,12,3,12,250,8,12,1,13,1,13,1,13,1,13,1,13,1,13,1,13,3,13,259,8,13,1,
    14,1,14,1,14,1,15,1,15,1,15,3,15,267,8,15,1,16,1,16,1,17,1,17,5,17,273,
    8,17,10,17,12,17,276,9,17,1,18,1,18,1,18,1,18,1,18,3,18,283,8,18,1,19,
    1,19,1,19,1,19,4,19,289,8,19,11,19,12,19,290,1,19,1,19,1,19,1,19,5,19,
    297,8,19,10,19,12,19,300,9,19,1,19,1,19,1,19,1,19,3,19,306,8,19,1,19,
    1,19,5,19,310,8,19,10,19,12,19,313,9,19,1,19,1,19,1,19,1,19,1,19,1,19,
    5,19,321,8,19,10,19,12,19,324,9,19,1,19,1,19,1,19,3,19,329,8,19,3,19,
    331,8,19,1,19,1,19,3,19,335,8,19,3,19,337,8,19,1,20,1,20,1,20,1,20,1,
    20,1,20,1,20,1,20,1,20,1,20,3,20,349,8,20,1,21,1,21,1,21,1,22,1,22,1,
    22,3,22,357,8,22,1,23,1,23,1,23,1,23,1,23,1,23,1,23,1,23,3,23,367,8,23,
    1,24,1,24,1,24,1,24,1,24,1,24,1,24,3,24,376,8,24,1,25,1,25,1,25,1,25,
    1,25,1,25,1,25,1,25,1,25,1,25,1,25,1,25,1,25,1,25,1,25,5,25,393,8,25,
    10,25,12,25,396,9,25,1,25,1,25,1,25,1,25,1,25,5,25,403,8,25,10,25,12,
    25,406,9,25,3,25,408,8,25,1,26,1,26,5,26,412,8,26,10,26,12,26,415,9,26,
    1,26,1,26,1,26,1,26,3,26,421,8,26,1,26,1,26,1,26,1,26,1,26,1,26,1,26,
    1,26,1,26,1,26,1,26,1,26,1,26,1,26,1,26,1,26,1,26,1,26,1,26,1,26,1,26,
    1,26,1,26,1,26,1,26,1,26,1,26,1,26,3,26,451,8,26,1,27,1,27,5,27,455,8,
    27,10,27,12,27,458,9,27,1,28,1,28,1,28,1,28,5,28,464,8,28,10,28,12,28,
    467,9,28,1,29,1,29,1,29,1,29,1,29,1,29,1,29,3,29,476,8,29,1,30,1,30,1,
    30,1,30,1,30,1,30,1,30,1,30,5,30,486,8,30,10,30,12,30,489,9,30,1,30,1,
    30,1,30,1,30,4,30,495,8,30,11,30,12,30,496,1,30,1,30,1,30,1,30,1,30,1,
    30,3,30,505,8,30,3,30,507,8,30,1,31,1,31,1,31,1,32,1,32,1,32,1,33,1,33,
    1,33,1,34,1,34,3,34,520,8,34,1,34,4,34,523,8,34,11,34,12,34,524,3,34,
    527,8,34,1,35,1,35,1,36,1,36,1,36,4,36,534,8,36,11,36,12,36,535,1,36,
    1,36,1,36,1,36,1,36,3,36,543,8,36,1,36,1,36,3,36,547,8,36,1,37,1,37,1,
    37,1,38,1,38,1,38,1,38,3,38,556,8,38,1,39,1,39,1,40,1,40,1,40,1,40,1,
    40,1,40,1,40,1,40,1,40,1,40,3,40,570,8,40,1,40,1,40,1,40,1,40,1,40,1,
    40,1,40,1,40,1,40,1,40,3,40,582,8,40,1,40,1,40,1,40,1,40,1,40,1,40,1,
    40,1,40,1,40,1,40,1,40,1,40,1,40,1,40,1,40,1,40,1,40,1,40,1,40,1,40,1,
    40,1,40,1,40,1,40,1,40,1,40,1,40,1,40,1,40,1,40,1,40,1,40,1,40,1,40,3,
    40,618,8,40,1,40,1,40,1,40,1,40,1,40,1,40,1,40,1,40,1,40,1,40,1,40,1,
    40,1,40,1,40,1,40,3,40,635,8,40,1,41,1,41,1,41,1,41,1,41,1,41,1,41,3,
    41,644,8,41,1,41,1,41,1,41,1,41,1,41,1,41,1,41,1,41,1,41,3,41,655,8,41,
    1,41,3,41,658,8,41,1,42,1,42,1,42,1,42,1,42,1,42,1,42,1,42,1,42,5,42,
    669,8,42,10,42,12,42,672,9,42,5,42,674,8,42,10,42,12,42,677,9,42,1,43,
    1,43,1,43,5,43,682,8,43,10,43,12,43,685,9,43,1,43,1,43,3,43,689,8,43,
    3,43,691,8,43,1,44,1,44,1,44,1,44,1,44,1,44,3,44,699,8,44,1,45,1,45,1,
    46,1,46,1,46,1,46,1,46,1,46,5,46,709,8,46,10,46,12,46,712,9,46,1,46,1,
    46,1,46,3,46,717,8,46,3,46,719,8,46,1,47,1,47,1,47,1,47,3,47,725,8,47,
    1,47,1,47,5,47,729,8,47,10,47,12,47,732,9,47,1,48,1,48,1,48,1,48,1,49,
    1,49,1,49,5,49,741,8,49,10,49,12,49,744,9,49,1,49,1,49,1,49,3,49,749,
    8,49,1,49,1,49,3,49,753,8,49,1,50,1,50,1,50,1,50,1,50,1,50,1,50,1,50,
    3,50,763,8,50,1,51,1,51,1,51,1,51,1,51,5,51,770,8,51,10,51,12,51,773,
    9,51,1,52,1,52,1,52,1,53,1,53,1,53,1,54,1,54,1,54,1,54,1,54,4,54,786,
    8,54,11,54,12,54,787,3,54,790,8,54,1,55,1,55,1,55,1,55,0,0,56,0,2,4,6,
    8,10,12,14,16,18,20,22,24,26,28,30,32,34,36,38,40,42,44,46,48,50,52,54,
    56,58,60,62,64,66,68,70,72,74,76,78,80,82,84,86,88,90,92,94,96,98,100,
    102,104,106,108,110,0,28,1,0,7,8,1,0,20,21,1,0,24,25,1,0,28,32,1,0,33,
    34,2,0,12,12,35,36,2,0,35,35,41,43,4,0,12,12,29,30,36,36,45,45,4,0,29,
    29,31,31,43,43,55,59,1,0,61,70,2,0,183,183,193,193,3,0,29,29,55,57,71,
    73,2,0,20,21,98,99,1,0,101,103,2,0,85,85,104,105,1,0,109,110,2,0,31,31,
    41,41,1,0,118,119,3,0,29,29,55,55,123,126,16,0,2,6,9,9,11,11,13,16,22,
    27,39,39,44,44,47,50,52,54,83,84,86,86,88,97,101,103,107,122,127,155,
    188,188,2,0,132,132,149,150,1,0,139,140,2,0,164,169,189,189,2,0,145,145,
    155,155,2,0,20,21,160,161,3,0,30,30,45,45,175,176,3,0,30,30,45,45,177,
    177,1,0,179,180,919,0,112,1,0,0,0,2,149,1,0,0,0,4,151,1,0,0,0,6,168,1,
    0,0,0,8,170,1,0,0,0,10,178,1,0,0,0,12,183,1,0,0,0,14,185,1,0,0,0,16,188,
    1,0,0,0,18,190,1,0,0,0,20,194,1,0,0,0,22,215,1,0,0,0,24,249,1,0,0,0,26,
    258,1,0,0,0,28,260,1,0,0,0,30,266,1,0,0,0,32,268,1,0,0,0,34,270,1,0,0,
    0,36,282,1,0,0,0,38,336,1,0,0,0,40,348,1,0,0,0,42,350,1,0,0,0,44,356,
    1,0,0,0,46,366,1,0,0,0,48,375,1,0,0,0,50,377,1,0,0,0,52,409,1,0,0,0,54,
    452,1,0,0,0,56,459,1,0,0,0,58,475,1,0,0,0,60,477,1,0,0,0,62,508,1,0,0,
    0,64,511,1,0,0,0,66,514,1,0,0,0,68,517,1,0,0,0,70,528,1,0,0,0,72,530,
    1,0,0,0,74,548,1,0,0,0,76,555,1,0,0,0,78,557,1,0,0,0,80,559,1,0,0,0,82,
    636,1,0,0,0,84,659,1,0,0,0,86,690,1,0,0,0,88,698,1,0,0,0,90,700,1,0,0,
    0,92,718,1,0,0,0,94,724,1,0,0,0,96,733,1,0,0,0,98,737,1,0,0,0,100,762,
    1,0,0,0,102,764,1,0,0,0,104,774,1,0,0,0,106,777,1,0,0,0,108,789,1,0,0,
    0,110,791,1,0,0,0,112,113,5,1,0,0,113,114,3,2,1,0,114,115,5,0,0,1,115,
    1,1,0,0,0,116,150,3,4,2,0,117,150,3,8,4,0,118,150,3,10,5,0,119,150,3,
    12,6,0,120,150,3,14,7,0,121,150,3,16,8,0,122,150,3,18,9,0,123,150,3,20,
    10,0,124,150,3,22,11,0,125,150,3,32,16,0,126,150,3,34,17,0,127,150,3,
    38,19,0,128,150,3,50,25,0,129,150,3,52,26,0,130,150,3,54,27,0,131,150,
    3,56,28,0,132,150,3,60,30,0,133,150,3,62,31,0,134,150,3,64,32,0,135,150,
    3,66,33,0,136,150,3,68,34,0,137,150,3,70,35,0,138,150,3,72,36,0,139,150,
    3,80,40,0,140,150,3,82,41,0,141,150,3,84,42,0,142,150,3,86,43,0,143,150,
    3,92,46,0,144,150,3,94,47,0,145,150,3,98,49,0,146,150,3,102,51,0,147,
    150,3,96,48,0,148,150,3,90,45,0,149,116,1,0,0,0,149,117,1,0,0,0,149,118,
    1,0,0,0,149,119,1,0,0,0,149,120,1,0,0,0,149,121,1,0,0,0,149,122,1,0,0,
    0,149,123,1,0,0,0,149,124,1,0,0,0,149,125,1,0,0,0,149,126,1,0,0,0,149,
    127,1,0,0,0,149,128,1,0,0,0,149,129,1,0,0,0,149,130,1,0,0,0,149,131,1,
    0,0,0,149,132,1,0,0,0,149,133,1,0,0,0,149,134,1,0,0,0,149,135,1,0,0,0,
    149,136,1,0,0,0,149,137,1,0,0,0,149,138,1,0,0,0,149,139,1,0,0,0,149,140,
    1,0,0,0,149,141,1,0,0,0,149,142,1,0,0,0,149,143,1,0,0,0,149,144,1,0,0,
    0,149,145,1,0,0,0,149,146,1,0,0,0,149,147,1,0,0,0,149,148,1,0,0,0,150,
    3,1,0,0,0,151,163,5,2,0,0,152,156,5,3,0,0,153,155,3,6,3,0,154,153,1,0,
    0,0,155,158,1,0,0,0,156,154,1,0,0,0,156,157,1,0,0,0,157,164,1,0,0,0,158,
    156,1,0,0,0,159,160,5,4,0,0,160,164,3,76,38,0,161,164,5,5,0,0,162,164,
    5,6,0,0,163,152,1,0,0,0,163,159,1,0,0,0,163,161,1,0,0,0,163,162,1,0,0,
    0,164,5,1,0,0,0,165,166,7,0,0,0,166,169,3,76,38,0,167,169,3,106,53,0,
    168,165,1,0,0,0,168,167,1,0,0,0,169,7,1,0,0,0,170,176,5,9,0,0,171,174,
    5,183,0,0,172,173,5,10,0,0,173,175,5,183,0,0,174,172,1,0,0,0,174,175,
    1,0,0,0,175,177,1,0,0,0,176,171,1,0,0,0,176,177,1,0,0,0,177,9,1,0,0,0,
    178,181,5,11,0,0,179,180,5,12,0,0,180,182,3,76,38,0,181,179,1,0,0,0,181,
    182,1,0,0,0,182,11,1,0,0,0,183,184,5,13,0,0,184,13,1,0,0,0,185,186,5,
    14,0,0,186,187,3,76,38,0,187,15,1,0,0,0,188,189,5,5,0,0,189,17,1,0,0,
    0,190,192,5,15,0,0,191,193,3,76,38,0,192,191,1,0,0,0,192,193,1,0,0,0,
    193,19,1,0,0,0,194,213,5,16,0,0,195,199,5,17,0,0,196,197,5,12,0,0,197,
    199,3,76,38,0,198,195,1,0,0,0,198,196,1,0,0,0,199,202,1,0,0,0,200,198,
    1,0,0,0,200,201,1,0,0,0,201,214,1,0,0,0,202,200,1,0,0,0,203,204,5,18,
    0,0,204,214,3,76,38,0,205,210,5,19,0,0,206,207,7,1,0,0,207,209,5,183,
    0,0,208,206,1,0,0,0,209,212,1,0,0,0,210,208,1,0,0,0,210,211,1,0,0,0,211,
    214,1,0,0,0,212,210,1,0,0,0,213,200,1,0,0,0,213,203,1,0,0,0,213,205,1,
    0,0,0,214,21,1,0,0,0,215,243,5,22,0,0,216,218,5,23,0,0,217,219,3,24,12,
    0,218,217,1,0,0,0,219,220,1,0,0,0,220,218,1,0,0,0,220,221,1,0,0,0,221,
    244,1,0,0,0,222,223,5,14,0,0,223,244,3,76,38,0,224,228,7,2,0,0,225,227,
    3,26,13,0,226,225,1,0,0,0,227,230,1,0,0,0,228,226,1,0,0,0,228,229,1,0,
    0,0,229,244,1,0,0,0,230,228,1,0,0,0,231,233,5,26,0,0,232,234,3,28,14,
    0,233,232,1,0,0,0,234,235,1,0,0,0,235,233,1,0,0,0,235,236,1,0,0,0,236,
    244,1,0,0,0,237,239,5,27,0,0,238,240,3,30,15,0,239,238,1,0,0,0,240,241,
    1,0,0,0,241,239,1,0,0,0,241,242,1,0,0,0,242,244,1,0,0,0,243,216,1,0,0,
    0,243,222,1,0,0,0,243,224,1,0,0,0,243,231,1,0,0,0,243,237,1,0,0,0,244,
    23,1,0,0,0,245,246,7,3,0,0,246,250,3,76,38,0,247,248,7,4,0,0,248,250,
    5,183,0,0,249,245,1,0,0,0,249,247,1,0,0,0,250,25,1,0,0,0,251,252,7,5,
    0,0,252,259,3,76,38,0,253,254,5,37,0,0,254,259,5,183,0,0,255,256,5,38,
    0,0,256,259,5,39,0,0,257,259,5,40,0,0,258,251,1,0,0,0,258,253,1,0,0,0,
    258,255,1,0,0,0,258,257,1,0,0,0,259,27,1,0,0,0,260,261,7,6,0,0,261,262,
    3,76,38,0,262,29,1,0,0,0,263,267,3,28,14,0,264,265,5,31,0,0,265,267,3,
    76,38,0,266,263,1,0,0,0,266,264,1,0,0,0,267,31,1,0,0,0,268,269,5,44,0,
    0,269,33,1,0,0,0,270,274,5,25,0,0,271,273,3,36,18,0,272,271,1,0,0,0,273,
    276,1,0,0,0,274,272,1,0,0,0,274,275,1,0,0,0,275,35,1,0,0,0,276,274,1,
    0,0,0,277,278,7,7,0,0,278,283,3,76,38,0,279,280,5,37,0,0,280,283,5,183,
    0,0,281,283,5,46,0,0,282,277,1,0,0,0,282,279,1,0,0,0,282,281,1,0,0,0,
    283,37,1,0,0,0,284,330,5,47,0,0,285,286,5,48,0,0,286,288,3,76,38,0,287,
    289,3,40,20,0,288,287,1,0,0,0,289,290,1,0,0,0,290,288,1,0,0,0,290,291,
    1,0,0,0,291,331,1,0,0,0,292,293,5,49,0,0,293,298,3,76,38,0,294,297,3,
    44,22,0,295,297,3,110,55,0,296,294,1,0,0,0,296,295,1,0,0,0,297,300,1,
    0,0,0,298,296,1,0,0,0,298,299,1,0,0,0,299,331,1,0,0,0,300,298,1,0,0,0,
    301,302,5,50,0,0,302,305,3,76,38,0,303,304,5,51,0,0,304,306,3,76,38,0,
    305,303,1,0,0,0,305,306,1,0,0,0,306,311,1,0,0,0,307,310,3,48,24,0,308,
    310,3,110,55,0,309,307,1,0,0,0,309,308,1,0,0,0,310,313,1,0,0,0,311,309,
    1,0,0,0,311,312,1,0,0,0,312,331,1,0,0,0,313,311,1,0,0,0,314,315,5,52,
    0,0,315,316,3,76,38,0,316,317,5,51,0,0,317,322,3,76,38,0,318,321,3,46,
    23,0,319,321,3,110,55,0,320,318,1,0,0,0,320,319,1,0,0,0,321,324,1,0,0,
    0,322,320,1,0,0,0,322,323,1,0,0,0,323,331,1,0,0,0,324,322,1,0,0,0,325,
    326,5,53,0,0,326,328,5,183,0,0,327,329,3,110,55,0,328,327,1,0,0,0,328,
    329,1,0,0,0,329,331,1,0,0,0,330,285,1,0,0,0,330,292,1,0,0,0,330,301,1,
    0,0,0,330,314,1,0,0,0,330,325,1,0,0,0,331,337,1,0,0,0,332,334,5,54,0,
    0,333,335,3,110,55,0,334,333,1,0,0,0,334,335,1,0,0,0,335,337,1,0,0,0,
    336,284,1,0,0,0,336,332,1,0,0,0,337,39,1,0,0,0,338,339,7,8,0,0,339,349,
    3,76,38,0,340,341,5,190,0,0,341,349,3,76,38,0,342,349,5,191,0,0,343,349,
    5,186,0,0,344,349,5,60,0,0,345,349,3,42,21,0,346,349,3,108,54,0,347,349,
    3,110,55,0,348,338,1,0,0,0,348,340,1,0,0,0,348,342,1,0,0,0,348,343,1,
    0,0,0,348,344,1,0,0,0,348,345,1,0,0,0,348,346,1,0,0,0,348,347,1,0,0,0,
    349,41,1,0,0,0,350,351,7,9,0,0,351,352,7,10,0,0,352,43,1,0,0,0,353,354,
    7,11,0,0,354,357,3,76,38,0,355,357,3,108,54,0,356,353,1,0,0,0,356,355,
    1,0,0,0,357,45,1,0,0,0,358,367,3,44,22,0,359,367,3,42,21,0,360,367,5,
    74,0,0,361,367,5,75,0,0,362,367,5,76,0,0,363,367,5,77,0,0,364,367,5,78,
    0,0,365,367,5,79,0,0,366,358,1,0,0,0,366,359,1,0,0,0,366,360,1,0,0,0,
    366,361,1,0,0,0,366,362,1,0,0,0,366,363,1,0,0,0,366,364,1,0,0,0,366,365,
    1,0,0,0,367,47,1,0,0,0,368,376,3,46,23,0,369,370,5,80,0,0,370,376,7,10,
    0,0,371,376,5,81,0,0,372,376,5,82,0,0,373,376,5,192,0,0,374,376,3,104,
    52,0,375,368,1,0,0,0,375,369,1,0,0,0,375,371,1,0,0,0,375,372,1,0,0,0,
    375,373,1,0,0,0,375,374,1,0,0,0,376,49,1,0,0,0,377,407,5,83,0,0,378,379,
    5,84,0,0,379,380,3,76,38,0,380,381,5,85,0,0,381,382,3,76,38,0,382,408,
    1,0,0,0,383,384,5,86,0,0,384,408,3,76,38,0,385,386,5,26,0,0,386,408,3,
    76,38,0,387,388,5,52,0,0,388,389,3,76,38,0,389,390,5,87,0,0,390,394,3,
    76,38,0,391,393,3,46,23,0,392,391,1,0,0,0,393,396,1,0,0,0,394,392,1,0,
    0,0,394,395,1,0,0,0,395,408,1,0,0,0,396,394,1,0,0,0,397,398,5,88,0,0,
    398,399,3,76,38,0,399,400,5,87,0,0,400,404,3,76,38,0,401,403,3,48,24,
    0,402,401,1,0,0,0,403,406,1,0,0,0,404,402,1,0,0,0,404,405,1,0,0,0,405,
    408,1,0,0,0,406,404,1,0,0,0,407,378,1,0,0,0,407,383,1,0,0,0,407,385,1,
    0,0,0,407,387,1,0,0,0,407,397,1,0,0,0,408,51,1,0,0,0,409,450,5,89,0,0,
    410,412,3,58,29,0,411,410,1,0,0,0,412,415,1,0,0,0,413,411,1,0,0,0,413,
    414,1,0,0,0,414,451,1,0,0,0,415,413,1,0,0,0,416,417,5,90,0,0,417,420,
    3,76,38,0,418,419,5,37,0,0,419,421,5,183,0,0,420,418,1,0,0,0,420,421,
    1,0,0,0,421,451,1,0,0,0,422,423,5,26,0,0,423,424,3,76,38,0,424,425,5,
    35,0,0,425,426,5,183,0,0,426,451,1,0,0,0,427,428,5,188,0,0,428,429,3,
    76,38,0,429,430,5,35,0,0,430,431,5,183,0,0,431,451,1,0,0,0,432,433,5,
    91,0,0,433,451,3,76,38,0,434,435,5,92,0,0,435,451,3,76,38,0,436,437,5,
    93,0,0,437,451,3,76,38,0,438,439,5,94,0,0,439,440,3,76,38,0,440,441,5,
    85,0,0,441,442,3,76,38,0,442,451,1,0,0,0,443,444,5,95,0,0,444,445,3,76,
    38,0,445,446,5,38,0,0,446,447,5,39,0,0,447,448,5,31,0,0,448,449,3,76,
    38,0,449,451,1,0,0,0,450,413,1,0,0,0,450,416,1,0,0,0,450,422,1,0,0,0,
    450,427,1,0,0,0,450,432,1,0,0,0,450,434,1,0,0,0,450,436,1,0,0,0,450,438,
    1,0,0,0,450,443,1,0,0,0,451,53,1,0,0,0,452,456,5,96,0,0,453,455,3,58,
    29,0,454,453,1,0,0,0,455,458,1,0,0,0,456,454,1,0,0,0,456,457,1,0,0,0,
    457,55,1,0,0,0,458,456,1,0,0,0,459,460,5,97,0,0,460,465,3,76,38,0,461,
    464,3,58,29,0,462,464,5,187,0,0,463,461,1,0,0,0,463,462,1,0,0,0,464,467,
    1,0,0,0,465,463,1,0,0,0,465,466,1,0,0,0,466,57,1,0,0,0,467,465,1,0,0,
    0,468,469,7,12,0,0,469,476,5,183,0,0,470,471,5,100,0,0,471,476,7,13,0,
    0,472,473,7,14,0,0,473,476,3,76,38,0,474,476,5,106,0,0,475,468,1,0,0,
    0,475,470,1,0,0,0,475,472,1,0,0,0,475,474,1,0,0,0,476,59,1,0,0,0,477,
    506,5,107,0,0,478,479,5,84,0,0,479,507,3,76,38,0,480,507,5,108,0,0,481,
    482,7,15,0,0,482,487,3,76,38,0,483,484,7,16,0,0,484,486,3,76,38,0,485,
    483,1,0,0,0,486,489,1,0,0,0,487,485,1,0,0,0,487,488,1,0,0,0,488,507,1,
    0,0,0,489,487,1,0,0,0,490,504,5,111,0,0,491,494,5,112,0,0,492,495,3,76,
    38,0,493,495,5,185,0,0,494,492,1,0,0,0,494,493,1,0,0,0,495,496,1,0,0,
    0,496,494,1,0,0,0,496,497,1,0,0,0,497,505,1,0,0,0,498,505,5,93,0,0,499,
    500,5,113,0,0,500,501,3,76,38,0,501,502,5,28,0,0,502,503,3,76,38,0,503,
    505,1,0,0,0,504,491,1,0,0,0,504,498,1,0,0,0,504,499,1,0,0,0,504,505,1,
    0,0,0,505,507,1,0,0,0,506,478,1,0,0,0,506,480,1,0,0,0,506,481,1,0,0,0,
    506,490,1,0,0,0,507,61,1,0,0,0,508,509,5,114,0,0,509,510,3,76,38,0,510,
    63,1,0,0,0,511,512,5,115,0,0,512,513,3,76,38,0,513,65,1,0,0,0,514,515,
    5,116,0,0,515,516,3,76,38,0,516,67,1,0,0,0,517,526,5,117,0,0,518,520,
    5,1,0,0,519,518,1,0,0,0,519,520,1,0,0,0,520,522,1,0,0,0,521,523,3,76,
    38,0,522,521,1,0,0,0,523,524,1,0,0,0,524,522,1,0,0,0,524,525,1,0,0,0,
    525,527,1,0,0,0,526,519,1,0,0,0,526,527,1,0,0,0,527,69,1,0,0,0,528,529,
    7,17,0,0,529,71,1,0,0,0,530,546,5,120,0,0,531,533,5,121,0,0,532,534,3,
    74,37,0,533,532,1,0,0,0,534,535,1,0,0,0,535,533,1,0,0,0,535,536,1,0,0,
    0,536,547,1,0,0,0,537,538,5,122,0,0,538,539,5,123,0,0,539,542,3,76,38,
    0,540,541,5,51,0,0,541,543,3,76,38,0,542,540,1,0,0,0,542,543,1,0,0,0,
    543,547,1,0,0,0,544,545,5,53,0,0,545,547,3,76,38,0,546,531,1,0,0,0,546,
    537,1,0,0,0,546,544,1,0,0,0,547,73,1,0,0,0,548,549,7,18,0,0,549,550,3,
    76,38,0,550,75,1,0,0,0,551,556,5,184,0,0,552,556,5,193,0,0,553,556,5,
    183,0,0,554,556,3,78,39,0,555,551,1,0,0,0,555,552,1,0,0,0,555,553,1,0,
    0,0,555,554,1,0,0,0,556,77,1,0,0,0,557,558,7,19,0,0,558,79,1,0,0,0,559,
    634,5,127,0,0,560,635,5,128,0,0,561,562,5,129,0,0,562,563,3,76,38,0,563,
    564,5,30,0,0,564,569,3,76,38,0,565,566,5,21,0,0,566,570,5,183,0,0,567,
    568,5,41,0,0,568,570,3,76,38,0,569,565,1,0,0,0,569,567,1,0,0,0,570,635,
    1,0,0,0,571,572,5,130,0,0,572,573,3,76,38,0,573,574,5,30,0,0,574,575,
    3,76,38,0,575,576,5,21,0,0,576,577,5,183,0,0,577,578,5,156,0,0,578,581,
    3,76,38,0,579,580,5,37,0,0,580,582,5,183,0,0,581,579,1,0,0,0,581,582,
    1,0,0,0,582,635,1,0,0,0,583,584,5,131,0,0,584,585,5,29,0,0,585,586,3,
    76,38,0,586,587,5,157,0,0,587,588,3,76,38,0,588,589,5,30,0,0,589,590,
    3,76,38,0,590,591,5,158,0,0,591,592,3,76,38,0,592,593,5,159,0,0,593,594,
    3,76,38,0,594,635,1,0,0,0,595,596,5,151,0,0,596,597,3,76,38,0,597,598,
    5,30,0,0,598,599,3,76,38,0,599,600,5,158,0,0,600,601,3,76,38,0,601,602,
    5,159,0,0,602,603,3,76,38,0,603,635,1,0,0,0,604,605,7,20,0,0,605,635,
    5,183,0,0,606,607,5,134,0,0,607,608,5,160,0,0,608,609,5,183,0,0,609,610,
    5,45,0,0,610,611,3,76,38,0,611,612,5,161,0,0,612,613,5,183,0,0,613,614,
    5,30,0,0,614,617,3,76,38,0,615,616,5,20,0,0,616,618,5,183,0,0,617,615,
    1,0,0,0,617,618,1,0,0,0,618,635,1,0,0,0,619,635,5,135,0,0,620,621,5,133,
    0,0,621,622,5,29,0,0,622,623,3,76,38,0,623,624,5,42,0,0,624,625,3,76,
    38,0,625,626,5,162,0,0,626,627,3,76,38,0,627,628,5,45,0,0,628,629,3,76,
    38,0,629,630,5,30,0,0,630,631,3,76,38,0,631,632,5,163,0,0,632,633,3,76,
    38,0,633,635,1,0,0,0,634,560,1,0,0,0,634,561,1,0,0,0,634,571,1,0,0,0,
    634,583,1,0,0,0,634,595,1,0,0,0,634,604,1,0,0,0,634,606,1,0,0,0,634,619,
    1,0,0,0,634,620,1,0,0,0,635,81,1,0,0,0,636,637,5,47,0,0,637,654,5,136,
    0,0,638,639,3,76,38,0,639,640,5,30,0,0,640,643,3,76,38,0,641,642,5,29,
    0,0,642,644,3,76,38,0,643,641,1,0,0,0,643,644,1,0,0,0,644,655,1,0,0,0,
    645,646,5,29,0,0,646,647,3,76,38,0,647,648,5,157,0,0,648,649,3,76,38,
    0,649,650,5,163,0,0,650,651,3,76,38,0,651,652,5,30,0,0,652,653,3,76,38,
    0,653,655,1,0,0,0,654,638,1,0,0,0,654,645,1,0,0,0,655,657,1,0,0,0,656,
    658,3,110,55,0,657,656,1,0,0,0,657,658,1,0,0,0,658,83,1,0,0,0,659,660,
    5,47,0,0,660,661,5,137,0,0,661,662,5,183,0,0,662,663,5,51,0,0,663,675,
    3,76,38,0,664,674,3,110,55,0,665,670,5,19,0,0,666,667,7,1,0,0,667,669,
    5,183,0,0,668,666,1,0,0,0,669,672,1,0,0,0,670,668,1,0,0,0,670,671,1,0,
    0,0,671,674,1,0,0,0,672,670,1,0,0,0,673,664,1,0,0,0,673,665,1,0,0,0,674,
    677,1,0,0,0,675,673,1,0,0,0,675,676,1,0,0,0,676,85,1,0,0,0,677,675,1,
    0,0,0,678,679,5,138,0,0,679,683,3,76,38,0,680,682,3,88,44,0,681,680,1,
    0,0,0,682,685,1,0,0,0,683,681,1,0,0,0,683,684,1,0,0,0,684,691,1,0,0,0,
    685,683,1,0,0,0,686,688,7,21,0,0,687,689,5,183,0,0,688,687,1,0,0,0,688,
    689,1,0,0,0,689,691,1,0,0,0,690,678,1,0,0,0,690,686,1,0,0,0,691,87,1,
    0,0,0,692,693,7,22,0,0,693,699,3,76,38,0,694,695,5,20,0,0,695,699,5,183,
    0,0,696,699,5,170,0,0,697,699,5,171,0,0,698,692,1,0,0,0,698,694,1,0,0,
    0,698,696,1,0,0,0,698,697,1,0,0,0,699,89,1,0,0,0,700,701,5,153,0,0,701,
    91,1,0,0,0,702,703,5,141,0,0,703,710,3,76,38,0,704,705,5,164,0,0,705,
    709,3,76,38,0,706,707,5,20,0,0,707,709,5,183,0,0,708,704,1,0,0,0,708,
    706,1,0,0,0,709,712,1,0,0,0,710,708,1,0,0,0,710,711,1,0,0,0,711,719,1,
    0,0,0,712,710,1,0,0,0,713,716,5,142,0,0,714,715,5,20,0,0,715,717,5,183,
    0,0,716,714,1,0,0,0,716,717,1,0,0,0,717,719,1,0,0,0,718,702,1,0,0,0,718,
    713,1,0,0,0,719,93,1,0,0,0,720,725,5,143,0,0,721,725,5,154,0,0,722,723,
    5,144,0,0,723,725,7,23,0,0,724,720,1,0,0,0,724,721,1,0,0,0,724,722,1,
    0,0,0,725,730,1,0,0,0,726,727,7,1,0,0,727,729,5,183,0,0,728,726,1,0,0,
    0,729,732,1,0,0,0,730,728,1,0,0,0,730,731,1,0,0,0,731,95,1,0,0,0,732,
    730,1,0,0,0,733,734,5,148,0,0,734,735,5,84,0,0,735,736,3,76,38,0,736,
    97,1,0,0,0,737,752,5,146,0,0,738,742,5,3,0,0,739,741,3,100,50,0,740,739,
    1,0,0,0,741,744,1,0,0,0,742,740,1,0,0,0,742,743,1,0,0,0,743,753,1,0,0,
    0,744,742,1,0,0,0,745,748,5,6,0,0,746,747,5,126,0,0,747,749,3,76,38,0,
    748,746,1,0,0,0,748,749,1,0,0,0,749,753,1,0,0,0,750,751,5,152,0,0,751,
    753,3,76,38,0,752,738,1,0,0,0,752,745,1,0,0,0,752,750,1,0,0,0,753,99,
    1,0,0,0,754,755,7,24,0,0,755,763,5,183,0,0,756,757,5,172,0,0,757,763,
    5,183,0,0,758,763,5,173,0,0,759,763,5,174,0,0,760,761,7,25,0,0,761,763,
    3,76,38,0,762,754,1,0,0,0,762,756,1,0,0,0,762,758,1,0,0,0,762,759,1,0,
    0,0,762,760,1,0,0,0,763,101,1,0,0,0,764,771,5,147,0,0,765,766,7,24,0,
    0,766,770,5,183,0,0,767,768,7,26,0,0,768,770,3,76,38,0,769,765,1,0,0,
    0,769,767,1,0,0,0,770,773,1,0,0,0,771,769,1,0,0,0,771,772,1,0,0,0,772,
    103,1,0,0,0,773,771,1,0,0,0,774,775,5,178,0,0,775,776,3,76,38,0,776,105,
    1,0,0,0,777,778,7,27,0,0,778,779,5,183,0,0,779,107,1,0,0,0,780,781,5,
    181,0,0,781,790,3,76,38,0,782,785,5,182,0,0,783,786,3,76,38,0,784,786,
    5,185,0,0,785,783,1,0,0,0,785,784,1,0,0,0,786,787,1,0,0,0,787,785,1,0,
    0,0,787,788,1,0,0,0,788,790,1,0,0,0,789,780,1,0,0,0,789,782,1,0,0,0,790,
    109,1,0,0,0,791,792,5,189,0,0,792,793,3,76,38,0,793,111,1,0,0,0,89,149,
    156,163,168,174,176,181,192,198,200,210,213,220,228,235,241,243,249,258,
    266,274,282,290,296,298,305,309,311,320,322,328,330,334,336,348,356,366,
    375,394,404,407,413,420,450,456,463,465,475,487,494,496,504,506,519,524,
    526,535,542,546,555,569,581,617,634,643,654,657,670,673,675,683,688,690,
    698,708,710,716,718,724,730,742,748,752,762,769,771,785,787,789
  };
  staticData->serializedATN = antlr4::atn::SerializedATNView(serializedATNSegment, sizeof(serializedATNSegment) / sizeof(serializedATNSegment[0]));

  antlr4::atn::ATNDeserializer deserializer;
  staticData->atn = deserializer.deserialize(staticData->serializedATN);

  const size_t count = staticData->atn->getNumberOfDecisions();
  staticData->decisionToDFA.reserve(count);
  for (size_t i = 0; i < count; i++) {
    staticData->decisionToDFA.emplace_back(staticData->atn->getDecisionState(i), i);
  }
  tracercommandsParserStaticData = std::move(staticData);
}

}

TracerCommandsParser::TracerCommandsParser(TokenStream *input) : TracerCommandsParser(input, antlr4::atn::ParserATNSimulatorOptions()) {}

TracerCommandsParser::TracerCommandsParser(TokenStream *input, const antlr4::atn::ParserATNSimulatorOptions &options) : Parser(input) {
  TracerCommandsParser::initialize();
  _interpreter = new atn::ParserATNSimulator(this, *tracercommandsParserStaticData->atn, tracercommandsParserStaticData->decisionToDFA, tracercommandsParserStaticData->sharedContextCache, options);
}

TracerCommandsParser::~TracerCommandsParser() {
  delete _interpreter;
}

const atn::ATN& TracerCommandsParser::getATN() const {
  return *tracercommandsParserStaticData->atn;
}

std::string TracerCommandsParser::getGrammarFileName() const {
  return "TracerCommands.g4";
}

const std::vector<std::string>& TracerCommandsParser::getRuleNames() const {
  return tracercommandsParserStaticData->ruleNames;
}

const dfa::Vocabulary& TracerCommandsParser::getVocabulary() const {
  return tracercommandsParserStaticData->vocabulary;
}

antlr4::atn::SerializedATNView TracerCommandsParser::getSerializedATN() const {
  return tracercommandsParserStaticData->serializedATN;
}


//----------------- LineContext ------------------------------------------------------------------

TracerCommandsParser::LineContext::LineContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

TracerCommandsParser::CommandContext* TracerCommandsParser::LineContext::command() {
  return getRuleContext<TracerCommandsParser::CommandContext>(0);
}

tree::TerminalNode* TracerCommandsParser::LineContext::EOF() {
  return getToken(TracerCommandsParser::EOF, 0);
}


size_t TracerCommandsParser::LineContext::getRuleIndex() const {
  return TracerCommandsParser::RuleLine;
}


std::any TracerCommandsParser::LineContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitLine(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::LineContext* TracerCommandsParser::line() {
  LineContext *_localctx = _tracker.createInstance<LineContext>(_ctx, getState());
  enterRule(_localctx, 0, TracerCommandsParser::RuleLine);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(112);
    match(TracerCommandsParser::T__0);
    setState(113);
    command();
    setState(114);
    match(TracerCommandsParser::EOF);

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- CommandContext ------------------------------------------------------------------

TracerCommandsParser::CommandContext::CommandContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

TracerCommandsParser::ServerContext* TracerCommandsParser::CommandContext::server() {
  return getRuleContext<TracerCommandsParser::ServerContext>(0);
}

TracerCommandsParser::AttachContext* TracerCommandsParser::CommandContext::attach() {
  return getRuleContext<TracerCommandsParser::AttachContext>(0);
}

TracerCommandsParser::ProcessesContext* TracerCommandsParser::CommandContext::processes() {
  return getRuleContext<TracerCommandsParser::ProcessesContext>(0);
}

TracerCommandsParser::SessionsContext* TracerCommandsParser::CommandContext::sessions() {
  return getRuleContext<TracerCommandsParser::SessionsContext>(0);
}

TracerCommandsParser::UseContext* TracerCommandsParser::CommandContext::use() {
  return getRuleContext<TracerCommandsParser::UseContext>(0);
}

TracerCommandsParser::StatusContext* TracerCommandsParser::CommandContext::status() {
  return getRuleContext<TracerCommandsParser::StatusContext>(0);
}

TracerCommandsParser::DetachContext* TracerCommandsParser::CommandContext::detach() {
  return getRuleContext<TracerCommandsParser::DetachContext>(0);
}

TracerCommandsParser::ModulesContext* TracerCommandsParser::CommandContext::modules() {
  return getRuleContext<TracerCommandsParser::ModulesContext>(0);
}

TracerCommandsParser::ModuleIndexContext* TracerCommandsParser::CommandContext::moduleIndex() {
  return getRuleContext<TracerCommandsParser::ModuleIndexContext>(0);
}

TracerCommandsParser::ThreadsContext* TracerCommandsParser::CommandContext::threads() {
  return getRuleContext<TracerCommandsParser::ThreadsContext>(0);
}

TracerCommandsParser::SymbolsContext* TracerCommandsParser::CommandContext::symbols() {
  return getRuleContext<TracerCommandsParser::SymbolsContext>(0);
}

TracerCommandsParser::ProbeContext* TracerCommandsParser::CommandContext::probe() {
  return getRuleContext<TracerCommandsParser::ProbeContext>(0);
}

TracerCommandsParser::InterceptorContext* TracerCommandsParser::CommandContext::interceptor() {
  return getRuleContext<TracerCommandsParser::InterceptorContext>(0);
}

TracerCommandsParser::EventsContext* TracerCommandsParser::CommandContext::events() {
  return getRuleContext<TracerCommandsParser::EventsContext>(0);
}

TracerCommandsParser::CollectContext* TracerCommandsParser::CommandContext::collect() {
  return getRuleContext<TracerCommandsParser::CollectContext>(0);
}

TracerCommandsParser::RecordContext* TracerCommandsParser::CommandContext::record() {
  return getRuleContext<TracerCommandsParser::RecordContext>(0);
}

TracerCommandsParser::CppContext* TracerCommandsParser::CommandContext::cpp() {
  return getRuleContext<TracerCommandsParser::CppContext>(0);
}

TracerCommandsParser::ShellContext* TracerCommandsParser::CommandContext::shell() {
  return getRuleContext<TracerCommandsParser::ShellContext>(0);
}

TracerCommandsParser::CompleteContext* TracerCommandsParser::CommandContext::complete() {
  return getRuleContext<TracerCommandsParser::CompleteContext>(0);
}

TracerCommandsParser::TypeContext* TracerCommandsParser::CommandContext::type() {
  return getRuleContext<TracerCommandsParser::TypeContext>(0);
}

TracerCommandsParser::HelpContext* TracerCommandsParser::CommandContext::help() {
  return getRuleContext<TracerCommandsParser::HelpContext>(0);
}

TracerCommandsParser::QuitContext* TracerCommandsParser::CommandContext::quit() {
  return getRuleContext<TracerCommandsParser::QuitContext>(0);
}

TracerCommandsParser::BulkTraceContext* TracerCommandsParser::CommandContext::bulkTrace() {
  return getRuleContext<TracerCommandsParser::BulkTraceContext>(0);
}

TracerCommandsParser::MemoryContext* TracerCommandsParser::CommandContext::memory() {
  return getRuleContext<TracerCommandsParser::MemoryContext>(0);
}

TracerCommandsParser::ProbeEntryContext* TracerCommandsParser::CommandContext::probeEntry() {
  return getRuleContext<TracerCommandsParser::ProbeEntryContext>(0);
}

TracerCommandsParser::ProbeReloadContext* TracerCommandsParser::CommandContext::probeReload() {
  return getRuleContext<TracerCommandsParser::ProbeReloadContext>(0);
}

TracerCommandsParser::TargetLaunchContext* TracerCommandsParser::CommandContext::targetLaunch() {
  return getRuleContext<TracerCommandsParser::TargetLaunchContext>(0);
}

TracerCommandsParser::TargetCallContext* TracerCommandsParser::CommandContext::targetCall() {
  return getRuleContext<TracerCommandsParser::TargetCallContext>(0);
}

TracerCommandsParser::LocalPerfContext* TracerCommandsParser::CommandContext::localPerf() {
  return getRuleContext<TracerCommandsParser::LocalPerfContext>(0);
}

TracerCommandsParser::InstructionsContext* TracerCommandsParser::CommandContext::instructions() {
  return getRuleContext<TracerCommandsParser::InstructionsContext>(0);
}

TracerCommandsParser::WatchpointContext* TracerCommandsParser::CommandContext::watchpoint() {
  return getRuleContext<TracerCommandsParser::WatchpointContext>(0);
}

TracerCommandsParser::TargetLibraryContext* TracerCommandsParser::CommandContext::targetLibrary() {
  return getRuleContext<TracerCommandsParser::TargetLibraryContext>(0);
}

TracerCommandsParser::ChildListContext* TracerCommandsParser::CommandContext::childList() {
  return getRuleContext<TracerCommandsParser::ChildListContext>(0);
}


size_t TracerCommandsParser::CommandContext::getRuleIndex() const {
  return TracerCommandsParser::RuleCommand;
}


std::any TracerCommandsParser::CommandContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitCommand(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::CommandContext* TracerCommandsParser::command() {
  CommandContext *_localctx = _tracker.createInstance<CommandContext>(_ctx, getState());
  enterRule(_localctx, 2, TracerCommandsParser::RuleCommand);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(149);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 0, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(116);
      server();
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(117);
      attach();
      break;
    }

    case 3: {
      enterOuterAlt(_localctx, 3);
      setState(118);
      processes();
      break;
    }

    case 4: {
      enterOuterAlt(_localctx, 4);
      setState(119);
      sessions();
      break;
    }

    case 5: {
      enterOuterAlt(_localctx, 5);
      setState(120);
      use();
      break;
    }

    case 6: {
      enterOuterAlt(_localctx, 6);
      setState(121);
      status();
      break;
    }

    case 7: {
      enterOuterAlt(_localctx, 7);
      setState(122);
      detach();
      break;
    }

    case 8: {
      enterOuterAlt(_localctx, 8);
      setState(123);
      modules();
      break;
    }

    case 9: {
      enterOuterAlt(_localctx, 9);
      setState(124);
      moduleIndex();
      break;
    }

    case 10: {
      enterOuterAlt(_localctx, 10);
      setState(125);
      threads();
      break;
    }

    case 11: {
      enterOuterAlt(_localctx, 11);
      setState(126);
      symbols();
      break;
    }

    case 12: {
      enterOuterAlt(_localctx, 12);
      setState(127);
      probe();
      break;
    }

    case 13: {
      enterOuterAlt(_localctx, 13);
      setState(128);
      interceptor();
      break;
    }

    case 14: {
      enterOuterAlt(_localctx, 14);
      setState(129);
      events();
      break;
    }

    case 15: {
      enterOuterAlt(_localctx, 15);
      setState(130);
      collect();
      break;
    }

    case 16: {
      enterOuterAlt(_localctx, 16);
      setState(131);
      record();
      break;
    }

    case 17: {
      enterOuterAlt(_localctx, 17);
      setState(132);
      cpp();
      break;
    }

    case 18: {
      enterOuterAlt(_localctx, 18);
      setState(133);
      shell();
      break;
    }

    case 19: {
      enterOuterAlt(_localctx, 19);
      setState(134);
      complete();
      break;
    }

    case 20: {
      enterOuterAlt(_localctx, 20);
      setState(135);
      type();
      break;
    }

    case 21: {
      enterOuterAlt(_localctx, 21);
      setState(136);
      help();
      break;
    }

    case 22: {
      enterOuterAlt(_localctx, 22);
      setState(137);
      quit();
      break;
    }

    case 23: {
      enterOuterAlt(_localctx, 23);
      setState(138);
      bulkTrace();
      break;
    }

    case 24: {
      enterOuterAlt(_localctx, 24);
      setState(139);
      memory();
      break;
    }

    case 25: {
      enterOuterAlt(_localctx, 25);
      setState(140);
      probeEntry();
      break;
    }

    case 26: {
      enterOuterAlt(_localctx, 26);
      setState(141);
      probeReload();
      break;
    }

    case 27: {
      enterOuterAlt(_localctx, 27);
      setState(142);
      targetLaunch();
      break;
    }

    case 28: {
      enterOuterAlt(_localctx, 28);
      setState(143);
      targetCall();
      break;
    }

    case 29: {
      enterOuterAlt(_localctx, 29);
      setState(144);
      localPerf();
      break;
    }

    case 30: {
      enterOuterAlt(_localctx, 30);
      setState(145);
      instructions();
      break;
    }

    case 31: {
      enterOuterAlt(_localctx, 31);
      setState(146);
      watchpoint();
      break;
    }

    case 32: {
      enterOuterAlt(_localctx, 32);
      setState(147);
      targetLibrary();
      break;
    }

    case 33: {
      enterOuterAlt(_localctx, 33);
      setState(148);
      childList();
      break;
    }

    default:
      break;
    }

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ServerContext ------------------------------------------------------------------

TracerCommandsParser::ServerContext::ServerContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

TracerCommandsParser::ValueContext* TracerCommandsParser::ServerContext::value() {
  return getRuleContext<TracerCommandsParser::ValueContext>(0);
}

std::vector<TracerCommandsParser::ServerOptionContext *> TracerCommandsParser::ServerContext::serverOption() {
  return getRuleContexts<TracerCommandsParser::ServerOptionContext>();
}

TracerCommandsParser::ServerOptionContext* TracerCommandsParser::ServerContext::serverOption(size_t i) {
  return getRuleContext<TracerCommandsParser::ServerOptionContext>(i);
}


size_t TracerCommandsParser::ServerContext::getRuleIndex() const {
  return TracerCommandsParser::RuleServer;
}


std::any TracerCommandsParser::ServerContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitServer(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::ServerContext* TracerCommandsParser::server() {
  ServerContext *_localctx = _tracker.createInstance<ServerContext>(_ctx, getState());
  enterRule(_localctx, 4, TracerCommandsParser::RuleServer);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(151);
    match(TracerCommandsParser::T__1);
    setState(163);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__2: {
        setState(152);
        match(TracerCommandsParser::T__2);
        setState(156);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__6

        || _la == TracerCommandsParser::T__7 || _la == TracerCommandsParser::T__178

        || _la == TracerCommandsParser::T__179) {
          setState(153);
          serverOption();
          setState(158);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__3: {
        setState(159);
        match(TracerCommandsParser::T__3);
        setState(160);
        value();
        break;
      }

      case TracerCommandsParser::T__4: {
        setState(161);
        match(TracerCommandsParser::T__4);
        break;
      }

      case TracerCommandsParser::T__5: {
        setState(162);
        match(TracerCommandsParser::T__5);
        break;
      }

    default:
      throw NoViableAltException(this);
    }

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ServerOptionContext ------------------------------------------------------------------

TracerCommandsParser::ServerOptionContext::ServerOptionContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

TracerCommandsParser::ValueContext* TracerCommandsParser::ServerOptionContext::value() {
  return getRuleContext<TracerCommandsParser::ValueContext>(0);
}

TracerCommandsParser::ContextEventBytesContext* TracerCommandsParser::ServerOptionContext::contextEventBytes() {
  return getRuleContext<TracerCommandsParser::ContextEventBytesContext>(0);
}


size_t TracerCommandsParser::ServerOptionContext::getRuleIndex() const {
  return TracerCommandsParser::RuleServerOption;
}


std::any TracerCommandsParser::ServerOptionContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitServerOption(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::ServerOptionContext* TracerCommandsParser::serverOption() {
  ServerOptionContext *_localctx = _tracker.createInstance<ServerOptionContext>(_ctx, getState());
  enterRule(_localctx, 6, TracerCommandsParser::RuleServerOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(168);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__6:
      case TracerCommandsParser::T__7: {
        enterOuterAlt(_localctx, 1);
        setState(165);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__6

        || _la == TracerCommandsParser::T__7)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(166);
        value();
        break;
      }

      case TracerCommandsParser::T__178:
      case TracerCommandsParser::T__179: {
        enterOuterAlt(_localctx, 2);
        setState(167);
        contextEventBytes();
        break;
      }

    default:
      throw NoViableAltException(this);
    }

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- AttachContext ------------------------------------------------------------------

TracerCommandsParser::AttachContext::AttachContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<tree::TerminalNode *> TracerCommandsParser::AttachContext::UINT() {
  return getTokens(TracerCommandsParser::UINT);
}

tree::TerminalNode* TracerCommandsParser::AttachContext::UINT(size_t i) {
  return getToken(TracerCommandsParser::UINT, i);
}


size_t TracerCommandsParser::AttachContext::getRuleIndex() const {
  return TracerCommandsParser::RuleAttach;
}


std::any TracerCommandsParser::AttachContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitAttach(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::AttachContext* TracerCommandsParser::attach() {
  AttachContext *_localctx = _tracker.createInstance<AttachContext>(_ctx, getState());
  enterRule(_localctx, 8, TracerCommandsParser::RuleAttach);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(170);
    match(TracerCommandsParser::T__8);
    setState(176);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::UINT) {
      setState(171);
      match(TracerCommandsParser::UINT);
      setState(174);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == TracerCommandsParser::T__9) {
        setState(172);
        match(TracerCommandsParser::T__9);
        setState(173);
        match(TracerCommandsParser::UINT);
      }
    }

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ProcessesContext ------------------------------------------------------------------

TracerCommandsParser::ProcessesContext::ProcessesContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

TracerCommandsParser::ValueContext* TracerCommandsParser::ProcessesContext::value() {
  return getRuleContext<TracerCommandsParser::ValueContext>(0);
}


size_t TracerCommandsParser::ProcessesContext::getRuleIndex() const {
  return TracerCommandsParser::RuleProcesses;
}


std::any TracerCommandsParser::ProcessesContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitProcesses(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::ProcessesContext* TracerCommandsParser::processes() {
  ProcessesContext *_localctx = _tracker.createInstance<ProcessesContext>(_ctx, getState());
  enterRule(_localctx, 10, TracerCommandsParser::RuleProcesses);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(178);
    match(TracerCommandsParser::T__10);
    setState(181);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::T__11) {
      setState(179);
      match(TracerCommandsParser::T__11);
      setState(180);
      value();
    }

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- SessionsContext ------------------------------------------------------------------

TracerCommandsParser::SessionsContext::SessionsContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}


size_t TracerCommandsParser::SessionsContext::getRuleIndex() const {
  return TracerCommandsParser::RuleSessions;
}


std::any TracerCommandsParser::SessionsContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitSessions(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::SessionsContext* TracerCommandsParser::sessions() {
  SessionsContext *_localctx = _tracker.createInstance<SessionsContext>(_ctx, getState());
  enterRule(_localctx, 12, TracerCommandsParser::RuleSessions);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(183);
    match(TracerCommandsParser::T__12);

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- UseContext ------------------------------------------------------------------

TracerCommandsParser::UseContext::UseContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

TracerCommandsParser::ValueContext* TracerCommandsParser::UseContext::value() {
  return getRuleContext<TracerCommandsParser::ValueContext>(0);
}


size_t TracerCommandsParser::UseContext::getRuleIndex() const {
  return TracerCommandsParser::RuleUse;
}


std::any TracerCommandsParser::UseContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitUse(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::UseContext* TracerCommandsParser::use() {
  UseContext *_localctx = _tracker.createInstance<UseContext>(_ctx, getState());
  enterRule(_localctx, 14, TracerCommandsParser::RuleUse);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(185);
    match(TracerCommandsParser::T__13);
    setState(186);
    value();

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- StatusContext ------------------------------------------------------------------

TracerCommandsParser::StatusContext::StatusContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}


size_t TracerCommandsParser::StatusContext::getRuleIndex() const {
  return TracerCommandsParser::RuleStatus;
}


std::any TracerCommandsParser::StatusContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitStatus(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::StatusContext* TracerCommandsParser::status() {
  StatusContext *_localctx = _tracker.createInstance<StatusContext>(_ctx, getState());
  enterRule(_localctx, 16, TracerCommandsParser::RuleStatus);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(188);
    match(TracerCommandsParser::T__4);

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- DetachContext ------------------------------------------------------------------

TracerCommandsParser::DetachContext::DetachContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

TracerCommandsParser::ValueContext* TracerCommandsParser::DetachContext::value() {
  return getRuleContext<TracerCommandsParser::ValueContext>(0);
}


size_t TracerCommandsParser::DetachContext::getRuleIndex() const {
  return TracerCommandsParser::RuleDetach;
}


std::any TracerCommandsParser::DetachContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitDetach(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::DetachContext* TracerCommandsParser::detach() {
  DetachContext *_localctx = _tracker.createInstance<DetachContext>(_ctx, getState());
  enterRule(_localctx, 18, TracerCommandsParser::RuleDetach);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(190);
    match(TracerCommandsParser::T__14);
    setState(192);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 33654401923148412) != 0) || ((((_la - 83) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 83)) & -16492689326101) != 0) || ((((_la - 147) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 147)) & 72773925863935) != 0)) {
      setState(191);
      value();
    }

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ModulesContext ------------------------------------------------------------------

TracerCommandsParser::ModulesContext::ModulesContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<TracerCommandsParser::ValueContext *> TracerCommandsParser::ModulesContext::value() {
  return getRuleContexts<TracerCommandsParser::ValueContext>();
}

TracerCommandsParser::ValueContext* TracerCommandsParser::ModulesContext::value(size_t i) {
  return getRuleContext<TracerCommandsParser::ValueContext>(i);
}

std::vector<tree::TerminalNode *> TracerCommandsParser::ModulesContext::UINT() {
  return getTokens(TracerCommandsParser::UINT);
}

tree::TerminalNode* TracerCommandsParser::ModulesContext::UINT(size_t i) {
  return getToken(TracerCommandsParser::UINT, i);
}


size_t TracerCommandsParser::ModulesContext::getRuleIndex() const {
  return TracerCommandsParser::RuleModules;
}


std::any TracerCommandsParser::ModulesContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitModules(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::ModulesContext* TracerCommandsParser::modules() {
  ModulesContext *_localctx = _tracker.createInstance<ModulesContext>(_ctx, getState());
  enterRule(_localctx, 20, TracerCommandsParser::RuleModules);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(194);
    match(TracerCommandsParser::T__15);
    setState(213);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::EOF:
      case TracerCommandsParser::T__11:
      case TracerCommandsParser::T__16: {
        setState(200);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__11

        || _la == TracerCommandsParser::T__16) {
          setState(198);
          _errHandler->sync(this);
          switch (_input->LA(1)) {
            case TracerCommandsParser::T__16: {
              setState(195);
              match(TracerCommandsParser::T__16);
              break;
            }

            case TracerCommandsParser::T__11: {
              setState(196);
              match(TracerCommandsParser::T__11);
              setState(197);
              value();
              break;
            }

          default:
            throw NoViableAltException(this);
          }
          setState(202);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__17: {
        setState(203);
        match(TracerCommandsParser::T__17);
        setState(204);
        value();
        break;
      }

      case TracerCommandsParser::T__18: {
        setState(205);
        match(TracerCommandsParser::T__18);
        setState(210);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__19

        || _la == TracerCommandsParser::T__20) {
          setState(206);
          _la = _input->LA(1);
          if (!(_la == TracerCommandsParser::T__19

          || _la == TracerCommandsParser::T__20)) {
          _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(207);
          match(TracerCommandsParser::UINT);
          setState(212);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

    default:
      throw NoViableAltException(this);
    }

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ModuleIndexContext ------------------------------------------------------------------

TracerCommandsParser::ModuleIndexContext::ModuleIndexContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

TracerCommandsParser::ValueContext* TracerCommandsParser::ModuleIndexContext::value() {
  return getRuleContext<TracerCommandsParser::ValueContext>(0);
}

std::vector<TracerCommandsParser::ModuleBuildOptionContext *> TracerCommandsParser::ModuleIndexContext::moduleBuildOption() {
  return getRuleContexts<TracerCommandsParser::ModuleBuildOptionContext>();
}

TracerCommandsParser::ModuleBuildOptionContext* TracerCommandsParser::ModuleIndexContext::moduleBuildOption(size_t i) {
  return getRuleContext<TracerCommandsParser::ModuleBuildOptionContext>(i);
}

std::vector<TracerCommandsParser::ModuleQueryOptionContext *> TracerCommandsParser::ModuleIndexContext::moduleQueryOption() {
  return getRuleContexts<TracerCommandsParser::ModuleQueryOptionContext>();
}

TracerCommandsParser::ModuleQueryOptionContext* TracerCommandsParser::ModuleIndexContext::moduleQueryOption(size_t i) {
  return getRuleContext<TracerCommandsParser::ModuleQueryOptionContext>(i);
}

std::vector<TracerCommandsParser::ModuleShowOptionContext *> TracerCommandsParser::ModuleIndexContext::moduleShowOption() {
  return getRuleContexts<TracerCommandsParser::ModuleShowOptionContext>();
}

TracerCommandsParser::ModuleShowOptionContext* TracerCommandsParser::ModuleIndexContext::moduleShowOption(size_t i) {
  return getRuleContext<TracerCommandsParser::ModuleShowOptionContext>(i);
}

std::vector<TracerCommandsParser::ModuleEmitOptionContext *> TracerCommandsParser::ModuleIndexContext::moduleEmitOption() {
  return getRuleContexts<TracerCommandsParser::ModuleEmitOptionContext>();
}

TracerCommandsParser::ModuleEmitOptionContext* TracerCommandsParser::ModuleIndexContext::moduleEmitOption(size_t i) {
  return getRuleContext<TracerCommandsParser::ModuleEmitOptionContext>(i);
}


size_t TracerCommandsParser::ModuleIndexContext::getRuleIndex() const {
  return TracerCommandsParser::RuleModuleIndex;
}


std::any TracerCommandsParser::ModuleIndexContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitModuleIndex(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::ModuleIndexContext* TracerCommandsParser::moduleIndex() {
  ModuleIndexContext *_localctx = _tracker.createInstance<ModuleIndexContext>(_ctx, getState());
  enterRule(_localctx, 22, TracerCommandsParser::RuleModuleIndex);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(215);
    match(TracerCommandsParser::T__21);
    setState(243);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__22: {
        setState(216);
        match(TracerCommandsParser::T__22);
        setState(218);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(217);
          moduleBuildOption();
          setState(220);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 34091302912) != 0));
        break;
      }

      case TracerCommandsParser::T__13: {
        setState(222);
        match(TracerCommandsParser::T__13);
        setState(223);
        value();
        break;
      }

      case TracerCommandsParser::T__23:
      case TracerCommandsParser::T__24: {
        setState(224);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__23

        || _la == TracerCommandsParser::T__24)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(228);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 1614907707392) != 0)) {
          setState(225);
          moduleQueryOption();
          setState(230);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__25: {
        setState(231);
        match(TracerCommandsParser::T__25);
        setState(233);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(232);
          moduleShowOption();
          setState(235);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 15427522527232) != 0));
        break;
      }

      case TracerCommandsParser::T__26: {
        setState(237);
        match(TracerCommandsParser::T__26);
        setState(239);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(238);
          moduleEmitOption();
          setState(241);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 15429670010880) != 0));
        break;
      }

    default:
      throw NoViableAltException(this);
    }

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ModuleBuildOptionContext ------------------------------------------------------------------

TracerCommandsParser::ModuleBuildOptionContext::ModuleBuildOptionContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

TracerCommandsParser::ValueContext* TracerCommandsParser::ModuleBuildOptionContext::value() {
  return getRuleContext<TracerCommandsParser::ValueContext>(0);
}

tree::TerminalNode* TracerCommandsParser::ModuleBuildOptionContext::UINT() {
  return getToken(TracerCommandsParser::UINT, 0);
}


size_t TracerCommandsParser::ModuleBuildOptionContext::getRuleIndex() const {
  return TracerCommandsParser::RuleModuleBuildOption;
}


std::any TracerCommandsParser::ModuleBuildOptionContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitModuleBuildOption(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::ModuleBuildOptionContext* TracerCommandsParser::moduleBuildOption() {
  ModuleBuildOptionContext *_localctx = _tracker.createInstance<ModuleBuildOptionContext>(_ctx, getState());
  enterRule(_localctx, 24, TracerCommandsParser::RuleModuleBuildOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(249);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__27:
      case TracerCommandsParser::T__28:
      case TracerCommandsParser::T__29:
      case TracerCommandsParser::T__30:
      case TracerCommandsParser::T__31: {
        enterOuterAlt(_localctx, 1);
        setState(245);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 8321499136) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(246);
        value();
        break;
      }

      case TracerCommandsParser::T__32:
      case TracerCommandsParser::T__33: {
        enterOuterAlt(_localctx, 2);
        setState(247);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__32

        || _la == TracerCommandsParser::T__33)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(248);
        match(TracerCommandsParser::UINT);
        break;
      }

    default:
      throw NoViableAltException(this);
    }

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ModuleQueryOptionContext ------------------------------------------------------------------

TracerCommandsParser::ModuleQueryOptionContext::ModuleQueryOptionContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

TracerCommandsParser::ValueContext* TracerCommandsParser::ModuleQueryOptionContext::value() {
  return getRuleContext<TracerCommandsParser::ValueContext>(0);
}

tree::TerminalNode* TracerCommandsParser::ModuleQueryOptionContext::UINT() {
  return getToken(TracerCommandsParser::UINT, 0);
}


size_t TracerCommandsParser::ModuleQueryOptionContext::getRuleIndex() const {
  return TracerCommandsParser::RuleModuleQueryOption;
}


std::any TracerCommandsParser::ModuleQueryOptionContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitModuleQueryOption(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::ModuleQueryOptionContext* TracerCommandsParser::moduleQueryOption() {
  ModuleQueryOptionContext *_localctx = _tracker.createInstance<ModuleQueryOptionContext>(_ctx, getState());
  enterRule(_localctx, 26, TracerCommandsParser::RuleModuleQueryOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(258);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__11:
      case TracerCommandsParser::T__34:
      case TracerCommandsParser::T__35: {
        enterOuterAlt(_localctx, 1);
        setState(251);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 103079219200) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(252);
        value();
        break;
      }

      case TracerCommandsParser::T__36: {
        enterOuterAlt(_localctx, 2);
        setState(253);
        match(TracerCommandsParser::T__36);
        setState(254);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__37: {
        enterOuterAlt(_localctx, 3);
        setState(255);
        match(TracerCommandsParser::T__37);
        setState(256);
        match(TracerCommandsParser::T__38);
        break;
      }

      case TracerCommandsParser::T__39: {
        enterOuterAlt(_localctx, 4);
        setState(257);
        match(TracerCommandsParser::T__39);
        break;
      }

    default:
      throw NoViableAltException(this);
    }

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ModuleShowOptionContext ------------------------------------------------------------------

TracerCommandsParser::ModuleShowOptionContext::ModuleShowOptionContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

TracerCommandsParser::ValueContext* TracerCommandsParser::ModuleShowOptionContext::value() {
  return getRuleContext<TracerCommandsParser::ValueContext>(0);
}


size_t TracerCommandsParser::ModuleShowOptionContext::getRuleIndex() const {
  return TracerCommandsParser::RuleModuleShowOption;
}


std::any TracerCommandsParser::ModuleShowOptionContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitModuleShowOption(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::ModuleShowOptionContext* TracerCommandsParser::moduleShowOption() {
  ModuleShowOptionContext *_localctx = _tracker.createInstance<ModuleShowOptionContext>(_ctx, getState());
  enterRule(_localctx, 28, TracerCommandsParser::RuleModuleShowOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(260);
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 15427522527232) != 0))) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(261);
    value();

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ModuleEmitOptionContext ------------------------------------------------------------------

TracerCommandsParser::ModuleEmitOptionContext::ModuleEmitOptionContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

TracerCommandsParser::ModuleShowOptionContext* TracerCommandsParser::ModuleEmitOptionContext::moduleShowOption() {
  return getRuleContext<TracerCommandsParser::ModuleShowOptionContext>(0);
}

TracerCommandsParser::ValueContext* TracerCommandsParser::ModuleEmitOptionContext::value() {
  return getRuleContext<TracerCommandsParser::ValueContext>(0);
}


size_t TracerCommandsParser::ModuleEmitOptionContext::getRuleIndex() const {
  return TracerCommandsParser::RuleModuleEmitOption;
}


std::any TracerCommandsParser::ModuleEmitOptionContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitModuleEmitOption(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::ModuleEmitOptionContext* TracerCommandsParser::moduleEmitOption() {
  ModuleEmitOptionContext *_localctx = _tracker.createInstance<ModuleEmitOptionContext>(_ctx, getState());
  enterRule(_localctx, 30, TracerCommandsParser::RuleModuleEmitOption);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(266);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__34:
      case TracerCommandsParser::T__40:
      case TracerCommandsParser::T__41:
      case TracerCommandsParser::T__42: {
        enterOuterAlt(_localctx, 1);
        setState(263);
        moduleShowOption();
        break;
      }

      case TracerCommandsParser::T__30: {
        enterOuterAlt(_localctx, 2);
        setState(264);
        match(TracerCommandsParser::T__30);
        setState(265);
        value();
        break;
      }

    default:
      throw NoViableAltException(this);
    }

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ThreadsContext ------------------------------------------------------------------

TracerCommandsParser::ThreadsContext::ThreadsContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}


size_t TracerCommandsParser::ThreadsContext::getRuleIndex() const {
  return TracerCommandsParser::RuleThreads;
}


std::any TracerCommandsParser::ThreadsContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitThreads(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::ThreadsContext* TracerCommandsParser::threads() {
  ThreadsContext *_localctx = _tracker.createInstance<ThreadsContext>(_ctx, getState());
  enterRule(_localctx, 32, TracerCommandsParser::RuleThreads);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(268);
    match(TracerCommandsParser::T__43);

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- SymbolsContext ------------------------------------------------------------------

TracerCommandsParser::SymbolsContext::SymbolsContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<TracerCommandsParser::SymbolOptionContext *> TracerCommandsParser::SymbolsContext::symbolOption() {
  return getRuleContexts<TracerCommandsParser::SymbolOptionContext>();
}

TracerCommandsParser::SymbolOptionContext* TracerCommandsParser::SymbolsContext::symbolOption(size_t i) {
  return getRuleContext<TracerCommandsParser::SymbolOptionContext>(i);
}


size_t TracerCommandsParser::SymbolsContext::getRuleIndex() const {
  return TracerCommandsParser::RuleSymbols;
}


std::any TracerCommandsParser::SymbolsContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitSymbols(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::SymbolsContext* TracerCommandsParser::symbols() {
  SymbolsContext *_localctx = _tracker.createInstance<SymbolsContext>(_ctx, getState());
  enterRule(_localctx, 34, TracerCommandsParser::RuleSymbols);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(270);
    match(TracerCommandsParser::T__24);
    setState(274);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 105760885313536) != 0)) {
      setState(271);
      symbolOption();
      setState(276);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- SymbolOptionContext ------------------------------------------------------------------

TracerCommandsParser::SymbolOptionContext::SymbolOptionContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

TracerCommandsParser::ValueContext* TracerCommandsParser::SymbolOptionContext::value() {
  return getRuleContext<TracerCommandsParser::ValueContext>(0);
}

tree::TerminalNode* TracerCommandsParser::SymbolOptionContext::UINT() {
  return getToken(TracerCommandsParser::UINT, 0);
}


size_t TracerCommandsParser::SymbolOptionContext::getRuleIndex() const {
  return TracerCommandsParser::RuleSymbolOption;
}


std::any TracerCommandsParser::SymbolOptionContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitSymbolOption(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::SymbolOptionContext* TracerCommandsParser::symbolOption() {
  SymbolOptionContext *_localctx = _tracker.createInstance<SymbolOptionContext>(_ctx, getState());
  enterRule(_localctx, 36, TracerCommandsParser::RuleSymbolOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(282);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__11:
      case TracerCommandsParser::T__28:
      case TracerCommandsParser::T__29:
      case TracerCommandsParser::T__35:
      case TracerCommandsParser::T__44: {
        enterOuterAlt(_localctx, 1);
        setState(277);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 35254702182400) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(278);
        value();
        break;
      }

      case TracerCommandsParser::T__36: {
        enterOuterAlt(_localctx, 2);
        setState(279);
        match(TracerCommandsParser::T__36);
        setState(280);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__45: {
        enterOuterAlt(_localctx, 3);
        setState(281);
        match(TracerCommandsParser::T__45);
        break;
      }

    default:
      throw NoViableAltException(this);
    }

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ProbeContext ------------------------------------------------------------------

TracerCommandsParser::ProbeContext::ProbeContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<TracerCommandsParser::ValueContext *> TracerCommandsParser::ProbeContext::value() {
  return getRuleContexts<TracerCommandsParser::ValueContext>();
}

TracerCommandsParser::ValueContext* TracerCommandsParser::ProbeContext::value(size_t i) {
  return getRuleContext<TracerCommandsParser::ValueContext>(i);
}

tree::TerminalNode* TracerCommandsParser::ProbeContext::UINT() {
  return getToken(TracerCommandsParser::UINT, 0);
}

std::vector<TracerCommandsParser::PrepareOptionContext *> TracerCommandsParser::ProbeContext::prepareOption() {
  return getRuleContexts<TracerCommandsParser::PrepareOptionContext>();
}

TracerCommandsParser::PrepareOptionContext* TracerCommandsParser::ProbeContext::prepareOption(size_t i) {
  return getRuleContext<TracerCommandsParser::PrepareOptionContext>(i);
}

std::vector<TracerCommandsParser::ResolveOptionContext *> TracerCommandsParser::ProbeContext::resolveOption() {
  return getRuleContexts<TracerCommandsParser::ResolveOptionContext>();
}

TracerCommandsParser::ResolveOptionContext* TracerCommandsParser::ProbeContext::resolveOption(size_t i) {
  return getRuleContext<TracerCommandsParser::ResolveOptionContext>(i);
}

std::vector<TracerCommandsParser::CwdOptionContext *> TracerCommandsParser::ProbeContext::cwdOption() {
  return getRuleContexts<TracerCommandsParser::CwdOptionContext>();
}

TracerCommandsParser::CwdOptionContext* TracerCommandsParser::ProbeContext::cwdOption(size_t i) {
  return getRuleContext<TracerCommandsParser::CwdOptionContext>(i);
}

std::vector<TracerCommandsParser::DeployOptionContext *> TracerCommandsParser::ProbeContext::deployOption() {
  return getRuleContexts<TracerCommandsParser::DeployOptionContext>();
}

TracerCommandsParser::DeployOptionContext* TracerCommandsParser::ProbeContext::deployOption(size_t i) {
  return getRuleContext<TracerCommandsParser::DeployOptionContext>(i);
}

std::vector<TracerCommandsParser::ValidateOptionContext *> TracerCommandsParser::ProbeContext::validateOption() {
  return getRuleContexts<TracerCommandsParser::ValidateOptionContext>();
}

TracerCommandsParser::ValidateOptionContext* TracerCommandsParser::ProbeContext::validateOption(size_t i) {
  return getRuleContext<TracerCommandsParser::ValidateOptionContext>(i);
}


size_t TracerCommandsParser::ProbeContext::getRuleIndex() const {
  return TracerCommandsParser::RuleProbe;
}


std::any TracerCommandsParser::ProbeContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitProbe(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::ProbeContext* TracerCommandsParser::probe() {
  ProbeContext *_localctx = _tracker.createInstance<ProbeContext>(_ctx, getState());
  enterRule(_localctx, 38, TracerCommandsParser::RuleProbe);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(336);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__46: {
        enterOuterAlt(_localctx, 1);
        setState(284);
        match(TracerCommandsParser::T__46);
        setState(330);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__47: {
            setState(285);
            match(TracerCommandsParser::T__47);
            setState(286);
            value();
            setState(288);
            _errHandler->sync(this);
            _la = _input->LA(1);
            do {
              setState(287);
              prepareOption();
              setState(290);
              _errHandler->sync(this);
              _la = _input->LA(1);
            } while (((((_la - 29) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 29)) & 4397979418629) != 0) || ((((_la - 181) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 181)) & 1827) != 0));
            break;
          }

          case TracerCommandsParser::T__48: {
            setState(292);
            match(TracerCommandsParser::T__48);
            setState(293);
            value();
            setState(298);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while (((((_la - 29) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 29)) & 30786795339777) != 0) || ((((_la - 181) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 181)) & 259) != 0)) {
              setState(296);
              _errHandler->sync(this);
              switch (_input->LA(1)) {
                case TracerCommandsParser::T__28:
                case TracerCommandsParser::T__54:
                case TracerCommandsParser::T__55:
                case TracerCommandsParser::T__56:
                case TracerCommandsParser::T__70:
                case TracerCommandsParser::T__71:
                case TracerCommandsParser::T__72:
                case TracerCommandsParser::T__180:
                case TracerCommandsParser::T__181: {
                  setState(294);
                  resolveOption();
                  break;
                }

                case TracerCommandsParser::CWD: {
                  setState(295);
                  cwdOption();
                  break;
                }

              default:
                throw NoViableAltException(this);
              }
              setState(300);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__49: {
            setState(301);
            match(TracerCommandsParser::T__49);
            setState(302);
            value();
            setState(305);
            _errHandler->sync(this);

            _la = _input->LA(1);
            if (_la == TracerCommandsParser::T__50) {
              setState(303);
              match(TracerCommandsParser::T__50);
              setState(304);
              value();
            }
            setState(311);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while (((((_la - 29) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 29)) & 18014394684276737) != 0) || ((((_la - 178) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 178)) & 18457) != 0)) {
              setState(309);
              _errHandler->sync(this);
              switch (_input->LA(1)) {
                case TracerCommandsParser::T__28:
                case TracerCommandsParser::T__54:
                case TracerCommandsParser::T__55:
                case TracerCommandsParser::T__56:
                case TracerCommandsParser::T__60:
                case TracerCommandsParser::T__61:
                case TracerCommandsParser::T__62:
                case TracerCommandsParser::T__63:
                case TracerCommandsParser::T__64:
                case TracerCommandsParser::T__65:
                case TracerCommandsParser::T__66:
                case TracerCommandsParser::T__67:
                case TracerCommandsParser::T__68:
                case TracerCommandsParser::T__69:
                case TracerCommandsParser::T__70:
                case TracerCommandsParser::T__71:
                case TracerCommandsParser::T__72:
                case TracerCommandsParser::T__73:
                case TracerCommandsParser::T__74:
                case TracerCommandsParser::T__75:
                case TracerCommandsParser::T__76:
                case TracerCommandsParser::T__77:
                case TracerCommandsParser::T__78:
                case TracerCommandsParser::T__79:
                case TracerCommandsParser::T__80:
                case TracerCommandsParser::T__81:
                case TracerCommandsParser::T__177:
                case TracerCommandsParser::T__180:
                case TracerCommandsParser::T__181:
                case TracerCommandsParser::SNAPSHOT_RETURN_POINTEE: {
                  setState(307);
                  deployOption();
                  break;
                }

                case TracerCommandsParser::CWD: {
                  setState(308);
                  cwdOption();
                  break;
                }

              default:
                throw NoViableAltException(this);
              }
              setState(313);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__51: {
            setState(314);
            match(TracerCommandsParser::T__51);
            setState(315);
            value();
            setState(316);
            match(TracerCommandsParser::T__50);
            setState(317);
            value();
            setState(322);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while (((((_la - 29) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 29)) & 2251795988480001) != 0) || ((((_la - 181) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 181)) & 259) != 0)) {
              setState(320);
              _errHandler->sync(this);
              switch (_input->LA(1)) {
                case TracerCommandsParser::T__28:
                case TracerCommandsParser::T__54:
                case TracerCommandsParser::T__55:
                case TracerCommandsParser::T__56:
                case TracerCommandsParser::T__60:
                case TracerCommandsParser::T__61:
                case TracerCommandsParser::T__62:
                case TracerCommandsParser::T__63:
                case TracerCommandsParser::T__64:
                case TracerCommandsParser::T__65:
                case TracerCommandsParser::T__66:
                case TracerCommandsParser::T__67:
                case TracerCommandsParser::T__68:
                case TracerCommandsParser::T__69:
                case TracerCommandsParser::T__70:
                case TracerCommandsParser::T__71:
                case TracerCommandsParser::T__72:
                case TracerCommandsParser::T__73:
                case TracerCommandsParser::T__74:
                case TracerCommandsParser::T__75:
                case TracerCommandsParser::T__76:
                case TracerCommandsParser::T__77:
                case TracerCommandsParser::T__78:
                case TracerCommandsParser::T__180:
                case TracerCommandsParser::T__181: {
                  setState(318);
                  validateOption();
                  break;
                }

                case TracerCommandsParser::CWD: {
                  setState(319);
                  cwdOption();
                  break;
                }

              default:
                throw NoViableAltException(this);
              }
              setState(324);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__52: {
            setState(325);
            match(TracerCommandsParser::T__52);
            setState(326);
            match(TracerCommandsParser::UINT);
            setState(328);
            _errHandler->sync(this);

            _la = _input->LA(1);
            if (_la == TracerCommandsParser::CWD) {
              setState(327);
              cwdOption();
            }
            break;
          }

        default:
          throw NoViableAltException(this);
        }
        break;
      }

      case TracerCommandsParser::T__53: {
        enterOuterAlt(_localctx, 2);
        setState(332);
        match(TracerCommandsParser::T__53);
        setState(334);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::CWD) {
          setState(333);
          cwdOption();
        }
        break;
      }

    default:
      throw NoViableAltException(this);
    }

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- PrepareOptionContext ------------------------------------------------------------------

TracerCommandsParser::PrepareOptionContext::PrepareOptionContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

TracerCommandsParser::ValueContext* TracerCommandsParser::PrepareOptionContext::value() {
  return getRuleContext<TracerCommandsParser::ValueContext>(0);
}

tree::TerminalNode* TracerCommandsParser::PrepareOptionContext::SIGNATURE() {
  return getToken(TracerCommandsParser::SIGNATURE, 0);
}

tree::TerminalNode* TracerCommandsParser::PrepareOptionContext::MEMBER() {
  return getToken(TracerCommandsParser::MEMBER, 0);
}

tree::TerminalNode* TracerCommandsParser::PrepareOptionContext::METADATA_ONLY() {
  return getToken(TracerCommandsParser::METADATA_ONLY, 0);
}

TracerCommandsParser::LimitOptionContext* TracerCommandsParser::PrepareOptionContext::limitOption() {
  return getRuleContext<TracerCommandsParser::LimitOptionContext>(0);
}

TracerCommandsParser::ExtraCompileOptionContext* TracerCommandsParser::PrepareOptionContext::extraCompileOption() {
  return getRuleContext<TracerCommandsParser::ExtraCompileOptionContext>(0);
}

TracerCommandsParser::CwdOptionContext* TracerCommandsParser::PrepareOptionContext::cwdOption() {
  return getRuleContext<TracerCommandsParser::CwdOptionContext>(0);
}


size_t TracerCommandsParser::PrepareOptionContext::getRuleIndex() const {
  return TracerCommandsParser::RulePrepareOption;
}


std::any TracerCommandsParser::PrepareOptionContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitPrepareOption(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::PrepareOptionContext* TracerCommandsParser::prepareOption() {
  PrepareOptionContext *_localctx = _tracker.createInstance<PrepareOptionContext>(_ctx, getState());
  enterRule(_localctx, 40, TracerCommandsParser::RulePrepareOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(348);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__28:
      case TracerCommandsParser::T__30:
      case TracerCommandsParser::T__42:
      case TracerCommandsParser::T__54:
      case TracerCommandsParser::T__55:
      case TracerCommandsParser::T__56:
      case TracerCommandsParser::T__57:
      case TracerCommandsParser::T__58: {
        enterOuterAlt(_localctx, 1);
        setState(338);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 1116901506365259776) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(339);
        value();
        break;
      }

      case TracerCommandsParser::SIGNATURE: {
        enterOuterAlt(_localctx, 2);
        setState(340);
        match(TracerCommandsParser::SIGNATURE);
        setState(341);
        value();
        break;
      }

      case TracerCommandsParser::MEMBER: {
        enterOuterAlt(_localctx, 3);
        setState(342);
        match(TracerCommandsParser::MEMBER);
        break;
      }

      case TracerCommandsParser::METADATA_ONLY: {
        enterOuterAlt(_localctx, 4);
        setState(343);
        match(TracerCommandsParser::METADATA_ONLY);
        break;
      }

      case TracerCommandsParser::T__59: {
        enterOuterAlt(_localctx, 5);
        setState(344);
        match(TracerCommandsParser::T__59);
        break;
      }

      case TracerCommandsParser::T__60:
      case TracerCommandsParser::T__61:
      case TracerCommandsParser::T__62:
      case TracerCommandsParser::T__63:
      case TracerCommandsParser::T__64:
      case TracerCommandsParser::T__65:
      case TracerCommandsParser::T__66:
      case TracerCommandsParser::T__67:
      case TracerCommandsParser::T__68:
      case TracerCommandsParser::T__69: {
        enterOuterAlt(_localctx, 6);
        setState(345);
        limitOption();
        break;
      }

      case TracerCommandsParser::T__180:
      case TracerCommandsParser::T__181: {
        enterOuterAlt(_localctx, 7);
        setState(346);
        extraCompileOption();
        break;
      }

      case TracerCommandsParser::CWD: {
        enterOuterAlt(_localctx, 8);
        setState(347);
        cwdOption();
        break;
      }

    default:
      throw NoViableAltException(this);
    }

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- LimitOptionContext ------------------------------------------------------------------

TracerCommandsParser::LimitOptionContext::LimitOptionContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* TracerCommandsParser::LimitOptionContext::UINT() {
  return getToken(TracerCommandsParser::UINT, 0);
}

tree::TerminalNode* TracerCommandsParser::LimitOptionContext::WORD() {
  return getToken(TracerCommandsParser::WORD, 0);
}


size_t TracerCommandsParser::LimitOptionContext::getRuleIndex() const {
  return TracerCommandsParser::RuleLimitOption;
}


std::any TracerCommandsParser::LimitOptionContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitLimitOption(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::LimitOptionContext* TracerCommandsParser::limitOption() {
  LimitOptionContext *_localctx = _tracker.createInstance<LimitOptionContext>(_ctx, getState());
  enterRule(_localctx, 42, TracerCommandsParser::RuleLimitOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(350);
    _la = _input->LA(1);
    if (!(((((_la - 61) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 61)) & 1023) != 0))) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(351);
    _la = _input->LA(1);
    if (!(_la == TracerCommandsParser::UINT

    || _la == TracerCommandsParser::WORD)) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ResolveOptionContext ------------------------------------------------------------------

TracerCommandsParser::ResolveOptionContext::ResolveOptionContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

TracerCommandsParser::ValueContext* TracerCommandsParser::ResolveOptionContext::value() {
  return getRuleContext<TracerCommandsParser::ValueContext>(0);
}

TracerCommandsParser::ExtraCompileOptionContext* TracerCommandsParser::ResolveOptionContext::extraCompileOption() {
  return getRuleContext<TracerCommandsParser::ExtraCompileOptionContext>(0);
}


size_t TracerCommandsParser::ResolveOptionContext::getRuleIndex() const {
  return TracerCommandsParser::RuleResolveOption;
}


std::any TracerCommandsParser::ResolveOptionContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitResolveOption(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::ResolveOptionContext* TracerCommandsParser::resolveOption() {
  ResolveOptionContext *_localctx = _tracker.createInstance<ResolveOptionContext>(_ctx, getState());
  enterRule(_localctx, 44, TracerCommandsParser::RuleResolveOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(356);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__28:
      case TracerCommandsParser::T__54:
      case TracerCommandsParser::T__55:
      case TracerCommandsParser::T__56:
      case TracerCommandsParser::T__70:
      case TracerCommandsParser::T__71:
      case TracerCommandsParser::T__72: {
        enterOuterAlt(_localctx, 1);
        setState(353);
        _la = _input->LA(1);
        if (!(((((_la - 29) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 29)) & 30786795339777) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(354);
        value();
        break;
      }

      case TracerCommandsParser::T__180:
      case TracerCommandsParser::T__181: {
        enterOuterAlt(_localctx, 2);
        setState(355);
        extraCompileOption();
        break;
      }

    default:
      throw NoViableAltException(this);
    }

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ValidateOptionContext ------------------------------------------------------------------

TracerCommandsParser::ValidateOptionContext::ValidateOptionContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

TracerCommandsParser::ResolveOptionContext* TracerCommandsParser::ValidateOptionContext::resolveOption() {
  return getRuleContext<TracerCommandsParser::ResolveOptionContext>(0);
}

TracerCommandsParser::LimitOptionContext* TracerCommandsParser::ValidateOptionContext::limitOption() {
  return getRuleContext<TracerCommandsParser::LimitOptionContext>(0);
}


size_t TracerCommandsParser::ValidateOptionContext::getRuleIndex() const {
  return TracerCommandsParser::RuleValidateOption;
}


std::any TracerCommandsParser::ValidateOptionContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitValidateOption(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::ValidateOptionContext* TracerCommandsParser::validateOption() {
  ValidateOptionContext *_localctx = _tracker.createInstance<ValidateOptionContext>(_ctx, getState());
  enterRule(_localctx, 46, TracerCommandsParser::RuleValidateOption);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(366);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__28:
      case TracerCommandsParser::T__54:
      case TracerCommandsParser::T__55:
      case TracerCommandsParser::T__56:
      case TracerCommandsParser::T__70:
      case TracerCommandsParser::T__71:
      case TracerCommandsParser::T__72:
      case TracerCommandsParser::T__180:
      case TracerCommandsParser::T__181: {
        enterOuterAlt(_localctx, 1);
        setState(358);
        resolveOption();
        break;
      }

      case TracerCommandsParser::T__60:
      case TracerCommandsParser::T__61:
      case TracerCommandsParser::T__62:
      case TracerCommandsParser::T__63:
      case TracerCommandsParser::T__64:
      case TracerCommandsParser::T__65:
      case TracerCommandsParser::T__66:
      case TracerCommandsParser::T__67:
      case TracerCommandsParser::T__68:
      case TracerCommandsParser::T__69: {
        enterOuterAlt(_localctx, 2);
        setState(359);
        limitOption();
        break;
      }

      case TracerCommandsParser::T__73: {
        enterOuterAlt(_localctx, 3);
        setState(360);
        match(TracerCommandsParser::T__73);
        break;
      }

      case TracerCommandsParser::T__74: {
        enterOuterAlt(_localctx, 4);
        setState(361);
        match(TracerCommandsParser::T__74);
        break;
      }

      case TracerCommandsParser::T__75: {
        enterOuterAlt(_localctx, 5);
        setState(362);
        match(TracerCommandsParser::T__75);
        break;
      }

      case TracerCommandsParser::T__76: {
        enterOuterAlt(_localctx, 6);
        setState(363);
        match(TracerCommandsParser::T__76);
        break;
      }

      case TracerCommandsParser::T__77: {
        enterOuterAlt(_localctx, 7);
        setState(364);
        match(TracerCommandsParser::T__77);
        break;
      }

      case TracerCommandsParser::T__78: {
        enterOuterAlt(_localctx, 8);
        setState(365);
        match(TracerCommandsParser::T__78);
        break;
      }

    default:
      throw NoViableAltException(this);
    }

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- DeployOptionContext ------------------------------------------------------------------

TracerCommandsParser::DeployOptionContext::DeployOptionContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

TracerCommandsParser::ValidateOptionContext* TracerCommandsParser::DeployOptionContext::validateOption() {
  return getRuleContext<TracerCommandsParser::ValidateOptionContext>(0);
}

tree::TerminalNode* TracerCommandsParser::DeployOptionContext::UINT() {
  return getToken(TracerCommandsParser::UINT, 0);
}

tree::TerminalNode* TracerCommandsParser::DeployOptionContext::WORD() {
  return getToken(TracerCommandsParser::WORD, 0);
}

tree::TerminalNode* TracerCommandsParser::DeployOptionContext::SNAPSHOT_RETURN_POINTEE() {
  return getToken(TracerCommandsParser::SNAPSHOT_RETURN_POINTEE, 0);
}

TracerCommandsParser::StackFilterContext* TracerCommandsParser::DeployOptionContext::stackFilter() {
  return getRuleContext<TracerCommandsParser::StackFilterContext>(0);
}


size_t TracerCommandsParser::DeployOptionContext::getRuleIndex() const {
  return TracerCommandsParser::RuleDeployOption;
}


std::any TracerCommandsParser::DeployOptionContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitDeployOption(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::DeployOptionContext* TracerCommandsParser::deployOption() {
  DeployOptionContext *_localctx = _tracker.createInstance<DeployOptionContext>(_ctx, getState());
  enterRule(_localctx, 48, TracerCommandsParser::RuleDeployOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(375);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__28:
      case TracerCommandsParser::T__54:
      case TracerCommandsParser::T__55:
      case TracerCommandsParser::T__56:
      case TracerCommandsParser::T__60:
      case TracerCommandsParser::T__61:
      case TracerCommandsParser::T__62:
      case TracerCommandsParser::T__63:
      case TracerCommandsParser::T__64:
      case TracerCommandsParser::T__65:
      case TracerCommandsParser::T__66:
      case TracerCommandsParser::T__67:
      case TracerCommandsParser::T__68:
      case TracerCommandsParser::T__69:
      case TracerCommandsParser::T__70:
      case TracerCommandsParser::T__71:
      case TracerCommandsParser::T__72:
      case TracerCommandsParser::T__73:
      case TracerCommandsParser::T__74:
      case TracerCommandsParser::T__75:
      case TracerCommandsParser::T__76:
      case TracerCommandsParser::T__77:
      case TracerCommandsParser::T__78:
      case TracerCommandsParser::T__180:
      case TracerCommandsParser::T__181: {
        enterOuterAlt(_localctx, 1);
        setState(368);
        validateOption();
        break;
      }

      case TracerCommandsParser::T__79: {
        enterOuterAlt(_localctx, 2);
        setState(369);
        match(TracerCommandsParser::T__79);
        setState(370);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::UINT

        || _la == TracerCommandsParser::WORD)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        break;
      }

      case TracerCommandsParser::T__80: {
        enterOuterAlt(_localctx, 3);
        setState(371);
        match(TracerCommandsParser::T__80);
        break;
      }

      case TracerCommandsParser::T__81: {
        enterOuterAlt(_localctx, 4);
        setState(372);
        match(TracerCommandsParser::T__81);
        break;
      }

      case TracerCommandsParser::SNAPSHOT_RETURN_POINTEE: {
        enterOuterAlt(_localctx, 5);
        setState(373);
        match(TracerCommandsParser::SNAPSHOT_RETURN_POINTEE);
        break;
      }

      case TracerCommandsParser::T__177: {
        enterOuterAlt(_localctx, 6);
        setState(374);
        stackFilter();
        break;
      }

    default:
      throw NoViableAltException(this);
    }

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- InterceptorContext ------------------------------------------------------------------

TracerCommandsParser::InterceptorContext::InterceptorContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<TracerCommandsParser::ValueContext *> TracerCommandsParser::InterceptorContext::value() {
  return getRuleContexts<TracerCommandsParser::ValueContext>();
}

TracerCommandsParser::ValueContext* TracerCommandsParser::InterceptorContext::value(size_t i) {
  return getRuleContext<TracerCommandsParser::ValueContext>(i);
}

std::vector<TracerCommandsParser::ValidateOptionContext *> TracerCommandsParser::InterceptorContext::validateOption() {
  return getRuleContexts<TracerCommandsParser::ValidateOptionContext>();
}

TracerCommandsParser::ValidateOptionContext* TracerCommandsParser::InterceptorContext::validateOption(size_t i) {
  return getRuleContext<TracerCommandsParser::ValidateOptionContext>(i);
}

std::vector<TracerCommandsParser::DeployOptionContext *> TracerCommandsParser::InterceptorContext::deployOption() {
  return getRuleContexts<TracerCommandsParser::DeployOptionContext>();
}

TracerCommandsParser::DeployOptionContext* TracerCommandsParser::InterceptorContext::deployOption(size_t i) {
  return getRuleContext<TracerCommandsParser::DeployOptionContext>(i);
}


size_t TracerCommandsParser::InterceptorContext::getRuleIndex() const {
  return TracerCommandsParser::RuleInterceptor;
}


std::any TracerCommandsParser::InterceptorContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitInterceptor(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::InterceptorContext* TracerCommandsParser::interceptor() {
  InterceptorContext *_localctx = _tracker.createInstance<InterceptorContext>(_ctx, getState());
  enterRule(_localctx, 50, TracerCommandsParser::RuleInterceptor);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(377);
    match(TracerCommandsParser::T__82);
    setState(407);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__83: {
        setState(378);
        match(TracerCommandsParser::T__83);
        setState(379);
        value();
        setState(380);
        match(TracerCommandsParser::T__84);
        setState(381);
        value();
        break;
      }

      case TracerCommandsParser::T__85: {
        setState(383);
        match(TracerCommandsParser::T__85);
        setState(384);
        value();
        break;
      }

      case TracerCommandsParser::T__25: {
        setState(385);
        match(TracerCommandsParser::T__25);
        setState(386);
        value();
        break;
      }

      case TracerCommandsParser::T__51: {
        setState(387);
        match(TracerCommandsParser::T__51);
        setState(388);
        value();
        setState(389);
        match(TracerCommandsParser::T__86);
        setState(390);
        value();
        setState(394);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (((((_la - 29) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 29)) & 2251795988480001) != 0) || _la == TracerCommandsParser::T__180

        || _la == TracerCommandsParser::T__181) {
          setState(391);
          validateOption();
          setState(396);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__87: {
        setState(397);
        match(TracerCommandsParser::T__87);
        setState(398);
        value();
        setState(399);
        match(TracerCommandsParser::T__86);
        setState(400);
        value();
        setState(404);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (((((_la - 29) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 29)) & 18014394684276737) != 0) || ((((_la - 178) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 178)) & 16409) != 0)) {
          setState(401);
          deployOption();
          setState(406);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

    default:
      throw NoViableAltException(this);
    }

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- EventsContext ------------------------------------------------------------------

TracerCommandsParser::EventsContext::EventsContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<TracerCommandsParser::ValueContext *> TracerCommandsParser::EventsContext::value() {
  return getRuleContexts<TracerCommandsParser::ValueContext>();
}

TracerCommandsParser::ValueContext* TracerCommandsParser::EventsContext::value(size_t i) {
  return getRuleContext<TracerCommandsParser::ValueContext>(i);
}

tree::TerminalNode* TracerCommandsParser::EventsContext::UINT() {
  return getToken(TracerCommandsParser::UINT, 0);
}

tree::TerminalNode* TracerCommandsParser::EventsContext::STACK() {
  return getToken(TracerCommandsParser::STACK, 0);
}

std::vector<TracerCommandsParser::StreamOptionContext *> TracerCommandsParser::EventsContext::streamOption() {
  return getRuleContexts<TracerCommandsParser::StreamOptionContext>();
}

TracerCommandsParser::StreamOptionContext* TracerCommandsParser::EventsContext::streamOption(size_t i) {
  return getRuleContext<TracerCommandsParser::StreamOptionContext>(i);
}


size_t TracerCommandsParser::EventsContext::getRuleIndex() const {
  return TracerCommandsParser::RuleEvents;
}


std::any TracerCommandsParser::EventsContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitEvents(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::EventsContext* TracerCommandsParser::events() {
  EventsContext *_localctx = _tracker.createInstance<EventsContext>(_ctx, getState());
  enterRule(_localctx, 52, TracerCommandsParser::RuleEvents);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(409);
    match(TracerCommandsParser::T__88);
    setState(450);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::EOF:
      case TracerCommandsParser::T__19:
      case TracerCommandsParser::T__20:
      case TracerCommandsParser::T__84:
      case TracerCommandsParser::T__97:
      case TracerCommandsParser::T__98:
      case TracerCommandsParser::T__99:
      case TracerCommandsParser::T__103:
      case TracerCommandsParser::T__104:
      case TracerCommandsParser::T__105: {
        setState(413);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__19

        || _la == TracerCommandsParser::T__20 || ((((_la - 85) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 85)) & 3727361) != 0)) {
          setState(410);
          streamOption();
          setState(415);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__89: {
        setState(416);
        match(TracerCommandsParser::T__89);
        setState(417);
        value();
        setState(420);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__36) {
          setState(418);
          match(TracerCommandsParser::T__36);
          setState(419);
          match(TracerCommandsParser::UINT);
        }
        break;
      }

      case TracerCommandsParser::T__25: {
        setState(422);
        match(TracerCommandsParser::T__25);
        setState(423);
        value();
        setState(424);
        match(TracerCommandsParser::T__34);
        setState(425);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::STACK: {
        setState(427);
        match(TracerCommandsParser::STACK);
        setState(428);
        value();
        setState(429);
        match(TracerCommandsParser::T__34);
        setState(430);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__90: {
        setState(432);
        match(TracerCommandsParser::T__90);
        setState(433);
        value();
        break;
      }

      case TracerCommandsParser::T__91: {
        setState(434);
        match(TracerCommandsParser::T__91);
        setState(435);
        value();
        break;
      }

      case TracerCommandsParser::T__92: {
        setState(436);
        match(TracerCommandsParser::T__92);
        setState(437);
        value();
        break;
      }

      case TracerCommandsParser::T__93: {
        setState(438);
        match(TracerCommandsParser::T__93);
        setState(439);
        value();
        setState(440);
        match(TracerCommandsParser::T__84);
        setState(441);
        value();
        break;
      }

      case TracerCommandsParser::T__94: {
        setState(443);
        match(TracerCommandsParser::T__94);
        setState(444);
        value();
        setState(445);
        match(TracerCommandsParser::T__37);
        setState(446);
        match(TracerCommandsParser::T__38);
        setState(447);
        match(TracerCommandsParser::T__30);
        setState(448);
        value();
        break;
      }

    default:
      throw NoViableAltException(this);
    }

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- CollectContext ------------------------------------------------------------------

TracerCommandsParser::CollectContext::CollectContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<TracerCommandsParser::StreamOptionContext *> TracerCommandsParser::CollectContext::streamOption() {
  return getRuleContexts<TracerCommandsParser::StreamOptionContext>();
}

TracerCommandsParser::StreamOptionContext* TracerCommandsParser::CollectContext::streamOption(size_t i) {
  return getRuleContext<TracerCommandsParser::StreamOptionContext>(i);
}


size_t TracerCommandsParser::CollectContext::getRuleIndex() const {
  return TracerCommandsParser::RuleCollect;
}


std::any TracerCommandsParser::CollectContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitCollect(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::CollectContext* TracerCommandsParser::collect() {
  CollectContext *_localctx = _tracker.createInstance<CollectContext>(_ctx, getState());
  enterRule(_localctx, 54, TracerCommandsParser::RuleCollect);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(452);
    match(TracerCommandsParser::T__95);
    setState(456);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == TracerCommandsParser::T__19

    || _la == TracerCommandsParser::T__20 || ((((_la - 85) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 85)) & 3727361) != 0)) {
      setState(453);
      streamOption();
      setState(458);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- RecordContext ------------------------------------------------------------------

TracerCommandsParser::RecordContext::RecordContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

TracerCommandsParser::ValueContext* TracerCommandsParser::RecordContext::value() {
  return getRuleContext<TracerCommandsParser::ValueContext>(0);
}

std::vector<TracerCommandsParser::StreamOptionContext *> TracerCommandsParser::RecordContext::streamOption() {
  return getRuleContexts<TracerCommandsParser::StreamOptionContext>();
}

TracerCommandsParser::StreamOptionContext* TracerCommandsParser::RecordContext::streamOption(size_t i) {
  return getRuleContext<TracerCommandsParser::StreamOptionContext>(i);
}

std::vector<tree::TerminalNode *> TracerCommandsParser::RecordContext::SQLITE() {
  return getTokens(TracerCommandsParser::SQLITE);
}

tree::TerminalNode* TracerCommandsParser::RecordContext::SQLITE(size_t i) {
  return getToken(TracerCommandsParser::SQLITE, i);
}


size_t TracerCommandsParser::RecordContext::getRuleIndex() const {
  return TracerCommandsParser::RuleRecord;
}


std::any TracerCommandsParser::RecordContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitRecord(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::RecordContext* TracerCommandsParser::record() {
  RecordContext *_localctx = _tracker.createInstance<RecordContext>(_ctx, getState());
  enterRule(_localctx, 56, TracerCommandsParser::RuleRecord);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(459);
    match(TracerCommandsParser::T__96);
    setState(460);
    value();
    setState(465);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == TracerCommandsParser::T__19

    || _la == TracerCommandsParser::T__20 || ((((_la - 85) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 85)) & 3727361) != 0) || _la == TracerCommandsParser::SQLITE) {
      setState(463);
      _errHandler->sync(this);
      switch (_input->LA(1)) {
        case TracerCommandsParser::T__19:
        case TracerCommandsParser::T__20:
        case TracerCommandsParser::T__84:
        case TracerCommandsParser::T__97:
        case TracerCommandsParser::T__98:
        case TracerCommandsParser::T__99:
        case TracerCommandsParser::T__103:
        case TracerCommandsParser::T__104:
        case TracerCommandsParser::T__105: {
          setState(461);
          streamOption();
          break;
        }

        case TracerCommandsParser::SQLITE: {
          setState(462);
          match(TracerCommandsParser::SQLITE);
          break;
        }

      default:
        throw NoViableAltException(this);
      }
      setState(467);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- StreamOptionContext ------------------------------------------------------------------

TracerCommandsParser::StreamOptionContext::StreamOptionContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* TracerCommandsParser::StreamOptionContext::UINT() {
  return getToken(TracerCommandsParser::UINT, 0);
}

TracerCommandsParser::ValueContext* TracerCommandsParser::StreamOptionContext::value() {
  return getRuleContext<TracerCommandsParser::ValueContext>(0);
}


size_t TracerCommandsParser::StreamOptionContext::getRuleIndex() const {
  return TracerCommandsParser::RuleStreamOption;
}


std::any TracerCommandsParser::StreamOptionContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitStreamOption(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::StreamOptionContext* TracerCommandsParser::streamOption() {
  StreamOptionContext *_localctx = _tracker.createInstance<StreamOptionContext>(_ctx, getState());
  enterRule(_localctx, 58, TracerCommandsParser::RuleStreamOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(475);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__19:
      case TracerCommandsParser::T__20:
      case TracerCommandsParser::T__97:
      case TracerCommandsParser::T__98: {
        enterOuterAlt(_localctx, 1);
        setState(468);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__19

        || _la == TracerCommandsParser::T__20 || _la == TracerCommandsParser::T__97

        || _la == TracerCommandsParser::T__98)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(469);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__99: {
        enterOuterAlt(_localctx, 2);
        setState(470);
        match(TracerCommandsParser::T__99);
        setState(471);
        _la = _input->LA(1);
        if (!(((((_la - 101) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 101)) & 7) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        break;
      }

      case TracerCommandsParser::T__84:
      case TracerCommandsParser::T__103:
      case TracerCommandsParser::T__104: {
        enterOuterAlt(_localctx, 3);
        setState(472);
        _la = _input->LA(1);
        if (!(((((_la - 85) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 85)) & 1572865) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(473);
        value();
        break;
      }

      case TracerCommandsParser::T__105: {
        enterOuterAlt(_localctx, 4);
        setState(474);
        match(TracerCommandsParser::T__105);
        break;
      }

    default:
      throw NoViableAltException(this);
    }

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- CppContext ------------------------------------------------------------------

TracerCommandsParser::CppContext::CppContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<TracerCommandsParser::ValueContext *> TracerCommandsParser::CppContext::value() {
  return getRuleContexts<TracerCommandsParser::ValueContext>();
}

TracerCommandsParser::ValueContext* TracerCommandsParser::CppContext::value(size_t i) {
  return getRuleContext<TracerCommandsParser::ValueContext>(i);
}

std::vector<tree::TerminalNode *> TracerCommandsParser::CppContext::SHELL() {
  return getTokens(TracerCommandsParser::SHELL);
}

tree::TerminalNode* TracerCommandsParser::CppContext::SHELL(size_t i) {
  return getToken(TracerCommandsParser::SHELL, i);
}


size_t TracerCommandsParser::CppContext::getRuleIndex() const {
  return TracerCommandsParser::RuleCpp;
}


std::any TracerCommandsParser::CppContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitCpp(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::CppContext* TracerCommandsParser::cpp() {
  CppContext *_localctx = _tracker.createInstance<CppContext>(_ctx, getState());
  enterRule(_localctx, 60, TracerCommandsParser::RuleCpp);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(477);
    match(TracerCommandsParser::T__106);
    setState(506);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__83: {
        setState(478);
        match(TracerCommandsParser::T__83);
        setState(479);
        value();
        break;
      }

      case TracerCommandsParser::T__107: {
        setState(480);
        match(TracerCommandsParser::T__107);
        break;
      }

      case TracerCommandsParser::T__108:
      case TracerCommandsParser::T__109: {
        setState(481);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__108

        || _la == TracerCommandsParser::T__109)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(482);
        value();
        setState(487);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__30

        || _la == TracerCommandsParser::T__40) {
          setState(483);
          _la = _input->LA(1);
          if (!(_la == TracerCommandsParser::T__30

          || _la == TracerCommandsParser::T__40)) {
          _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(484);
          value();
          setState(489);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__110: {
        setState(490);
        match(TracerCommandsParser::T__110);
        setState(504);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__111: {
            setState(491);
            match(TracerCommandsParser::T__111);
            setState(494);
            _errHandler->sync(this);
            _la = _input->LA(1);
            do {
              setState(494);
              _errHandler->sync(this);
              switch (_input->LA(1)) {
                case TracerCommandsParser::T__1:
                case TracerCommandsParser::T__2:
                case TracerCommandsParser::T__3:
                case TracerCommandsParser::T__4:
                case TracerCommandsParser::T__5:
                case TracerCommandsParser::T__8:
                case TracerCommandsParser::T__10:
                case TracerCommandsParser::T__12:
                case TracerCommandsParser::T__13:
                case TracerCommandsParser::T__14:
                case TracerCommandsParser::T__15:
                case TracerCommandsParser::T__21:
                case TracerCommandsParser::T__22:
                case TracerCommandsParser::T__23:
                case TracerCommandsParser::T__24:
                case TracerCommandsParser::T__25:
                case TracerCommandsParser::T__26:
                case TracerCommandsParser::T__38:
                case TracerCommandsParser::T__43:
                case TracerCommandsParser::T__46:
                case TracerCommandsParser::T__47:
                case TracerCommandsParser::T__48:
                case TracerCommandsParser::T__49:
                case TracerCommandsParser::T__51:
                case TracerCommandsParser::T__52:
                case TracerCommandsParser::T__53:
                case TracerCommandsParser::T__82:
                case TracerCommandsParser::T__83:
                case TracerCommandsParser::T__85:
                case TracerCommandsParser::T__87:
                case TracerCommandsParser::T__88:
                case TracerCommandsParser::T__89:
                case TracerCommandsParser::T__90:
                case TracerCommandsParser::T__91:
                case TracerCommandsParser::T__92:
                case TracerCommandsParser::T__93:
                case TracerCommandsParser::T__94:
                case TracerCommandsParser::T__95:
                case TracerCommandsParser::T__96:
                case TracerCommandsParser::T__100:
                case TracerCommandsParser::T__101:
                case TracerCommandsParser::T__102:
                case TracerCommandsParser::T__106:
                case TracerCommandsParser::T__107:
                case TracerCommandsParser::T__108:
                case TracerCommandsParser::T__109:
                case TracerCommandsParser::T__110:
                case TracerCommandsParser::T__111:
                case TracerCommandsParser::T__112:
                case TracerCommandsParser::T__113:
                case TracerCommandsParser::T__114:
                case TracerCommandsParser::T__115:
                case TracerCommandsParser::T__116:
                case TracerCommandsParser::T__117:
                case TracerCommandsParser::T__118:
                case TracerCommandsParser::T__119:
                case TracerCommandsParser::T__120:
                case TracerCommandsParser::T__121:
                case TracerCommandsParser::T__126:
                case TracerCommandsParser::T__127:
                case TracerCommandsParser::T__128:
                case TracerCommandsParser::T__129:
                case TracerCommandsParser::T__130:
                case TracerCommandsParser::T__131:
                case TracerCommandsParser::T__132:
                case TracerCommandsParser::T__133:
                case TracerCommandsParser::T__134:
                case TracerCommandsParser::T__135:
                case TracerCommandsParser::T__136:
                case TracerCommandsParser::T__137:
                case TracerCommandsParser::T__138:
                case TracerCommandsParser::T__139:
                case TracerCommandsParser::T__140:
                case TracerCommandsParser::T__141:
                case TracerCommandsParser::T__142:
                case TracerCommandsParser::T__143:
                case TracerCommandsParser::T__144:
                case TracerCommandsParser::T__145:
                case TracerCommandsParser::T__146:
                case TracerCommandsParser::T__147:
                case TracerCommandsParser::T__148:
                case TracerCommandsParser::T__149:
                case TracerCommandsParser::T__150:
                case TracerCommandsParser::T__151:
                case TracerCommandsParser::T__152:
                case TracerCommandsParser::T__153:
                case TracerCommandsParser::T__154:
                case TracerCommandsParser::UINT:
                case TracerCommandsParser::STRING:
                case TracerCommandsParser::STACK:
                case TracerCommandsParser::WORD: {
                  setState(492);
                  value();
                  break;
                }

                case TracerCommandsParser::SHELL: {
                  setState(493);
                  match(TracerCommandsParser::SHELL);
                  break;
                }

              default:
                throw NoViableAltException(this);
              }
              setState(496);
              _errHandler->sync(this);
              _la = _input->LA(1);
            } while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 33654401923148412) != 0) || ((((_la - 83) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 83)) & -16492689326101) != 0) || ((((_la - 147) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 147)) & 73048803770879) != 0));
            break;
          }

          case TracerCommandsParser::T__92: {
            setState(498);
            match(TracerCommandsParser::T__92);
            break;
          }

          case TracerCommandsParser::T__112: {
            setState(499);
            match(TracerCommandsParser::T__112);
            setState(500);
            value();
            setState(501);
            match(TracerCommandsParser::T__27);
            setState(502);
            value();
            break;
          }

          case TracerCommandsParser::EOF: {
            break;
          }

        default:
          break;
        }
        break;
      }

    default:
      throw NoViableAltException(this);
    }

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ShellContext ------------------------------------------------------------------

TracerCommandsParser::ShellContext::ShellContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

TracerCommandsParser::ValueContext* TracerCommandsParser::ShellContext::value() {
  return getRuleContext<TracerCommandsParser::ValueContext>(0);
}


size_t TracerCommandsParser::ShellContext::getRuleIndex() const {
  return TracerCommandsParser::RuleShell;
}


std::any TracerCommandsParser::ShellContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitShell(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::ShellContext* TracerCommandsParser::shell() {
  ShellContext *_localctx = _tracker.createInstance<ShellContext>(_ctx, getState());
  enterRule(_localctx, 62, TracerCommandsParser::RuleShell);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(508);
    match(TracerCommandsParser::T__113);
    setState(509);
    value();

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- CompleteContext ------------------------------------------------------------------

TracerCommandsParser::CompleteContext::CompleteContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

TracerCommandsParser::ValueContext* TracerCommandsParser::CompleteContext::value() {
  return getRuleContext<TracerCommandsParser::ValueContext>(0);
}


size_t TracerCommandsParser::CompleteContext::getRuleIndex() const {
  return TracerCommandsParser::RuleComplete;
}


std::any TracerCommandsParser::CompleteContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitComplete(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::CompleteContext* TracerCommandsParser::complete() {
  CompleteContext *_localctx = _tracker.createInstance<CompleteContext>(_ctx, getState());
  enterRule(_localctx, 64, TracerCommandsParser::RuleComplete);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(511);
    match(TracerCommandsParser::T__114);
    setState(512);
    value();

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- TypeContext ------------------------------------------------------------------

TracerCommandsParser::TypeContext::TypeContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

TracerCommandsParser::ValueContext* TracerCommandsParser::TypeContext::value() {
  return getRuleContext<TracerCommandsParser::ValueContext>(0);
}


size_t TracerCommandsParser::TypeContext::getRuleIndex() const {
  return TracerCommandsParser::RuleType;
}


std::any TracerCommandsParser::TypeContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitType(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::TypeContext* TracerCommandsParser::type() {
  TypeContext *_localctx = _tracker.createInstance<TypeContext>(_ctx, getState());
  enterRule(_localctx, 66, TracerCommandsParser::RuleType);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(514);
    match(TracerCommandsParser::T__115);
    setState(515);
    value();

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- HelpContext ------------------------------------------------------------------

TracerCommandsParser::HelpContext::HelpContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<TracerCommandsParser::ValueContext *> TracerCommandsParser::HelpContext::value() {
  return getRuleContexts<TracerCommandsParser::ValueContext>();
}

TracerCommandsParser::ValueContext* TracerCommandsParser::HelpContext::value(size_t i) {
  return getRuleContext<TracerCommandsParser::ValueContext>(i);
}


size_t TracerCommandsParser::HelpContext::getRuleIndex() const {
  return TracerCommandsParser::RuleHelp;
}


std::any TracerCommandsParser::HelpContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitHelp(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::HelpContext* TracerCommandsParser::help() {
  HelpContext *_localctx = _tracker.createInstance<HelpContext>(_ctx, getState());
  enterRule(_localctx, 68, TracerCommandsParser::RuleHelp);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(517);
    match(TracerCommandsParser::T__116);
    setState(526);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 33654401923148414) != 0) || ((((_la - 83) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 83)) & -16492689326101) != 0) || ((((_la - 147) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 147)) & 72773925863935) != 0)) {
      setState(519);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == TracerCommandsParser::T__0) {
        setState(518);
        match(TracerCommandsParser::T__0);
      }
      setState(522);
      _errHandler->sync(this);
      _la = _input->LA(1);
      do {
        setState(521);
        value();
        setState(524);
        _errHandler->sync(this);
        _la = _input->LA(1);
      } while ((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & 33654401923148412) != 0) || ((((_la - 83) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 83)) & -16492689326101) != 0) || ((((_la - 147) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 147)) & 72773925863935) != 0));
    }

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- QuitContext ------------------------------------------------------------------

TracerCommandsParser::QuitContext::QuitContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}


size_t TracerCommandsParser::QuitContext::getRuleIndex() const {
  return TracerCommandsParser::RuleQuit;
}


std::any TracerCommandsParser::QuitContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitQuit(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::QuitContext* TracerCommandsParser::quit() {
  QuitContext *_localctx = _tracker.createInstance<QuitContext>(_ctx, getState());
  enterRule(_localctx, 70, TracerCommandsParser::RuleQuit);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(528);
    _la = _input->LA(1);
    if (!(_la == TracerCommandsParser::T__117

    || _la == TracerCommandsParser::T__118)) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- BulkTraceContext ------------------------------------------------------------------

TracerCommandsParser::BulkTraceContext::BulkTraceContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<TracerCommandsParser::ValueContext *> TracerCommandsParser::BulkTraceContext::value() {
  return getRuleContexts<TracerCommandsParser::ValueContext>();
}

TracerCommandsParser::ValueContext* TracerCommandsParser::BulkTraceContext::value(size_t i) {
  return getRuleContext<TracerCommandsParser::ValueContext>(i);
}

std::vector<TracerCommandsParser::BulkTraceOptionContext *> TracerCommandsParser::BulkTraceContext::bulkTraceOption() {
  return getRuleContexts<TracerCommandsParser::BulkTraceOptionContext>();
}

TracerCommandsParser::BulkTraceOptionContext* TracerCommandsParser::BulkTraceContext::bulkTraceOption(size_t i) {
  return getRuleContext<TracerCommandsParser::BulkTraceOptionContext>(i);
}


size_t TracerCommandsParser::BulkTraceContext::getRuleIndex() const {
  return TracerCommandsParser::RuleBulkTrace;
}


std::any TracerCommandsParser::BulkTraceContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitBulkTrace(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::BulkTraceContext* TracerCommandsParser::bulkTrace() {
  BulkTraceContext *_localctx = _tracker.createInstance<BulkTraceContext>(_ctx, getState());
  enterRule(_localctx, 72, TracerCommandsParser::RuleBulkTrace);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(530);
    match(TracerCommandsParser::T__119);
    setState(546);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__120: {
        setState(531);
        match(TracerCommandsParser::T__120);
        setState(533);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(532);
          bulkTraceOption();
          setState(535);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while (_la == TracerCommandsParser::T__28

        || _la == TracerCommandsParser::T__54 || ((((_la - 123) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 123)) & 15) != 0));
        break;
      }

      case TracerCommandsParser::T__121: {
        setState(537);
        match(TracerCommandsParser::T__121);
        setState(538);
        match(TracerCommandsParser::T__122);
        setState(539);
        value();
        setState(542);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__50) {
          setState(540);
          match(TracerCommandsParser::T__50);
          setState(541);
          value();
        }
        break;
      }

      case TracerCommandsParser::T__52: {
        setState(544);
        match(TracerCommandsParser::T__52);
        setState(545);
        value();
        break;
      }

    default:
      throw NoViableAltException(this);
    }

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- BulkTraceOptionContext ------------------------------------------------------------------

TracerCommandsParser::BulkTraceOptionContext::BulkTraceOptionContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

TracerCommandsParser::ValueContext* TracerCommandsParser::BulkTraceOptionContext::value() {
  return getRuleContext<TracerCommandsParser::ValueContext>(0);
}


size_t TracerCommandsParser::BulkTraceOptionContext::getRuleIndex() const {
  return TracerCommandsParser::RuleBulkTraceOption;
}


std::any TracerCommandsParser::BulkTraceOptionContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitBulkTraceOption(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::BulkTraceOptionContext* TracerCommandsParser::bulkTraceOption() {
  BulkTraceOptionContext *_localctx = _tracker.createInstance<BulkTraceOptionContext>(_ctx, getState());
  enterRule(_localctx, 74, TracerCommandsParser::RuleBulkTraceOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(548);
    _la = _input->LA(1);
    if (!(_la == TracerCommandsParser::T__28

    || _la == TracerCommandsParser::T__54 || ((((_la - 123) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 123)) & 15) != 0))) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(549);
    value();

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ValueContext ------------------------------------------------------------------

TracerCommandsParser::ValueContext::ValueContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* TracerCommandsParser::ValueContext::STRING() {
  return getToken(TracerCommandsParser::STRING, 0);
}

tree::TerminalNode* TracerCommandsParser::ValueContext::WORD() {
  return getToken(TracerCommandsParser::WORD, 0);
}

tree::TerminalNode* TracerCommandsParser::ValueContext::UINT() {
  return getToken(TracerCommandsParser::UINT, 0);
}

TracerCommandsParser::KeywordContext* TracerCommandsParser::ValueContext::keyword() {
  return getRuleContext<TracerCommandsParser::KeywordContext>(0);
}


size_t TracerCommandsParser::ValueContext::getRuleIndex() const {
  return TracerCommandsParser::RuleValue;
}


std::any TracerCommandsParser::ValueContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitValue(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::ValueContext* TracerCommandsParser::value() {
  ValueContext *_localctx = _tracker.createInstance<ValueContext>(_ctx, getState());
  enterRule(_localctx, 76, TracerCommandsParser::RuleValue);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(555);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::STRING: {
        enterOuterAlt(_localctx, 1);
        setState(551);
        match(TracerCommandsParser::STRING);
        break;
      }

      case TracerCommandsParser::WORD: {
        enterOuterAlt(_localctx, 2);
        setState(552);
        match(TracerCommandsParser::WORD);
        break;
      }

      case TracerCommandsParser::UINT: {
        enterOuterAlt(_localctx, 3);
        setState(553);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__1:
      case TracerCommandsParser::T__2:
      case TracerCommandsParser::T__3:
      case TracerCommandsParser::T__4:
      case TracerCommandsParser::T__5:
      case TracerCommandsParser::T__8:
      case TracerCommandsParser::T__10:
      case TracerCommandsParser::T__12:
      case TracerCommandsParser::T__13:
      case TracerCommandsParser::T__14:
      case TracerCommandsParser::T__15:
      case TracerCommandsParser::T__21:
      case TracerCommandsParser::T__22:
      case TracerCommandsParser::T__23:
      case TracerCommandsParser::T__24:
      case TracerCommandsParser::T__25:
      case TracerCommandsParser::T__26:
      case TracerCommandsParser::T__38:
      case TracerCommandsParser::T__43:
      case TracerCommandsParser::T__46:
      case TracerCommandsParser::T__47:
      case TracerCommandsParser::T__48:
      case TracerCommandsParser::T__49:
      case TracerCommandsParser::T__51:
      case TracerCommandsParser::T__52:
      case TracerCommandsParser::T__53:
      case TracerCommandsParser::T__82:
      case TracerCommandsParser::T__83:
      case TracerCommandsParser::T__85:
      case TracerCommandsParser::T__87:
      case TracerCommandsParser::T__88:
      case TracerCommandsParser::T__89:
      case TracerCommandsParser::T__90:
      case TracerCommandsParser::T__91:
      case TracerCommandsParser::T__92:
      case TracerCommandsParser::T__93:
      case TracerCommandsParser::T__94:
      case TracerCommandsParser::T__95:
      case TracerCommandsParser::T__96:
      case TracerCommandsParser::T__100:
      case TracerCommandsParser::T__101:
      case TracerCommandsParser::T__102:
      case TracerCommandsParser::T__106:
      case TracerCommandsParser::T__107:
      case TracerCommandsParser::T__108:
      case TracerCommandsParser::T__109:
      case TracerCommandsParser::T__110:
      case TracerCommandsParser::T__111:
      case TracerCommandsParser::T__112:
      case TracerCommandsParser::T__113:
      case TracerCommandsParser::T__114:
      case TracerCommandsParser::T__115:
      case TracerCommandsParser::T__116:
      case TracerCommandsParser::T__117:
      case TracerCommandsParser::T__118:
      case TracerCommandsParser::T__119:
      case TracerCommandsParser::T__120:
      case TracerCommandsParser::T__121:
      case TracerCommandsParser::T__126:
      case TracerCommandsParser::T__127:
      case TracerCommandsParser::T__128:
      case TracerCommandsParser::T__129:
      case TracerCommandsParser::T__130:
      case TracerCommandsParser::T__131:
      case TracerCommandsParser::T__132:
      case TracerCommandsParser::T__133:
      case TracerCommandsParser::T__134:
      case TracerCommandsParser::T__135:
      case TracerCommandsParser::T__136:
      case TracerCommandsParser::T__137:
      case TracerCommandsParser::T__138:
      case TracerCommandsParser::T__139:
      case TracerCommandsParser::T__140:
      case TracerCommandsParser::T__141:
      case TracerCommandsParser::T__142:
      case TracerCommandsParser::T__143:
      case TracerCommandsParser::T__144:
      case TracerCommandsParser::T__145:
      case TracerCommandsParser::T__146:
      case TracerCommandsParser::T__147:
      case TracerCommandsParser::T__148:
      case TracerCommandsParser::T__149:
      case TracerCommandsParser::T__150:
      case TracerCommandsParser::T__151:
      case TracerCommandsParser::T__152:
      case TracerCommandsParser::T__153:
      case TracerCommandsParser::T__154:
      case TracerCommandsParser::STACK: {
        enterOuterAlt(_localctx, 4);
        setState(554);
        keyword();
        break;
      }

    default:
      throw NoViableAltException(this);
    }

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- KeywordContext ------------------------------------------------------------------

TracerCommandsParser::KeywordContext::KeywordContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* TracerCommandsParser::KeywordContext::STACK() {
  return getToken(TracerCommandsParser::STACK, 0);
}


size_t TracerCommandsParser::KeywordContext::getRuleIndex() const {
  return TracerCommandsParser::RuleKeyword;
}


std::any TracerCommandsParser::KeywordContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitKeyword(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::KeywordContext* TracerCommandsParser::keyword() {
  KeywordContext *_localctx = _tracker.createInstance<KeywordContext>(_ctx, getState());
  enterRule(_localctx, 78, TracerCommandsParser::RuleKeyword);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(557);
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 33654401923148412) != 0) || ((((_la - 83) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 83)) & -16492689326101) != 0) || ((((_la - 147) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 147)) & 2199023256063) != 0))) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- MemoryContext ------------------------------------------------------------------

TracerCommandsParser::MemoryContext::MemoryContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<TracerCommandsParser::ValueContext *> TracerCommandsParser::MemoryContext::value() {
  return getRuleContexts<TracerCommandsParser::ValueContext>();
}

TracerCommandsParser::ValueContext* TracerCommandsParser::MemoryContext::value(size_t i) {
  return getRuleContext<TracerCommandsParser::ValueContext>(i);
}

std::vector<tree::TerminalNode *> TracerCommandsParser::MemoryContext::UINT() {
  return getTokens(TracerCommandsParser::UINT);
}

tree::TerminalNode* TracerCommandsParser::MemoryContext::UINT(size_t i) {
  return getToken(TracerCommandsParser::UINT, i);
}


size_t TracerCommandsParser::MemoryContext::getRuleIndex() const {
  return TracerCommandsParser::RuleMemory;
}


std::any TracerCommandsParser::MemoryContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitMemory(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::MemoryContext* TracerCommandsParser::memory() {
  MemoryContext *_localctx = _tracker.createInstance<MemoryContext>(_ctx, getState());
  enterRule(_localctx, 80, TracerCommandsParser::RuleMemory);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(559);
    match(TracerCommandsParser::T__126);
    setState(634);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__127: {
        setState(560);
        match(TracerCommandsParser::T__127);
        break;
      }

      case TracerCommandsParser::T__128: {
        setState(561);
        match(TracerCommandsParser::T__128);
        setState(562);
        value();
        setState(563);
        match(TracerCommandsParser::T__29);
        setState(564);
        value();
        setState(569);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__20: {
            setState(565);
            match(TracerCommandsParser::T__20);
            setState(566);
            match(TracerCommandsParser::UINT);
            break;
          }

          case TracerCommandsParser::T__40: {
            setState(567);
            match(TracerCommandsParser::T__40);
            setState(568);
            value();
            break;
          }

        default:
          throw NoViableAltException(this);
        }
        break;
      }

      case TracerCommandsParser::T__129: {
        setState(571);
        match(TracerCommandsParser::T__129);
        setState(572);
        value();
        setState(573);
        match(TracerCommandsParser::T__29);
        setState(574);
        value();
        setState(575);
        match(TracerCommandsParser::T__20);
        setState(576);
        match(TracerCommandsParser::UINT);
        setState(577);
        match(TracerCommandsParser::T__155);
        setState(578);
        value();
        setState(581);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__36) {
          setState(579);
          match(TracerCommandsParser::T__36);
          setState(580);
          match(TracerCommandsParser::UINT);
        }
        break;
      }

      case TracerCommandsParser::T__130: {
        setState(583);
        match(TracerCommandsParser::T__130);
        setState(584);
        match(TracerCommandsParser::T__28);
        setState(585);
        value();
        setState(586);
        match(TracerCommandsParser::T__156);
        setState(587);
        value();
        setState(588);
        match(TracerCommandsParser::T__29);
        setState(589);
        value();
        setState(590);
        match(TracerCommandsParser::T__157);
        setState(591);
        value();
        setState(592);
        match(TracerCommandsParser::T__158);
        setState(593);
        value();
        break;
      }

      case TracerCommandsParser::T__150: {
        setState(595);
        match(TracerCommandsParser::T__150);
        setState(596);
        value();
        setState(597);
        match(TracerCommandsParser::T__29);
        setState(598);
        value();
        setState(599);
        match(TracerCommandsParser::T__157);
        setState(600);
        value();
        setState(601);
        match(TracerCommandsParser::T__158);
        setState(602);
        value();
        break;
      }

      case TracerCommandsParser::T__131:
      case TracerCommandsParser::T__148:
      case TracerCommandsParser::T__149: {
        setState(604);
        _la = _input->LA(1);
        if (!(((((_la - 132) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 132)) & 393217) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(605);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__133: {
        setState(606);
        match(TracerCommandsParser::T__133);
        setState(607);
        match(TracerCommandsParser::T__159);
        setState(608);
        match(TracerCommandsParser::UINT);
        setState(609);
        match(TracerCommandsParser::T__44);
        setState(610);
        value();
        setState(611);
        match(TracerCommandsParser::T__160);
        setState(612);
        match(TracerCommandsParser::UINT);
        setState(613);
        match(TracerCommandsParser::T__29);
        setState(614);
        value();
        setState(617);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__19) {
          setState(615);
          match(TracerCommandsParser::T__19);
          setState(616);
          match(TracerCommandsParser::UINT);
        }
        break;
      }

      case TracerCommandsParser::T__134: {
        setState(619);
        match(TracerCommandsParser::T__134);
        break;
      }

      case TracerCommandsParser::T__132: {
        setState(620);
        match(TracerCommandsParser::T__132);
        setState(621);
        match(TracerCommandsParser::T__28);
        setState(622);
        value();
        setState(623);
        match(TracerCommandsParser::T__41);
        setState(624);
        value();
        setState(625);
        match(TracerCommandsParser::T__161);
        setState(626);
        value();
        setState(627);
        match(TracerCommandsParser::T__44);
        setState(628);
        value();
        setState(629);
        match(TracerCommandsParser::T__29);
        setState(630);
        value();
        setState(631);
        match(TracerCommandsParser::T__162);
        setState(632);
        value();
        break;
      }

    default:
      throw NoViableAltException(this);
    }

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ProbeEntryContext ------------------------------------------------------------------

TracerCommandsParser::ProbeEntryContext::ProbeEntryContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<TracerCommandsParser::ValueContext *> TracerCommandsParser::ProbeEntryContext::value() {
  return getRuleContexts<TracerCommandsParser::ValueContext>();
}

TracerCommandsParser::ValueContext* TracerCommandsParser::ProbeEntryContext::value(size_t i) {
  return getRuleContext<TracerCommandsParser::ValueContext>(i);
}

TracerCommandsParser::CwdOptionContext* TracerCommandsParser::ProbeEntryContext::cwdOption() {
  return getRuleContext<TracerCommandsParser::CwdOptionContext>(0);
}


size_t TracerCommandsParser::ProbeEntryContext::getRuleIndex() const {
  return TracerCommandsParser::RuleProbeEntry;
}


std::any TracerCommandsParser::ProbeEntryContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitProbeEntry(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::ProbeEntryContext* TracerCommandsParser::probeEntry() {
  ProbeEntryContext *_localctx = _tracker.createInstance<ProbeEntryContext>(_ctx, getState());
  enterRule(_localctx, 82, TracerCommandsParser::RuleProbeEntry);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(636);
    match(TracerCommandsParser::T__46);
    setState(637);
    match(TracerCommandsParser::T__135);
    setState(654);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__1:
      case TracerCommandsParser::T__2:
      case TracerCommandsParser::T__3:
      case TracerCommandsParser::T__4:
      case TracerCommandsParser::T__5:
      case TracerCommandsParser::T__8:
      case TracerCommandsParser::T__10:
      case TracerCommandsParser::T__12:
      case TracerCommandsParser::T__13:
      case TracerCommandsParser::T__14:
      case TracerCommandsParser::T__15:
      case TracerCommandsParser::T__21:
      case TracerCommandsParser::T__22:
      case TracerCommandsParser::T__23:
      case TracerCommandsParser::T__24:
      case TracerCommandsParser::T__25:
      case TracerCommandsParser::T__26:
      case TracerCommandsParser::T__38:
      case TracerCommandsParser::T__43:
      case TracerCommandsParser::T__46:
      case TracerCommandsParser::T__47:
      case TracerCommandsParser::T__48:
      case TracerCommandsParser::T__49:
      case TracerCommandsParser::T__51:
      case TracerCommandsParser::T__52:
      case TracerCommandsParser::T__53:
      case TracerCommandsParser::T__82:
      case TracerCommandsParser::T__83:
      case TracerCommandsParser::T__85:
      case TracerCommandsParser::T__87:
      case TracerCommandsParser::T__88:
      case TracerCommandsParser::T__89:
      case TracerCommandsParser::T__90:
      case TracerCommandsParser::T__91:
      case TracerCommandsParser::T__92:
      case TracerCommandsParser::T__93:
      case TracerCommandsParser::T__94:
      case TracerCommandsParser::T__95:
      case TracerCommandsParser::T__96:
      case TracerCommandsParser::T__100:
      case TracerCommandsParser::T__101:
      case TracerCommandsParser::T__102:
      case TracerCommandsParser::T__106:
      case TracerCommandsParser::T__107:
      case TracerCommandsParser::T__108:
      case TracerCommandsParser::T__109:
      case TracerCommandsParser::T__110:
      case TracerCommandsParser::T__111:
      case TracerCommandsParser::T__112:
      case TracerCommandsParser::T__113:
      case TracerCommandsParser::T__114:
      case TracerCommandsParser::T__115:
      case TracerCommandsParser::T__116:
      case TracerCommandsParser::T__117:
      case TracerCommandsParser::T__118:
      case TracerCommandsParser::T__119:
      case TracerCommandsParser::T__120:
      case TracerCommandsParser::T__121:
      case TracerCommandsParser::T__126:
      case TracerCommandsParser::T__127:
      case TracerCommandsParser::T__128:
      case TracerCommandsParser::T__129:
      case TracerCommandsParser::T__130:
      case TracerCommandsParser::T__131:
      case TracerCommandsParser::T__132:
      case TracerCommandsParser::T__133:
      case TracerCommandsParser::T__134:
      case TracerCommandsParser::T__135:
      case TracerCommandsParser::T__136:
      case TracerCommandsParser::T__137:
      case TracerCommandsParser::T__138:
      case TracerCommandsParser::T__139:
      case TracerCommandsParser::T__140:
      case TracerCommandsParser::T__141:
      case TracerCommandsParser::T__142:
      case TracerCommandsParser::T__143:
      case TracerCommandsParser::T__144:
      case TracerCommandsParser::T__145:
      case TracerCommandsParser::T__146:
      case TracerCommandsParser::T__147:
      case TracerCommandsParser::T__148:
      case TracerCommandsParser::T__149:
      case TracerCommandsParser::T__150:
      case TracerCommandsParser::T__151:
      case TracerCommandsParser::T__152:
      case TracerCommandsParser::T__153:
      case TracerCommandsParser::T__154:
      case TracerCommandsParser::UINT:
      case TracerCommandsParser::STRING:
      case TracerCommandsParser::STACK:
      case TracerCommandsParser::WORD: {
        setState(638);
        value();
        setState(639);
        match(TracerCommandsParser::T__29);
        setState(640);
        value();
        setState(643);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__28) {
          setState(641);
          match(TracerCommandsParser::T__28);
          setState(642);
          value();
        }
        break;
      }

      case TracerCommandsParser::T__28: {
        setState(645);
        match(TracerCommandsParser::T__28);
        setState(646);
        value();
        setState(647);
        match(TracerCommandsParser::T__156);
        setState(648);
        value();
        setState(649);
        match(TracerCommandsParser::T__162);
        setState(650);
        value();
        setState(651);
        match(TracerCommandsParser::T__29);
        setState(652);
        value();
        break;
      }

    default:
      throw NoViableAltException(this);
    }
    setState(657);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::CWD) {
      setState(656);
      cwdOption();
    }

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ProbeReloadContext ------------------------------------------------------------------

TracerCommandsParser::ProbeReloadContext::ProbeReloadContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<tree::TerminalNode *> TracerCommandsParser::ProbeReloadContext::UINT() {
  return getTokens(TracerCommandsParser::UINT);
}

tree::TerminalNode* TracerCommandsParser::ProbeReloadContext::UINT(size_t i) {
  return getToken(TracerCommandsParser::UINT, i);
}

TracerCommandsParser::ValueContext* TracerCommandsParser::ProbeReloadContext::value() {
  return getRuleContext<TracerCommandsParser::ValueContext>(0);
}

std::vector<TracerCommandsParser::CwdOptionContext *> TracerCommandsParser::ProbeReloadContext::cwdOption() {
  return getRuleContexts<TracerCommandsParser::CwdOptionContext>();
}

TracerCommandsParser::CwdOptionContext* TracerCommandsParser::ProbeReloadContext::cwdOption(size_t i) {
  return getRuleContext<TracerCommandsParser::CwdOptionContext>(i);
}


size_t TracerCommandsParser::ProbeReloadContext::getRuleIndex() const {
  return TracerCommandsParser::RuleProbeReload;
}


std::any TracerCommandsParser::ProbeReloadContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitProbeReload(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::ProbeReloadContext* TracerCommandsParser::probeReload() {
  ProbeReloadContext *_localctx = _tracker.createInstance<ProbeReloadContext>(_ctx, getState());
  enterRule(_localctx, 84, TracerCommandsParser::RuleProbeReload);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(659);
    match(TracerCommandsParser::T__46);
    setState(660);
    match(TracerCommandsParser::T__136);
    setState(661);
    match(TracerCommandsParser::UINT);
    setState(662);
    match(TracerCommandsParser::T__50);
    setState(663);
    value();
    setState(675);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == TracerCommandsParser::T__18 || _la == TracerCommandsParser::CWD) {
      setState(673);
      _errHandler->sync(this);
      switch (_input->LA(1)) {
        case TracerCommandsParser::CWD: {
          setState(664);
          cwdOption();
          break;
        }

        case TracerCommandsParser::T__18: {
          setState(665);
          match(TracerCommandsParser::T__18);
          setState(670);
          _errHandler->sync(this);
          _la = _input->LA(1);
          while (_la == TracerCommandsParser::T__19

          || _la == TracerCommandsParser::T__20) {
            setState(666);
            _la = _input->LA(1);
            if (!(_la == TracerCommandsParser::T__19

            || _la == TracerCommandsParser::T__20)) {
            _errHandler->recoverInline(this);
            }
            else {
              _errHandler->reportMatch(this);
              consume();
            }
            setState(667);
            match(TracerCommandsParser::UINT);
            setState(672);
            _errHandler->sync(this);
            _la = _input->LA(1);
          }
          break;
        }

      default:
        throw NoViableAltException(this);
      }
      setState(677);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- TargetLaunchContext ------------------------------------------------------------------

TracerCommandsParser::TargetLaunchContext::TargetLaunchContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

TracerCommandsParser::ValueContext* TracerCommandsParser::TargetLaunchContext::value() {
  return getRuleContext<TracerCommandsParser::ValueContext>(0);
}

std::vector<TracerCommandsParser::LaunchOptionContext *> TracerCommandsParser::TargetLaunchContext::launchOption() {
  return getRuleContexts<TracerCommandsParser::LaunchOptionContext>();
}

TracerCommandsParser::LaunchOptionContext* TracerCommandsParser::TargetLaunchContext::launchOption(size_t i) {
  return getRuleContext<TracerCommandsParser::LaunchOptionContext>(i);
}

tree::TerminalNode* TracerCommandsParser::TargetLaunchContext::UINT() {
  return getToken(TracerCommandsParser::UINT, 0);
}


size_t TracerCommandsParser::TargetLaunchContext::getRuleIndex() const {
  return TracerCommandsParser::RuleTargetLaunch;
}


std::any TracerCommandsParser::TargetLaunchContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitTargetLaunch(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::TargetLaunchContext* TracerCommandsParser::targetLaunch() {
  TargetLaunchContext *_localctx = _tracker.createInstance<TargetLaunchContext>(_ctx, getState());
  enterRule(_localctx, 86, TracerCommandsParser::RuleTargetLaunch);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(690);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__137: {
        enterOuterAlt(_localctx, 1);
        setState(678);
        match(TracerCommandsParser::T__137);
        setState(679);
        value();
        setState(683);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__19 || ((((_la - 164) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 164)) & 33554687) != 0)) {
          setState(680);
          launchOption();
          setState(685);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__138:
      case TracerCommandsParser::T__139: {
        enterOuterAlt(_localctx, 2);
        setState(686);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__138

        || _la == TracerCommandsParser::T__139)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(688);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::UINT) {
          setState(687);
          match(TracerCommandsParser::UINT);
        }
        break;
      }

    default:
      throw NoViableAltException(this);
    }

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- LaunchOptionContext ------------------------------------------------------------------

TracerCommandsParser::LaunchOptionContext::LaunchOptionContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

TracerCommandsParser::ValueContext* TracerCommandsParser::LaunchOptionContext::value() {
  return getRuleContext<TracerCommandsParser::ValueContext>(0);
}

tree::TerminalNode* TracerCommandsParser::LaunchOptionContext::CWD() {
  return getToken(TracerCommandsParser::CWD, 0);
}

tree::TerminalNode* TracerCommandsParser::LaunchOptionContext::UINT() {
  return getToken(TracerCommandsParser::UINT, 0);
}


size_t TracerCommandsParser::LaunchOptionContext::getRuleIndex() const {
  return TracerCommandsParser::RuleLaunchOption;
}


std::any TracerCommandsParser::LaunchOptionContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitLaunchOption(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::LaunchOptionContext* TracerCommandsParser::launchOption() {
  LaunchOptionContext *_localctx = _tracker.createInstance<LaunchOptionContext>(_ctx, getState());
  enterRule(_localctx, 88, TracerCommandsParser::RuleLaunchOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(698);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__163:
      case TracerCommandsParser::T__164:
      case TracerCommandsParser::T__165:
      case TracerCommandsParser::T__166:
      case TracerCommandsParser::T__167:
      case TracerCommandsParser::T__168:
      case TracerCommandsParser::CWD: {
        enterOuterAlt(_localctx, 1);
        setState(692);
        _la = _input->LA(1);
        if (!(((((_la - 164) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 164)) & 33554495) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(693);
        value();
        break;
      }

      case TracerCommandsParser::T__19: {
        enterOuterAlt(_localctx, 2);
        setState(694);
        match(TracerCommandsParser::T__19);
        setState(695);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__169: {
        enterOuterAlt(_localctx, 3);
        setState(696);
        match(TracerCommandsParser::T__169);
        break;
      }

      case TracerCommandsParser::T__170: {
        enterOuterAlt(_localctx, 4);
        setState(697);
        match(TracerCommandsParser::T__170);
        break;
      }

    default:
      throw NoViableAltException(this);
    }

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ChildListContext ------------------------------------------------------------------

TracerCommandsParser::ChildListContext::ChildListContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}


size_t TracerCommandsParser::ChildListContext::getRuleIndex() const {
  return TracerCommandsParser::RuleChildList;
}


std::any TracerCommandsParser::ChildListContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitChildList(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::ChildListContext* TracerCommandsParser::childList() {
  ChildListContext *_localctx = _tracker.createInstance<ChildListContext>(_ctx, getState());
  enterRule(_localctx, 90, TracerCommandsParser::RuleChildList);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(700);
    match(TracerCommandsParser::T__152);

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- TargetCallContext ------------------------------------------------------------------

TracerCommandsParser::TargetCallContext::TargetCallContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<TracerCommandsParser::ValueContext *> TracerCommandsParser::TargetCallContext::value() {
  return getRuleContexts<TracerCommandsParser::ValueContext>();
}

TracerCommandsParser::ValueContext* TracerCommandsParser::TargetCallContext::value(size_t i) {
  return getRuleContext<TracerCommandsParser::ValueContext>(i);
}

std::vector<tree::TerminalNode *> TracerCommandsParser::TargetCallContext::UINT() {
  return getTokens(TracerCommandsParser::UINT);
}

tree::TerminalNode* TracerCommandsParser::TargetCallContext::UINT(size_t i) {
  return getToken(TracerCommandsParser::UINT, i);
}


size_t TracerCommandsParser::TargetCallContext::getRuleIndex() const {
  return TracerCommandsParser::RuleTargetCall;
}


std::any TracerCommandsParser::TargetCallContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitTargetCall(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::TargetCallContext* TracerCommandsParser::targetCall() {
  TargetCallContext *_localctx = _tracker.createInstance<TargetCallContext>(_ctx, getState());
  enterRule(_localctx, 92, TracerCommandsParser::RuleTargetCall);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(718);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__140: {
        enterOuterAlt(_localctx, 1);
        setState(702);
        match(TracerCommandsParser::T__140);
        setState(703);
        value();
        setState(710);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__19 || _la == TracerCommandsParser::T__163) {
          setState(708);
          _errHandler->sync(this);
          switch (_input->LA(1)) {
            case TracerCommandsParser::T__163: {
              setState(704);
              match(TracerCommandsParser::T__163);
              setState(705);
              value();
              break;
            }

            case TracerCommandsParser::T__19: {
              setState(706);
              match(TracerCommandsParser::T__19);
              setState(707);
              match(TracerCommandsParser::UINT);
              break;
            }

          default:
            throw NoViableAltException(this);
          }
          setState(712);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__141: {
        enterOuterAlt(_localctx, 2);
        setState(713);
        match(TracerCommandsParser::T__141);
        setState(716);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__19) {
          setState(714);
          match(TracerCommandsParser::T__19);
          setState(715);
          match(TracerCommandsParser::UINT);
        }
        break;
      }

    default:
      throw NoViableAltException(this);
    }

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- LocalPerfContext ------------------------------------------------------------------

TracerCommandsParser::LocalPerfContext::LocalPerfContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<tree::TerminalNode *> TracerCommandsParser::LocalPerfContext::UINT() {
  return getTokens(TracerCommandsParser::UINT);
}

tree::TerminalNode* TracerCommandsParser::LocalPerfContext::UINT(size_t i) {
  return getToken(TracerCommandsParser::UINT, i);
}


size_t TracerCommandsParser::LocalPerfContext::getRuleIndex() const {
  return TracerCommandsParser::RuleLocalPerf;
}


std::any TracerCommandsParser::LocalPerfContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitLocalPerf(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::LocalPerfContext* TracerCommandsParser::localPerf() {
  LocalPerfContext *_localctx = _tracker.createInstance<LocalPerfContext>(_ctx, getState());
  enterRule(_localctx, 94, TracerCommandsParser::RuleLocalPerf);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(724);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__142: {
        setState(720);
        match(TracerCommandsParser::T__142);
        break;
      }

      case TracerCommandsParser::T__153: {
        setState(721);
        match(TracerCommandsParser::T__153);
        break;
      }

      case TracerCommandsParser::T__143: {
        setState(722);
        match(TracerCommandsParser::T__143);
        setState(723);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__144

        || _la == TracerCommandsParser::T__154)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        break;
      }

    default:
      throw NoViableAltException(this);
    }
    setState(730);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == TracerCommandsParser::T__19

    || _la == TracerCommandsParser::T__20) {
      setState(726);
      _la = _input->LA(1);
      if (!(_la == TracerCommandsParser::T__19

      || _la == TracerCommandsParser::T__20)) {
      _errHandler->recoverInline(this);
      }
      else {
        _errHandler->reportMatch(this);
        consume();
      }
      setState(727);
      match(TracerCommandsParser::UINT);
      setState(732);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- TargetLibraryContext ------------------------------------------------------------------

TracerCommandsParser::TargetLibraryContext::TargetLibraryContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

TracerCommandsParser::ValueContext* TracerCommandsParser::TargetLibraryContext::value() {
  return getRuleContext<TracerCommandsParser::ValueContext>(0);
}


size_t TracerCommandsParser::TargetLibraryContext::getRuleIndex() const {
  return TracerCommandsParser::RuleTargetLibrary;
}


std::any TracerCommandsParser::TargetLibraryContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitTargetLibrary(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::TargetLibraryContext* TracerCommandsParser::targetLibrary() {
  TargetLibraryContext *_localctx = _tracker.createInstance<TargetLibraryContext>(_ctx, getState());
  enterRule(_localctx, 96, TracerCommandsParser::RuleTargetLibrary);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(733);
    match(TracerCommandsParser::T__147);
    setState(734);
    match(TracerCommandsParser::T__83);
    setState(735);
    value();

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- InstructionsContext ------------------------------------------------------------------

TracerCommandsParser::InstructionsContext::InstructionsContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

TracerCommandsParser::ValueContext* TracerCommandsParser::InstructionsContext::value() {
  return getRuleContext<TracerCommandsParser::ValueContext>(0);
}

std::vector<TracerCommandsParser::InstructionOptionContext *> TracerCommandsParser::InstructionsContext::instructionOption() {
  return getRuleContexts<TracerCommandsParser::InstructionOptionContext>();
}

TracerCommandsParser::InstructionOptionContext* TracerCommandsParser::InstructionsContext::instructionOption(size_t i) {
  return getRuleContext<TracerCommandsParser::InstructionOptionContext>(i);
}


size_t TracerCommandsParser::InstructionsContext::getRuleIndex() const {
  return TracerCommandsParser::RuleInstructions;
}


std::any TracerCommandsParser::InstructionsContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitInstructions(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::InstructionsContext* TracerCommandsParser::instructions() {
  InstructionsContext *_localctx = _tracker.createInstance<InstructionsContext>(_ctx, getState());
  enterRule(_localctx, 98, TracerCommandsParser::RuleInstructions);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(737);
    match(TracerCommandsParser::T__145);
    setState(752);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__2: {
        setState(738);
        match(TracerCommandsParser::T__2);
        setState(742);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 35185448976384) != 0) || ((((_la - 160) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 160)) & 126979) != 0)) {
          setState(739);
          instructionOption();
          setState(744);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__5: {
        setState(745);
        match(TracerCommandsParser::T__5);
        setState(748);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__125) {
          setState(746);
          match(TracerCommandsParser::T__125);
          setState(747);
          value();
        }
        break;
      }

      case TracerCommandsParser::T__151: {
        setState(750);
        match(TracerCommandsParser::T__151);
        setState(751);
        value();
        break;
      }

    default:
      throw NoViableAltException(this);
    }

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- InstructionOptionContext ------------------------------------------------------------------

TracerCommandsParser::InstructionOptionContext::InstructionOptionContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* TracerCommandsParser::InstructionOptionContext::UINT() {
  return getToken(TracerCommandsParser::UINT, 0);
}

TracerCommandsParser::ValueContext* TracerCommandsParser::InstructionOptionContext::value() {
  return getRuleContext<TracerCommandsParser::ValueContext>(0);
}


size_t TracerCommandsParser::InstructionOptionContext::getRuleIndex() const {
  return TracerCommandsParser::RuleInstructionOption;
}


std::any TracerCommandsParser::InstructionOptionContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitInstructionOption(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::InstructionOptionContext* TracerCommandsParser::instructionOption() {
  InstructionOptionContext *_localctx = _tracker.createInstance<InstructionOptionContext>(_ctx, getState());
  enterRule(_localctx, 100, TracerCommandsParser::RuleInstructionOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(762);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__19:
      case TracerCommandsParser::T__20:
      case TracerCommandsParser::T__159:
      case TracerCommandsParser::T__160: {
        enterOuterAlt(_localctx, 1);
        setState(754);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__19

        || _la == TracerCommandsParser::T__20 || _la == TracerCommandsParser::T__159

        || _la == TracerCommandsParser::T__160)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(755);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__171: {
        enterOuterAlt(_localctx, 2);
        setState(756);
        match(TracerCommandsParser::T__171);
        setState(757);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__172: {
        enterOuterAlt(_localctx, 3);
        setState(758);
        match(TracerCommandsParser::T__172);
        break;
      }

      case TracerCommandsParser::T__173: {
        enterOuterAlt(_localctx, 4);
        setState(759);
        match(TracerCommandsParser::T__173);
        break;
      }

      case TracerCommandsParser::T__29:
      case TracerCommandsParser::T__44:
      case TracerCommandsParser::T__174:
      case TracerCommandsParser::T__175: {
        enterOuterAlt(_localctx, 5);
        setState(760);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__29

        || _la == TracerCommandsParser::T__44 || _la == TracerCommandsParser::T__174

        || _la == TracerCommandsParser::T__175)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(761);
        value();
        break;
      }

    default:
      throw NoViableAltException(this);
    }

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- WatchpointContext ------------------------------------------------------------------

TracerCommandsParser::WatchpointContext::WatchpointContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<tree::TerminalNode *> TracerCommandsParser::WatchpointContext::UINT() {
  return getTokens(TracerCommandsParser::UINT);
}

tree::TerminalNode* TracerCommandsParser::WatchpointContext::UINT(size_t i) {
  return getToken(TracerCommandsParser::UINT, i);
}

std::vector<TracerCommandsParser::ValueContext *> TracerCommandsParser::WatchpointContext::value() {
  return getRuleContexts<TracerCommandsParser::ValueContext>();
}

TracerCommandsParser::ValueContext* TracerCommandsParser::WatchpointContext::value(size_t i) {
  return getRuleContext<TracerCommandsParser::ValueContext>(i);
}


size_t TracerCommandsParser::WatchpointContext::getRuleIndex() const {
  return TracerCommandsParser::RuleWatchpoint;
}


std::any TracerCommandsParser::WatchpointContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitWatchpoint(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::WatchpointContext* TracerCommandsParser::watchpoint() {
  WatchpointContext *_localctx = _tracker.createInstance<WatchpointContext>(_ctx, getState());
  enterRule(_localctx, 102, TracerCommandsParser::RuleWatchpoint);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(764);
    match(TracerCommandsParser::T__146);
    setState(771);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 35185448976384) != 0) || ((((_la - 160) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 160)) & 131075) != 0)) {
      setState(769);
      _errHandler->sync(this);
      switch (_input->LA(1)) {
        case TracerCommandsParser::T__19:
        case TracerCommandsParser::T__20:
        case TracerCommandsParser::T__159:
        case TracerCommandsParser::T__160: {
          setState(765);
          _la = _input->LA(1);
          if (!(_la == TracerCommandsParser::T__19

          || _la == TracerCommandsParser::T__20 || _la == TracerCommandsParser::T__159

          || _la == TracerCommandsParser::T__160)) {
          _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(766);
          match(TracerCommandsParser::UINT);
          break;
        }

        case TracerCommandsParser::T__29:
        case TracerCommandsParser::T__44:
        case TracerCommandsParser::T__176: {
          setState(767);
          _la = _input->LA(1);
          if (!(_la == TracerCommandsParser::T__29

          || _la == TracerCommandsParser::T__44 || _la == TracerCommandsParser::T__176)) {
          _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(768);
          value();
          break;
        }

      default:
        throw NoViableAltException(this);
      }
      setState(773);
      _errHandler->sync(this);
      _la = _input->LA(1);
    }

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- StackFilterContext ------------------------------------------------------------------

TracerCommandsParser::StackFilterContext::StackFilterContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

TracerCommandsParser::ValueContext* TracerCommandsParser::StackFilterContext::value() {
  return getRuleContext<TracerCommandsParser::ValueContext>(0);
}


size_t TracerCommandsParser::StackFilterContext::getRuleIndex() const {
  return TracerCommandsParser::RuleStackFilter;
}


std::any TracerCommandsParser::StackFilterContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitStackFilter(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::StackFilterContext* TracerCommandsParser::stackFilter() {
  StackFilterContext *_localctx = _tracker.createInstance<StackFilterContext>(_ctx, getState());
  enterRule(_localctx, 104, TracerCommandsParser::RuleStackFilter);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(774);
    match(TracerCommandsParser::T__177);
    setState(775);
    value();

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ContextEventBytesContext ------------------------------------------------------------------

TracerCommandsParser::ContextEventBytesContext::ContextEventBytesContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* TracerCommandsParser::ContextEventBytesContext::UINT() {
  return getToken(TracerCommandsParser::UINT, 0);
}


size_t TracerCommandsParser::ContextEventBytesContext::getRuleIndex() const {
  return TracerCommandsParser::RuleContextEventBytes;
}


std::any TracerCommandsParser::ContextEventBytesContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitContextEventBytes(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::ContextEventBytesContext* TracerCommandsParser::contextEventBytes() {
  ContextEventBytesContext *_localctx = _tracker.createInstance<ContextEventBytesContext>(_ctx, getState());
  enterRule(_localctx, 106, TracerCommandsParser::RuleContextEventBytes);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(777);
    _la = _input->LA(1);
    if (!(_la == TracerCommandsParser::T__178

    || _la == TracerCommandsParser::T__179)) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(778);
    match(TracerCommandsParser::UINT);

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- ExtraCompileOptionContext ------------------------------------------------------------------

TracerCommandsParser::ExtraCompileOptionContext::ExtraCompileOptionContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<TracerCommandsParser::ValueContext *> TracerCommandsParser::ExtraCompileOptionContext::value() {
  return getRuleContexts<TracerCommandsParser::ValueContext>();
}

TracerCommandsParser::ValueContext* TracerCommandsParser::ExtraCompileOptionContext::value(size_t i) {
  return getRuleContext<TracerCommandsParser::ValueContext>(i);
}

std::vector<tree::TerminalNode *> TracerCommandsParser::ExtraCompileOptionContext::SHELL() {
  return getTokens(TracerCommandsParser::SHELL);
}

tree::TerminalNode* TracerCommandsParser::ExtraCompileOptionContext::SHELL(size_t i) {
  return getToken(TracerCommandsParser::SHELL, i);
}


size_t TracerCommandsParser::ExtraCompileOptionContext::getRuleIndex() const {
  return TracerCommandsParser::RuleExtraCompileOption;
}


std::any TracerCommandsParser::ExtraCompileOptionContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitExtraCompileOption(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::ExtraCompileOptionContext* TracerCommandsParser::extraCompileOption() {
  ExtraCompileOptionContext *_localctx = _tracker.createInstance<ExtraCompileOptionContext>(_ctx, getState());
  enterRule(_localctx, 108, TracerCommandsParser::RuleExtraCompileOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(789);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__180: {
        enterOuterAlt(_localctx, 1);
        setState(780);
        match(TracerCommandsParser::T__180);
        setState(781);
        value();
        break;
      }

      case TracerCommandsParser::T__181: {
        enterOuterAlt(_localctx, 2);
        setState(782);
        match(TracerCommandsParser::T__181);
        setState(785);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(785);
          _errHandler->sync(this);
          switch (_input->LA(1)) {
            case TracerCommandsParser::T__1:
            case TracerCommandsParser::T__2:
            case TracerCommandsParser::T__3:
            case TracerCommandsParser::T__4:
            case TracerCommandsParser::T__5:
            case TracerCommandsParser::T__8:
            case TracerCommandsParser::T__10:
            case TracerCommandsParser::T__12:
            case TracerCommandsParser::T__13:
            case TracerCommandsParser::T__14:
            case TracerCommandsParser::T__15:
            case TracerCommandsParser::T__21:
            case TracerCommandsParser::T__22:
            case TracerCommandsParser::T__23:
            case TracerCommandsParser::T__24:
            case TracerCommandsParser::T__25:
            case TracerCommandsParser::T__26:
            case TracerCommandsParser::T__38:
            case TracerCommandsParser::T__43:
            case TracerCommandsParser::T__46:
            case TracerCommandsParser::T__47:
            case TracerCommandsParser::T__48:
            case TracerCommandsParser::T__49:
            case TracerCommandsParser::T__51:
            case TracerCommandsParser::T__52:
            case TracerCommandsParser::T__53:
            case TracerCommandsParser::T__82:
            case TracerCommandsParser::T__83:
            case TracerCommandsParser::T__85:
            case TracerCommandsParser::T__87:
            case TracerCommandsParser::T__88:
            case TracerCommandsParser::T__89:
            case TracerCommandsParser::T__90:
            case TracerCommandsParser::T__91:
            case TracerCommandsParser::T__92:
            case TracerCommandsParser::T__93:
            case TracerCommandsParser::T__94:
            case TracerCommandsParser::T__95:
            case TracerCommandsParser::T__96:
            case TracerCommandsParser::T__100:
            case TracerCommandsParser::T__101:
            case TracerCommandsParser::T__102:
            case TracerCommandsParser::T__106:
            case TracerCommandsParser::T__107:
            case TracerCommandsParser::T__108:
            case TracerCommandsParser::T__109:
            case TracerCommandsParser::T__110:
            case TracerCommandsParser::T__111:
            case TracerCommandsParser::T__112:
            case TracerCommandsParser::T__113:
            case TracerCommandsParser::T__114:
            case TracerCommandsParser::T__115:
            case TracerCommandsParser::T__116:
            case TracerCommandsParser::T__117:
            case TracerCommandsParser::T__118:
            case TracerCommandsParser::T__119:
            case TracerCommandsParser::T__120:
            case TracerCommandsParser::T__121:
            case TracerCommandsParser::T__126:
            case TracerCommandsParser::T__127:
            case TracerCommandsParser::T__128:
            case TracerCommandsParser::T__129:
            case TracerCommandsParser::T__130:
            case TracerCommandsParser::T__131:
            case TracerCommandsParser::T__132:
            case TracerCommandsParser::T__133:
            case TracerCommandsParser::T__134:
            case TracerCommandsParser::T__135:
            case TracerCommandsParser::T__136:
            case TracerCommandsParser::T__137:
            case TracerCommandsParser::T__138:
            case TracerCommandsParser::T__139:
            case TracerCommandsParser::T__140:
            case TracerCommandsParser::T__141:
            case TracerCommandsParser::T__142:
            case TracerCommandsParser::T__143:
            case TracerCommandsParser::T__144:
            case TracerCommandsParser::T__145:
            case TracerCommandsParser::T__146:
            case TracerCommandsParser::T__147:
            case TracerCommandsParser::T__148:
            case TracerCommandsParser::T__149:
            case TracerCommandsParser::T__150:
            case TracerCommandsParser::T__151:
            case TracerCommandsParser::T__152:
            case TracerCommandsParser::T__153:
            case TracerCommandsParser::T__154:
            case TracerCommandsParser::UINT:
            case TracerCommandsParser::STRING:
            case TracerCommandsParser::STACK:
            case TracerCommandsParser::WORD: {
              setState(783);
              value();
              break;
            }

            case TracerCommandsParser::SHELL: {
              setState(784);
              match(TracerCommandsParser::SHELL);
              break;
            }

          default:
            throw NoViableAltException(this);
          }
          setState(787);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 33654401923148412) != 0) || ((((_la - 83) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 83)) & -16492689326101) != 0) || ((((_la - 147) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 147)) & 73048803770879) != 0));
        break;
      }

    default:
      throw NoViableAltException(this);
    }

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

//----------------- CwdOptionContext ------------------------------------------------------------------

TracerCommandsParser::CwdOptionContext::CwdOptionContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* TracerCommandsParser::CwdOptionContext::CWD() {
  return getToken(TracerCommandsParser::CWD, 0);
}

TracerCommandsParser::ValueContext* TracerCommandsParser::CwdOptionContext::value() {
  return getRuleContext<TracerCommandsParser::ValueContext>(0);
}


size_t TracerCommandsParser::CwdOptionContext::getRuleIndex() const {
  return TracerCommandsParser::RuleCwdOption;
}


std::any TracerCommandsParser::CwdOptionContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitCwdOption(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::CwdOptionContext* TracerCommandsParser::cwdOption() {
  CwdOptionContext *_localctx = _tracker.createInstance<CwdOptionContext>(_ctx, getState());
  enterRule(_localctx, 110, TracerCommandsParser::RuleCwdOption);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(791);
    match(TracerCommandsParser::CWD);
    setState(792);
    value();

  }
  catch (RecognitionException &e) {
    _errHandler->reportError(this, e);
    _localctx->exception = std::current_exception();
    _errHandler->recover(this, _localctx->exception);
  }

  return _localctx;
}

void TracerCommandsParser::initialize() {
#if ANTLR4_USE_THREAD_LOCAL_CACHE
  tracercommandsParserInitialize();
#else
  ::antlr4::internal::call_once(tracercommandsParserOnceFlag, tracercommandsParserInitialize);
#endif
}
