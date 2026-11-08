
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
      "sessions", "use", "status", "detach", "modules", "threads", "symbols",
      "symbolOption", "probe", "prepareOption", "resolveOption", "validateOption",
      "deployOption", "interceptor", "events", "collect", "record", "streamOption",
      "cpp", "shell", "complete", "type", "help", "quit", "bulkTrace", "bulkTraceOption",
      "value", "keyword", "memory", "probeEntry", "probeReload", "targetLaunch",
      "launchOption", "childList", "targetCall", "localPerf", "targetLibrary",
      "instructions", "instructionOption", "watchpoint", "stackFilter",
      "contextEventBytes", "extraCompileOption", "cwdOption"
    },
    std::vector<std::string>{
      "", "'%'", "'server'", "'start'", "'connect'", "'status'", "'stop'",
      "'--executable'", "'--agent'", "'attach'", "'--start-time'", "'processes'",
      "'--filter'", "'sessions'", "'use'", "'detach'", "'modules'", "'--details'",
      "'--elf'", "'--watch'", "'--timeout'", "'--count'", "'threads'", "'symbols'",
      "'--module'", "'--page'", "'--address'", "'--mapping'", "'--limit'",
      "'--addresses'", "'probe'", "'prepare'", "'resolve'", "'inject'",
      "'--callback'", "'validate'", "'remove'", "'probes'", "'--include'",
      "'--compdb'", "'--translation-unit'", "'--output'", "'--compile-option'",
      "'--prepared'", "'--prepared-mode'", "'--allow-arg-overrides'", "'--allow-result-override'",
      "'--allow-replacement'", "'--reclaimable'", "'--allow-call-original'",
      "'--trace-exceptions'", "'--stack-depth'", "'--print-arguments'",
      "'--print-result'", "'interceptor'", "'load'", "'--as'", "'edit'",
      "'show'", "'--target'", "'upload'", "'events'", "'list'", "'--index'",
      "'calls'", "'stats'", "'clear'", "'open'", "'export'", "'--format'",
      "'json'", "'collect'", "'record'", "'--after'", "'--probe'", "'--phase'",
      "'PRE'", "'POST'", "'EXCEPTION'", "'--name'", "'--session'", "'--all'",
      "'cpp'", "'reset'", "'import'", "'generate'", "'--type'", "'options'",
      "'add'", "'compdb'", "'--file'", "'shell'", "'complete'", "'type'",
      "'help'", "'quit'", "'exit'", "'trace'", "'preview'", "'apply'", "'--manifest'",
      "'--exclude'", "'--exact'", "'--save'", "'memory'", "'maps'", "'read'",
      "'scan'", "'patch'", "'restore'", "'import-hook'", "'monitor-start'",
      "'monitor-stop'", "'entry'", "'reload'", "'launch'", "'resume'", "'terminate'",
      "'call'", "'call-result'", "'syscalls'", "'sample'", "'cpu'", "'instructions'",
      "'watchpoint'", "'library'", "'allocate'", "'free'", "'write'", "'replay'",
      "'children'", "'faults'", "'allocations'", "'--pattern'", "'--elf-address'",
      "'--expect'", "'--replace'", "'--tid'", "'--size'", "'--symbol'",
      "'--original'", "'--abi'", "'--arg'", "'--env'", "'--stdin'", "'--stdout'",
      "'--stderr'", "'--preload'", "'--supervise'", "'--gate-children'",
      "'--memory-size'", "'--registers'", "'--discover'", "'--callback-library'",
      "'--memory-address'", "'--access'", "'--stack-application-only'",
      "'--context-event-bytes'", "'--context-transport-memory-bytes'", "'--extra-arg'",
      "'--extra-args'", "", "", "", "'--metadata-only'", "'--sqlite'", "'stack'",
      "'--cwd'"
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
      "", "", "", "", "", "", "", "", "UINT", "STRING", "SHELL", "METADATA_ONLY",
      "SQLITE", "STACK", "CWD", "WORD", "WS"
    }
  );
  static const int32_t serializedATNSegment[] = {
    4,1,169,710,2,0,7,0,2,1,7,1,2,2,7,2,2,3,7,3,2,4,7,4,2,5,7,5,2,6,7,6,2,
    7,7,7,2,8,7,8,2,9,7,9,2,10,7,10,2,11,7,11,2,12,7,12,2,13,7,13,2,14,7,
    14,2,15,7,15,2,16,7,16,2,17,7,17,2,18,7,18,2,19,7,19,2,20,7,20,2,21,7,
    21,2,22,7,22,2,23,7,23,2,24,7,24,2,25,7,25,2,26,7,26,2,27,7,27,2,28,7,
    28,2,29,7,29,2,30,7,30,2,31,7,31,2,32,7,32,2,33,7,33,2,34,7,34,2,35,7,
    35,2,36,7,36,2,37,7,37,2,38,7,38,2,39,7,39,2,40,7,40,2,41,7,41,2,42,7,
    42,2,43,7,43,2,44,7,44,2,45,7,45,2,46,7,46,2,47,7,47,2,48,7,48,2,49,7,
    49,1,0,1,0,1,0,1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,3,1,137,8,1,1,2,1,2,1,2,5,2,142,8,2,10,2,12,2,145,9,2,1,2,1,2,
    1,2,1,2,3,2,151,8,2,1,3,1,3,1,3,3,3,156,8,3,1,4,1,4,1,4,1,4,3,4,162,8,
    4,3,4,164,8,4,1,5,1,5,1,5,3,5,169,8,5,1,6,1,6,1,7,1,7,1,7,1,8,1,8,1,9,
    1,9,3,9,180,8,9,1,10,1,10,1,10,1,10,1,10,1,10,1,10,5,10,189,8,10,10,10,
    12,10,192,9,10,3,10,194,8,10,1,11,1,11,1,12,1,12,5,12,200,8,12,10,12,
    12,12,203,9,12,1,13,1,13,1,13,1,13,1,13,3,13,210,8,13,1,14,1,14,1,14,
    1,14,4,14,216,8,14,11,14,12,14,217,1,14,1,14,1,14,1,14,5,14,224,8,14,
    10,14,12,14,227,9,14,1,14,1,14,1,14,1,14,1,14,1,14,5,14,235,8,14,10,14,
    12,14,238,9,14,1,14,1,14,1,14,1,14,1,14,1,14,5,14,246,8,14,10,14,12,14,
    249,9,14,1,14,1,14,1,14,3,14,254,8,14,3,14,256,8,14,1,14,1,14,3,14,260,
    8,14,3,14,262,8,14,1,15,1,15,1,15,1,15,1,15,3,15,269,8,15,1,16,1,16,1,
    16,3,16,274,8,16,1,17,1,17,1,17,1,17,1,17,1,17,1,17,3,17,283,8,17,1,18,
    1,18,1,18,1,18,1,18,1,18,3,18,291,8,18,1,19,1,19,1,19,1,19,1,19,1,19,
    1,19,1,19,1,19,1,19,1,19,1,19,1,19,1,19,1,19,5,19,308,8,19,10,19,12,19,
    311,9,19,1,19,1,19,1,19,1,19,1,19,5,19,318,8,19,10,19,12,19,321,9,19,
    3,19,323,8,19,1,20,1,20,5,20,327,8,20,10,20,12,20,330,9,20,1,20,1,20,
    1,20,1,20,3,20,336,8,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,
    1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,
    1,20,1,20,1,20,1,20,1,20,3,20,366,8,20,1,21,1,21,5,21,370,8,21,10,21,
    12,21,373,9,21,1,22,1,22,1,22,1,22,5,22,379,8,22,10,22,12,22,382,9,22,
    1,23,1,23,1,23,1,23,1,23,1,23,1,23,3,23,391,8,23,1,24,1,24,1,24,1,24,
    1,24,1,24,1,24,1,24,5,24,401,8,24,10,24,12,24,404,9,24,1,24,1,24,1,24,
    1,24,4,24,410,8,24,11,24,12,24,411,1,24,1,24,1,24,1,24,1,24,1,24,3,24,
    420,8,24,3,24,422,8,24,1,25,1,25,1,25,1,26,1,26,1,26,1,27,1,27,1,27,1,
    28,1,28,3,28,435,8,28,1,28,4,28,438,8,28,11,28,12,28,439,3,28,442,8,28,
    1,29,1,29,1,30,1,30,1,30,4,30,449,8,30,11,30,12,30,450,1,30,1,30,1,30,
    1,30,1,30,3,30,458,8,30,1,30,1,30,3,30,462,8,30,1,31,1,31,1,31,1,32,1,
    32,1,32,1,32,3,32,471,8,32,1,33,1,33,1,34,1,34,1,34,1,34,1,34,1,34,1,
    34,1,34,1,34,1,34,3,34,485,8,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,
    34,1,34,1,34,3,34,497,8,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,
    34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,
    34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,3,34,533,8,
    34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,
    34,1,34,3,34,550,8,34,1,35,1,35,1,35,1,35,1,35,1,35,1,35,3,35,559,8,35,
    1,35,1,35,1,35,1,35,1,35,1,35,1,35,1,35,1,35,3,35,570,8,35,1,35,3,35,
    573,8,35,1,36,1,36,1,36,1,36,1,36,1,36,1,36,1,36,1,36,5,36,584,8,36,10,
    36,12,36,587,9,36,5,36,589,8,36,10,36,12,36,592,9,36,1,37,1,37,1,37,5,
    37,597,8,37,10,37,12,37,600,9,37,1,37,1,37,3,37,604,8,37,3,37,606,8,37,
    1,38,1,38,1,38,1,38,1,38,1,38,3,38,614,8,38,1,39,1,39,1,40,1,40,1,40,
    1,40,1,40,1,40,5,40,624,8,40,10,40,12,40,627,9,40,1,40,1,40,1,40,3,40,
    632,8,40,3,40,634,8,40,1,41,1,41,1,41,1,41,3,41,640,8,41,1,41,1,41,5,
    41,644,8,41,10,41,12,41,647,9,41,1,42,1,42,1,42,1,42,1,43,1,43,1,43,5,
    43,656,8,43,10,43,12,43,659,9,43,1,43,1,43,1,43,3,43,664,8,43,1,43,1,
    43,3,43,668,8,43,1,44,1,44,1,44,1,44,1,44,1,44,1,44,1,44,3,44,678,8,44,
    1,45,1,45,1,45,1,45,1,45,5,45,685,8,45,10,45,12,45,688,9,45,1,46,1,46,
    1,46,1,47,1,47,1,47,1,48,1,48,1,48,1,48,1,48,4,48,701,8,48,11,48,12,48,
    702,3,48,705,8,48,1,49,1,49,1,49,1,49,0,0,50,0,2,4,6,8,10,12,14,16,18,
    20,22,24,26,28,30,32,34,36,38,40,42,44,46,48,50,52,54,56,58,60,62,64,
    66,68,70,72,74,76,78,80,82,84,86,88,90,92,94,96,98,0,22,1,0,7,8,1,0,20,
    21,2,0,12,12,24,27,2,0,24,24,38,41,3,0,24,24,38,40,42,44,2,0,161,161,
    168,168,2,0,20,21,73,74,1,0,76,78,2,0,56,56,79,80,1,0,84,85,2,0,41,41,
    86,86,1,0,95,96,3,0,24,24,38,38,100,103,18,0,2,6,9,9,11,11,13,16,22,23,
    30,33,35,37,54,55,57,58,60,62,64,68,70,72,76,78,82,85,87,89,91,99,104,
    132,166,166,2,0,109,109,126,127,1,0,116,117,2,0,142,147,167,167,2,0,122,
    122,132,132,2,0,20,21,137,138,2,0,26,27,153,154,2,0,26,27,155,155,1,0,
    157,158,818,0,100,1,0,0,0,2,136,1,0,0,0,4,138,1,0,0,0,6,155,1,0,0,0,8,
    157,1,0,0,0,10,165,1,0,0,0,12,170,1,0,0,0,14,172,1,0,0,0,16,175,1,0,0,
    0,18,177,1,0,0,0,20,181,1,0,0,0,22,195,1,0,0,0,24,197,1,0,0,0,26,209,
    1,0,0,0,28,261,1,0,0,0,30,268,1,0,0,0,32,273,1,0,0,0,34,282,1,0,0,0,36,
    290,1,0,0,0,38,292,1,0,0,0,40,324,1,0,0,0,42,367,1,0,0,0,44,374,1,0,0,
    0,46,390,1,0,0,0,48,392,1,0,0,0,50,423,1,0,0,0,52,426,1,0,0,0,54,429,
    1,0,0,0,56,432,1,0,0,0,58,443,1,0,0,0,60,445,1,0,0,0,62,463,1,0,0,0,64,
    470,1,0,0,0,66,472,1,0,0,0,68,474,1,0,0,0,70,551,1,0,0,0,72,574,1,0,0,
    0,74,605,1,0,0,0,76,613,1,0,0,0,78,615,1,0,0,0,80,633,1,0,0,0,82,639,
    1,0,0,0,84,648,1,0,0,0,86,652,1,0,0,0,88,677,1,0,0,0,90,679,1,0,0,0,92,
    689,1,0,0,0,94,692,1,0,0,0,96,704,1,0,0,0,98,706,1,0,0,0,100,101,5,1,
    0,0,101,102,3,2,1,0,102,103,5,0,0,1,103,1,1,0,0,0,104,137,3,4,2,0,105,
    137,3,8,4,0,106,137,3,10,5,0,107,137,3,12,6,0,108,137,3,14,7,0,109,137,
    3,16,8,0,110,137,3,18,9,0,111,137,3,20,10,0,112,137,3,22,11,0,113,137,
    3,24,12,0,114,137,3,28,14,0,115,137,3,38,19,0,116,137,3,40,20,0,117,137,
    3,42,21,0,118,137,3,44,22,0,119,137,3,48,24,0,120,137,3,50,25,0,121,137,
    3,52,26,0,122,137,3,54,27,0,123,137,3,56,28,0,124,137,3,58,29,0,125,137,
    3,60,30,0,126,137,3,68,34,0,127,137,3,70,35,0,128,137,3,72,36,0,129,137,
    3,74,37,0,130,137,3,80,40,0,131,137,3,82,41,0,132,137,3,86,43,0,133,137,
    3,90,45,0,134,137,3,84,42,0,135,137,3,78,39,0,136,104,1,0,0,0,136,105,
    1,0,0,0,136,106,1,0,0,0,136,107,1,0,0,0,136,108,1,0,0,0,136,109,1,0,0,
    0,136,110,1,0,0,0,136,111,1,0,0,0,136,112,1,0,0,0,136,113,1,0,0,0,136,
    114,1,0,0,0,136,115,1,0,0,0,136,116,1,0,0,0,136,117,1,0,0,0,136,118,1,
    0,0,0,136,119,1,0,0,0,136,120,1,0,0,0,136,121,1,0,0,0,136,122,1,0,0,0,
    136,123,1,0,0,0,136,124,1,0,0,0,136,125,1,0,0,0,136,126,1,0,0,0,136,127,
    1,0,0,0,136,128,1,0,0,0,136,129,1,0,0,0,136,130,1,0,0,0,136,131,1,0,0,
    0,136,132,1,0,0,0,136,133,1,0,0,0,136,134,1,0,0,0,136,135,1,0,0,0,137,
    3,1,0,0,0,138,150,5,2,0,0,139,143,5,3,0,0,140,142,3,6,3,0,141,140,1,0,
    0,0,142,145,1,0,0,0,143,141,1,0,0,0,143,144,1,0,0,0,144,151,1,0,0,0,145,
    143,1,0,0,0,146,147,5,4,0,0,147,151,3,64,32,0,148,151,5,5,0,0,149,151,
    5,6,0,0,150,139,1,0,0,0,150,146,1,0,0,0,150,148,1,0,0,0,150,149,1,0,0,
    0,151,5,1,0,0,0,152,153,7,0,0,0,153,156,3,64,32,0,154,156,3,94,47,0,155,
    152,1,0,0,0,155,154,1,0,0,0,156,7,1,0,0,0,157,163,5,9,0,0,158,161,5,161,
    0,0,159,160,5,10,0,0,160,162,5,161,0,0,161,159,1,0,0,0,161,162,1,0,0,
    0,162,164,1,0,0,0,163,158,1,0,0,0,163,164,1,0,0,0,164,9,1,0,0,0,165,168,
    5,11,0,0,166,167,5,12,0,0,167,169,3,64,32,0,168,166,1,0,0,0,168,169,1,
    0,0,0,169,11,1,0,0,0,170,171,5,13,0,0,171,13,1,0,0,0,172,173,5,14,0,0,
    173,174,3,64,32,0,174,15,1,0,0,0,175,176,5,5,0,0,176,17,1,0,0,0,177,179,
    5,15,0,0,178,180,3,64,32,0,179,178,1,0,0,0,179,180,1,0,0,0,180,19,1,0,
    0,0,181,193,5,16,0,0,182,194,5,17,0,0,183,184,5,18,0,0,184,194,3,64,32,
    0,185,190,5,19,0,0,186,187,7,1,0,0,187,189,5,161,0,0,188,186,1,0,0,0,
    189,192,1,0,0,0,190,188,1,0,0,0,190,191,1,0,0,0,191,194,1,0,0,0,192,190,
    1,0,0,0,193,182,1,0,0,0,193,183,1,0,0,0,193,185,1,0,0,0,193,194,1,0,0,
    0,194,21,1,0,0,0,195,196,5,22,0,0,196,23,1,0,0,0,197,201,5,23,0,0,198,
    200,3,26,13,0,199,198,1,0,0,0,200,203,1,0,0,0,201,199,1,0,0,0,201,202,
    1,0,0,0,202,25,1,0,0,0,203,201,1,0,0,0,204,205,7,2,0,0,205,210,3,64,32,
    0,206,207,5,28,0,0,207,210,5,161,0,0,208,210,5,29,0,0,209,204,1,0,0,0,
    209,206,1,0,0,0,209,208,1,0,0,0,210,27,1,0,0,0,211,255,5,30,0,0,212,213,
    5,31,0,0,213,215,3,64,32,0,214,216,3,30,15,0,215,214,1,0,0,0,216,217,
    1,0,0,0,217,215,1,0,0,0,217,218,1,0,0,0,218,256,1,0,0,0,219,220,5,32,
    0,0,220,225,3,64,32,0,221,224,3,32,16,0,222,224,3,98,49,0,223,221,1,0,
    0,0,223,222,1,0,0,0,224,227,1,0,0,0,225,223,1,0,0,0,225,226,1,0,0,0,226,
    256,1,0,0,0,227,225,1,0,0,0,228,229,5,33,0,0,229,230,3,64,32,0,230,231,
    5,34,0,0,231,236,3,64,32,0,232,235,3,36,18,0,233,235,3,98,49,0,234,232,
    1,0,0,0,234,233,1,0,0,0,235,238,1,0,0,0,236,234,1,0,0,0,236,237,1,0,0,
    0,237,256,1,0,0,0,238,236,1,0,0,0,239,240,5,35,0,0,240,241,3,64,32,0,
    241,242,5,34,0,0,242,247,3,64,32,0,243,246,3,34,17,0,244,246,3,98,49,
    0,245,243,1,0,0,0,245,244,1,0,0,0,246,249,1,0,0,0,247,245,1,0,0,0,247,
    248,1,0,0,0,248,256,1,0,0,0,249,247,1,0,0,0,250,251,5,36,0,0,251,253,
    5,161,0,0,252,254,3,98,49,0,253,252,1,0,0,0,253,254,1,0,0,0,254,256,1,
    0,0,0,255,212,1,0,0,0,255,219,1,0,0,0,255,228,1,0,0,0,255,239,1,0,0,0,
    255,250,1,0,0,0,256,262,1,0,0,0,257,259,5,37,0,0,258,260,3,98,49,0,259,
    258,1,0,0,0,259,260,1,0,0,0,260,262,1,0,0,0,261,211,1,0,0,0,261,257,1,
    0,0,0,262,29,1,0,0,0,263,264,7,3,0,0,264,269,3,64,32,0,265,269,5,164,
    0,0,266,269,3,96,48,0,267,269,3,98,49,0,268,263,1,0,0,0,268,265,1,0,0,
    0,268,266,1,0,0,0,268,267,1,0,0,0,269,31,1,0,0,0,270,271,7,4,0,0,271,
    274,3,64,32,0,272,274,3,96,48,0,273,270,1,0,0,0,273,272,1,0,0,0,274,33,
    1,0,0,0,275,283,3,32,16,0,276,283,5,45,0,0,277,283,5,46,0,0,278,283,5,
    47,0,0,279,283,5,48,0,0,280,283,5,49,0,0,281,283,5,50,0,0,282,275,1,0,
    0,0,282,276,1,0,0,0,282,277,1,0,0,0,282,278,1,0,0,0,282,279,1,0,0,0,282,
    280,1,0,0,0,282,281,1,0,0,0,283,35,1,0,0,0,284,291,3,34,17,0,285,286,
    5,51,0,0,286,291,7,5,0,0,287,291,5,52,0,0,288,291,5,53,0,0,289,291,3,
    92,46,0,290,284,1,0,0,0,290,285,1,0,0,0,290,287,1,0,0,0,290,288,1,0,0,
    0,290,289,1,0,0,0,291,37,1,0,0,0,292,322,5,54,0,0,293,294,5,55,0,0,294,
    295,3,64,32,0,295,296,5,56,0,0,296,297,3,64,32,0,297,323,1,0,0,0,298,
    299,5,57,0,0,299,323,3,64,32,0,300,301,5,58,0,0,301,323,3,64,32,0,302,
    303,5,35,0,0,303,304,3,64,32,0,304,305,5,59,0,0,305,309,3,64,32,0,306,
    308,3,34,17,0,307,306,1,0,0,0,308,311,1,0,0,0,309,307,1,0,0,0,309,310,
    1,0,0,0,310,323,1,0,0,0,311,309,1,0,0,0,312,313,5,60,0,0,313,314,3,64,
    32,0,314,315,5,59,0,0,315,319,3,64,32,0,316,318,3,36,18,0,317,316,1,0,
    0,0,318,321,1,0,0,0,319,317,1,0,0,0,319,320,1,0,0,0,320,323,1,0,0,0,321,
    319,1,0,0,0,322,293,1,0,0,0,322,298,1,0,0,0,322,300,1,0,0,0,322,302,1,
    0,0,0,322,312,1,0,0,0,323,39,1,0,0,0,324,365,5,61,0,0,325,327,3,46,23,
    0,326,325,1,0,0,0,327,330,1,0,0,0,328,326,1,0,0,0,328,329,1,0,0,0,329,
    366,1,0,0,0,330,328,1,0,0,0,331,332,5,62,0,0,332,335,3,64,32,0,333,334,
    5,28,0,0,334,336,5,161,0,0,335,333,1,0,0,0,335,336,1,0,0,0,336,366,1,
    0,0,0,337,338,5,58,0,0,338,339,3,64,32,0,339,340,5,63,0,0,340,341,5,161,
    0,0,341,366,1,0,0,0,342,343,5,166,0,0,343,344,3,64,32,0,344,345,5,63,
    0,0,345,346,5,161,0,0,346,366,1,0,0,0,347,348,5,64,0,0,348,366,3,64,32,
    0,349,350,5,65,0,0,350,366,3,64,32,0,351,352,5,66,0,0,352,366,3,64,32,
    0,353,354,5,67,0,0,354,355,3,64,32,0,355,356,5,56,0,0,356,357,3,64,32,
    0,357,366,1,0,0,0,358,359,5,68,0,0,359,360,3,64,32,0,360,361,5,69,0,0,
    361,362,5,70,0,0,362,363,5,41,0,0,363,364,3,64,32,0,364,366,1,0,0,0,365,
    328,1,0,0,0,365,331,1,0,0,0,365,337,1,0,0,0,365,342,1,0,0,0,365,347,1,
    0,0,0,365,349,1,0,0,0,365,351,1,0,0,0,365,353,1,0,0,0,365,358,1,0,0,0,
    366,41,1,0,0,0,367,371,5,71,0,0,368,370,3,46,23,0,369,368,1,0,0,0,370,
    373,1,0,0,0,371,369,1,0,0,0,371,372,1,0,0,0,372,43,1,0,0,0,373,371,1,
    0,0,0,374,375,5,72,0,0,375,380,3,64,32,0,376,379,3,46,23,0,377,379,5,
    165,0,0,378,376,1,0,0,0,378,377,1,0,0,0,379,382,1,0,0,0,380,378,1,0,0,
    0,380,381,1,0,0,0,381,45,1,0,0,0,382,380,1,0,0,0,383,384,7,6,0,0,384,
    391,5,161,0,0,385,386,5,75,0,0,386,391,7,7,0,0,387,388,7,8,0,0,388,391,
    3,64,32,0,389,391,5,81,0,0,390,383,1,0,0,0,390,385,1,0,0,0,390,387,1,
    0,0,0,390,389,1,0,0,0,391,47,1,0,0,0,392,421,5,82,0,0,393,394,5,55,0,
    0,394,422,3,64,32,0,395,422,5,83,0,0,396,397,7,9,0,0,397,402,3,64,32,
    0,398,399,7,10,0,0,399,401,3,64,32,0,400,398,1,0,0,0,401,404,1,0,0,0,
    402,400,1,0,0,0,402,403,1,0,0,0,403,422,1,0,0,0,404,402,1,0,0,0,405,419,
    5,87,0,0,406,409,5,88,0,0,407,410,3,64,32,0,408,410,5,163,0,0,409,407,
    1,0,0,0,409,408,1,0,0,0,410,411,1,0,0,0,411,409,1,0,0,0,411,412,1,0,0,
    0,412,420,1,0,0,0,413,420,5,66,0,0,414,415,5,89,0,0,415,416,3,64,32,0,
    416,417,5,90,0,0,417,418,3,64,32,0,418,420,1,0,0,0,419,406,1,0,0,0,419,
    413,1,0,0,0,419,414,1,0,0,0,419,420,1,0,0,0,420,422,1,0,0,0,421,393,1,
    0,0,0,421,395,1,0,0,0,421,396,1,0,0,0,421,405,1,0,0,0,422,49,1,0,0,0,
    423,424,5,91,0,0,424,425,3,64,32,0,425,51,1,0,0,0,426,427,5,92,0,0,427,
    428,3,64,32,0,428,53,1,0,0,0,429,430,5,93,0,0,430,431,3,64,32,0,431,55,
    1,0,0,0,432,441,5,94,0,0,433,435,5,1,0,0,434,433,1,0,0,0,434,435,1,0,
    0,0,435,437,1,0,0,0,436,438,3,64,32,0,437,436,1,0,0,0,438,439,1,0,0,0,
    439,437,1,0,0,0,439,440,1,0,0,0,440,442,1,0,0,0,441,434,1,0,0,0,441,442,
    1,0,0,0,442,57,1,0,0,0,443,444,7,11,0,0,444,59,1,0,0,0,445,461,5,97,0,
    0,446,448,5,98,0,0,447,449,3,62,31,0,448,447,1,0,0,0,449,450,1,0,0,0,
    450,448,1,0,0,0,450,451,1,0,0,0,451,462,1,0,0,0,452,453,5,99,0,0,453,
    454,5,100,0,0,454,457,3,64,32,0,455,456,5,34,0,0,456,458,3,64,32,0,457,
    455,1,0,0,0,457,458,1,0,0,0,458,462,1,0,0,0,459,460,5,36,0,0,460,462,
    3,64,32,0,461,446,1,0,0,0,461,452,1,0,0,0,461,459,1,0,0,0,462,61,1,0,
    0,0,463,464,7,12,0,0,464,465,3,64,32,0,465,63,1,0,0,0,466,471,5,162,0,
    0,467,471,5,168,0,0,468,471,5,161,0,0,469,471,3,66,33,0,470,466,1,0,0,
    0,470,467,1,0,0,0,470,468,1,0,0,0,470,469,1,0,0,0,471,65,1,0,0,0,472,
    473,7,13,0,0,473,67,1,0,0,0,474,549,5,104,0,0,475,550,5,105,0,0,476,477,
    5,106,0,0,477,478,3,64,32,0,478,479,5,27,0,0,479,484,3,64,32,0,480,481,
    5,21,0,0,481,485,5,161,0,0,482,483,5,86,0,0,483,485,3,64,32,0,484,480,
    1,0,0,0,484,482,1,0,0,0,485,550,1,0,0,0,486,487,5,107,0,0,487,488,3,64,
    32,0,488,489,5,27,0,0,489,490,3,64,32,0,490,491,5,21,0,0,491,492,5,161,
    0,0,492,493,5,133,0,0,493,496,3,64,32,0,494,495,5,28,0,0,495,497,5,161,
    0,0,496,494,1,0,0,0,496,497,1,0,0,0,497,550,1,0,0,0,498,499,5,108,0,0,
    499,500,5,24,0,0,500,501,3,64,32,0,501,502,5,134,0,0,502,503,3,64,32,
    0,503,504,5,27,0,0,504,505,3,64,32,0,505,506,5,135,0,0,506,507,3,64,32,
    0,507,508,5,136,0,0,508,509,3,64,32,0,509,550,1,0,0,0,510,511,5,128,0,
    0,511,512,3,64,32,0,512,513,5,27,0,0,513,514,3,64,32,0,514,515,5,135,
    0,0,515,516,3,64,32,0,516,517,5,136,0,0,517,518,3,64,32,0,518,550,1,0,
    0,0,519,520,7,14,0,0,520,550,5,161,0,0,521,522,5,111,0,0,522,523,5,137,
    0,0,523,524,5,161,0,0,524,525,5,26,0,0,525,526,3,64,32,0,526,527,5,138,
    0,0,527,528,5,161,0,0,528,529,5,27,0,0,529,532,3,64,32,0,530,531,5,20,
    0,0,531,533,5,161,0,0,532,530,1,0,0,0,532,533,1,0,0,0,533,550,1,0,0,0,
    534,550,5,112,0,0,535,536,5,110,0,0,536,537,5,24,0,0,537,538,3,64,32,
    0,538,539,5,139,0,0,539,540,3,64,32,0,540,541,5,140,0,0,541,542,3,64,
    32,0,542,543,5,26,0,0,543,544,3,64,32,0,544,545,5,27,0,0,545,546,3,64,
    32,0,546,547,5,141,0,0,547,548,3,64,32,0,548,550,1,0,0,0,549,475,1,0,
    0,0,549,476,1,0,0,0,549,486,1,0,0,0,549,498,1,0,0,0,549,510,1,0,0,0,549,
    519,1,0,0,0,549,521,1,0,0,0,549,534,1,0,0,0,549,535,1,0,0,0,550,69,1,
    0,0,0,551,552,5,30,0,0,552,569,5,113,0,0,553,554,3,64,32,0,554,555,5,
    27,0,0,555,558,3,64,32,0,556,557,5,24,0,0,557,559,3,64,32,0,558,556,1,
    0,0,0,558,559,1,0,0,0,559,570,1,0,0,0,560,561,5,24,0,0,561,562,3,64,32,
    0,562,563,5,134,0,0,563,564,3,64,32,0,564,565,5,141,0,0,565,566,3,64,
    32,0,566,567,5,27,0,0,567,568,3,64,32,0,568,570,1,0,0,0,569,553,1,0,0,
    0,569,560,1,0,0,0,570,572,1,0,0,0,571,573,3,98,49,0,572,571,1,0,0,0,572,
    573,1,0,0,0,573,71,1,0,0,0,574,575,5,30,0,0,575,576,5,114,0,0,576,577,
    5,161,0,0,577,578,5,34,0,0,578,590,3,64,32,0,579,589,3,98,49,0,580,585,
    5,19,0,0,581,582,7,1,0,0,582,584,5,161,0,0,583,581,1,0,0,0,584,587,1,
    0,0,0,585,583,1,0,0,0,585,586,1,0,0,0,586,589,1,0,0,0,587,585,1,0,0,0,
    588,579,1,0,0,0,588,580,1,0,0,0,589,592,1,0,0,0,590,588,1,0,0,0,590,591,
    1,0,0,0,591,73,1,0,0,0,592,590,1,0,0,0,593,594,5,115,0,0,594,598,3,64,
    32,0,595,597,3,76,38,0,596,595,1,0,0,0,597,600,1,0,0,0,598,596,1,0,0,
    0,598,599,1,0,0,0,599,606,1,0,0,0,600,598,1,0,0,0,601,603,7,15,0,0,602,
    604,5,161,0,0,603,602,1,0,0,0,603,604,1,0,0,0,604,606,1,0,0,0,605,593,
    1,0,0,0,605,601,1,0,0,0,606,75,1,0,0,0,607,608,7,16,0,0,608,614,3,64,
    32,0,609,610,5,20,0,0,610,614,5,161,0,0,611,614,5,148,0,0,612,614,5,149,
    0,0,613,607,1,0,0,0,613,609,1,0,0,0,613,611,1,0,0,0,613,612,1,0,0,0,614,
    77,1,0,0,0,615,616,5,130,0,0,616,79,1,0,0,0,617,618,5,118,0,0,618,625,
    3,64,32,0,619,620,5,142,0,0,620,624,3,64,32,0,621,622,5,20,0,0,622,624,
    5,161,0,0,623,619,1,0,0,0,623,621,1,0,0,0,624,627,1,0,0,0,625,623,1,0,
    0,0,625,626,1,0,0,0,626,634,1,0,0,0,627,625,1,0,0,0,628,631,5,119,0,0,
    629,630,5,20,0,0,630,632,5,161,0,0,631,629,1,0,0,0,631,632,1,0,0,0,632,
    634,1,0,0,0,633,617,1,0,0,0,633,628,1,0,0,0,634,81,1,0,0,0,635,640,5,
    120,0,0,636,640,5,131,0,0,637,638,5,121,0,0,638,640,7,17,0,0,639,635,
    1,0,0,0,639,636,1,0,0,0,639,637,1,0,0,0,640,645,1,0,0,0,641,642,7,1,0,
    0,642,644,5,161,0,0,643,641,1,0,0,0,644,647,1,0,0,0,645,643,1,0,0,0,645,
    646,1,0,0,0,646,83,1,0,0,0,647,645,1,0,0,0,648,649,5,125,0,0,649,650,
    5,55,0,0,650,651,3,64,32,0,651,85,1,0,0,0,652,667,5,123,0,0,653,657,5,
    3,0,0,654,656,3,88,44,0,655,654,1,0,0,0,656,659,1,0,0,0,657,655,1,0,0,
    0,657,658,1,0,0,0,658,668,1,0,0,0,659,657,1,0,0,0,660,663,5,6,0,0,661,
    662,5,103,0,0,662,664,3,64,32,0,663,661,1,0,0,0,663,664,1,0,0,0,664,668,
    1,0,0,0,665,666,5,129,0,0,666,668,3,64,32,0,667,653,1,0,0,0,667,660,1,
    0,0,0,667,665,1,0,0,0,668,87,1,0,0,0,669,670,7,18,0,0,670,678,5,161,0,
    0,671,672,5,150,0,0,672,678,5,161,0,0,673,678,5,151,0,0,674,678,5,152,
    0,0,675,676,7,19,0,0,676,678,3,64,32,0,677,669,1,0,0,0,677,671,1,0,0,
    0,677,673,1,0,0,0,677,674,1,0,0,0,677,675,1,0,0,0,678,89,1,0,0,0,679,
    686,5,124,0,0,680,681,7,18,0,0,681,685,5,161,0,0,682,683,7,20,0,0,683,
    685,3,64,32,0,684,680,1,0,0,0,684,682,1,0,0,0,685,688,1,0,0,0,686,684,
    1,0,0,0,686,687,1,0,0,0,687,91,1,0,0,0,688,686,1,0,0,0,689,690,5,156,
    0,0,690,691,3,64,32,0,691,93,1,0,0,0,692,693,7,21,0,0,693,694,5,161,0,
    0,694,95,1,0,0,0,695,696,5,159,0,0,696,705,3,64,32,0,697,700,5,160,0,
    0,698,701,3,64,32,0,699,701,5,163,0,0,700,698,1,0,0,0,700,699,1,0,0,0,
    701,702,1,0,0,0,702,700,1,0,0,0,702,703,1,0,0,0,703,705,1,0,0,0,704,695,
    1,0,0,0,704,697,1,0,0,0,705,97,1,0,0,0,706,707,5,167,0,0,707,708,3,64,
    32,0,708,99,1,0,0,0,78,136,143,150,155,161,163,168,179,190,193,201,209,
    217,223,225,234,236,245,247,253,255,259,261,268,273,282,290,309,319,322,
    328,335,365,371,378,380,390,402,409,411,419,421,434,439,441,450,457,461,
    470,484,496,532,549,558,569,572,585,588,590,598,603,605,613,623,625,631,
    633,639,645,657,663,667,677,684,686,700,702,704
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
    setState(100);
    match(TracerCommandsParser::T__0);
    setState(101);
    command();
    setState(102);
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
    setState(136);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 0, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(104);
      server();
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(105);
      attach();
      break;
    }

    case 3: {
      enterOuterAlt(_localctx, 3);
      setState(106);
      processes();
      break;
    }

    case 4: {
      enterOuterAlt(_localctx, 4);
      setState(107);
      sessions();
      break;
    }

    case 5: {
      enterOuterAlt(_localctx, 5);
      setState(108);
      use();
      break;
    }

    case 6: {
      enterOuterAlt(_localctx, 6);
      setState(109);
      status();
      break;
    }

    case 7: {
      enterOuterAlt(_localctx, 7);
      setState(110);
      detach();
      break;
    }

    case 8: {
      enterOuterAlt(_localctx, 8);
      setState(111);
      modules();
      break;
    }

    case 9: {
      enterOuterAlt(_localctx, 9);
      setState(112);
      threads();
      break;
    }

    case 10: {
      enterOuterAlt(_localctx, 10);
      setState(113);
      symbols();
      break;
    }

    case 11: {
      enterOuterAlt(_localctx, 11);
      setState(114);
      probe();
      break;
    }

    case 12: {
      enterOuterAlt(_localctx, 12);
      setState(115);
      interceptor();
      break;
    }

    case 13: {
      enterOuterAlt(_localctx, 13);
      setState(116);
      events();
      break;
    }

    case 14: {
      enterOuterAlt(_localctx, 14);
      setState(117);
      collect();
      break;
    }

    case 15: {
      enterOuterAlt(_localctx, 15);
      setState(118);
      record();
      break;
    }

    case 16: {
      enterOuterAlt(_localctx, 16);
      setState(119);
      cpp();
      break;
    }

    case 17: {
      enterOuterAlt(_localctx, 17);
      setState(120);
      shell();
      break;
    }

    case 18: {
      enterOuterAlt(_localctx, 18);
      setState(121);
      complete();
      break;
    }

    case 19: {
      enterOuterAlt(_localctx, 19);
      setState(122);
      type();
      break;
    }

    case 20: {
      enterOuterAlt(_localctx, 20);
      setState(123);
      help();
      break;
    }

    case 21: {
      enterOuterAlt(_localctx, 21);
      setState(124);
      quit();
      break;
    }

    case 22: {
      enterOuterAlt(_localctx, 22);
      setState(125);
      bulkTrace();
      break;
    }

    case 23: {
      enterOuterAlt(_localctx, 23);
      setState(126);
      memory();
      break;
    }

    case 24: {
      enterOuterAlt(_localctx, 24);
      setState(127);
      probeEntry();
      break;
    }

    case 25: {
      enterOuterAlt(_localctx, 25);
      setState(128);
      probeReload();
      break;
    }

    case 26: {
      enterOuterAlt(_localctx, 26);
      setState(129);
      targetLaunch();
      break;
    }

    case 27: {
      enterOuterAlt(_localctx, 27);
      setState(130);
      targetCall();
      break;
    }

    case 28: {
      enterOuterAlt(_localctx, 28);
      setState(131);
      localPerf();
      break;
    }

    case 29: {
      enterOuterAlt(_localctx, 29);
      setState(132);
      instructions();
      break;
    }

    case 30: {
      enterOuterAlt(_localctx, 30);
      setState(133);
      watchpoint();
      break;
    }

    case 31: {
      enterOuterAlt(_localctx, 31);
      setState(134);
      targetLibrary();
      break;
    }

    case 32: {
      enterOuterAlt(_localctx, 32);
      setState(135);
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
    setState(138);
    match(TracerCommandsParser::T__1);
    setState(150);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__2: {
        setState(139);
        match(TracerCommandsParser::T__2);
        setState(143);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__6

        || _la == TracerCommandsParser::T__7 || _la == TracerCommandsParser::T__156

        || _la == TracerCommandsParser::T__157) {
          setState(140);
          serverOption();
          setState(145);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__3: {
        setState(146);
        match(TracerCommandsParser::T__3);
        setState(147);
        value();
        break;
      }

      case TracerCommandsParser::T__4: {
        setState(148);
        match(TracerCommandsParser::T__4);
        break;
      }

      case TracerCommandsParser::T__5: {
        setState(149);
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
    setState(155);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__6:
      case TracerCommandsParser::T__7: {
        enterOuterAlt(_localctx, 1);
        setState(152);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__6

        || _la == TracerCommandsParser::T__7)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(153);
        value();
        break;
      }

      case TracerCommandsParser::T__156:
      case TracerCommandsParser::T__157: {
        enterOuterAlt(_localctx, 2);
        setState(154);
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
    setState(157);
    match(TracerCommandsParser::T__8);
    setState(163);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::UINT) {
      setState(158);
      match(TracerCommandsParser::UINT);
      setState(161);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == TracerCommandsParser::T__9) {
        setState(159);
        match(TracerCommandsParser::T__9);
        setState(160);
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
    setState(165);
    match(TracerCommandsParser::T__10);
    setState(168);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::T__11) {
      setState(166);
      match(TracerCommandsParser::T__11);
      setState(167);
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
    setState(170);
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
    setState(172);
    match(TracerCommandsParser::T__13);
    setState(173);
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
    setState(175);
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
    setState(177);
    match(TracerCommandsParser::T__14);
    setState(179);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 8556839548640946812) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 64)) & -1030863687201) != 0) || ((((_la - 128) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 128)) & 1400159338527) != 0)) {
      setState(178);
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

TracerCommandsParser::ValueContext* TracerCommandsParser::ModulesContext::value() {
  return getRuleContext<TracerCommandsParser::ValueContext>(0);
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
    setState(181);
    match(TracerCommandsParser::T__15);
    setState(193);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__16: {
        setState(182);
        match(TracerCommandsParser::T__16);
        break;
      }

      case TracerCommandsParser::T__17: {
        setState(183);
        match(TracerCommandsParser::T__17);
        setState(184);
        value();
        break;
      }

      case TracerCommandsParser::T__18: {
        setState(185);
        match(TracerCommandsParser::T__18);
        setState(190);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__19

        || _la == TracerCommandsParser::T__20) {
          setState(186);
          _la = _input->LA(1);
          if (!(_la == TracerCommandsParser::T__19

          || _la == TracerCommandsParser::T__20)) {
          _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(187);
          match(TracerCommandsParser::UINT);
          setState(192);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::EOF: {
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
  enterRule(_localctx, 22, TracerCommandsParser::RuleThreads);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(195);
    match(TracerCommandsParser::T__21);

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
  enterRule(_localctx, 24, TracerCommandsParser::RuleSymbols);
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
    setState(197);
    match(TracerCommandsParser::T__22);
    setState(201);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 1056968704) != 0)) {
      setState(198);
      symbolOption();
      setState(203);
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
  enterRule(_localctx, 26, TracerCommandsParser::RuleSymbolOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(209);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__11:
      case TracerCommandsParser::T__23:
      case TracerCommandsParser::T__24:
      case TracerCommandsParser::T__25:
      case TracerCommandsParser::T__26: {
        enterOuterAlt(_localctx, 1);
        setState(204);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 251662336) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(205);
        value();
        break;
      }

      case TracerCommandsParser::T__27: {
        enterOuterAlt(_localctx, 2);
        setState(206);
        match(TracerCommandsParser::T__27);
        setState(207);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__28: {
        enterOuterAlt(_localctx, 3);
        setState(208);
        match(TracerCommandsParser::T__28);
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
  enterRule(_localctx, 28, TracerCommandsParser::RuleProbe);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(261);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__29: {
        enterOuterAlt(_localctx, 1);
        setState(211);
        match(TracerCommandsParser::T__29);
        setState(255);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__30: {
            setState(212);
            match(TracerCommandsParser::T__30);
            setState(213);
            value();
            setState(215);
            _errHandler->sync(this);
            _la = _input->LA(1);
            do {
              setState(214);
              prepareOption();
              setState(217);
              _errHandler->sync(this);
              _la = _input->LA(1);
            } while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 4123185381376) != 0) || ((((_la - 159) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 159)) & 291) != 0));
            break;
          }

          case TracerCommandsParser::T__31: {
            setState(219);
            match(TracerCommandsParser::T__31);
            setState(220);
            value();
            setState(225);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 32710487703552) != 0) || ((((_la - 159) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 159)) & 259) != 0)) {
              setState(223);
              _errHandler->sync(this);
              switch (_input->LA(1)) {
                case TracerCommandsParser::T__23:
                case TracerCommandsParser::T__37:
                case TracerCommandsParser::T__38:
                case TracerCommandsParser::T__39:
                case TracerCommandsParser::T__41:
                case TracerCommandsParser::T__42:
                case TracerCommandsParser::T__43:
                case TracerCommandsParser::T__158:
                case TracerCommandsParser::T__159: {
                  setState(221);
                  resolveOption();
                  break;
                }

                case TracerCommandsParser::CWD: {
                  setState(222);
                  cwdOption();
                  break;
                }

              default:
                throw NoViableAltException(this);
              }
              setState(227);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__32: {
            setState(228);
            match(TracerCommandsParser::T__32);
            setState(229);
            value();
            setState(230);
            match(TracerCommandsParser::T__33);
            setState(231);
            value();
            setState(236);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 18011924625096704) != 0) || ((((_la - 156) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 156)) & 2073) != 0)) {
              setState(234);
              _errHandler->sync(this);
              switch (_input->LA(1)) {
                case TracerCommandsParser::T__23:
                case TracerCommandsParser::T__37:
                case TracerCommandsParser::T__38:
                case TracerCommandsParser::T__39:
                case TracerCommandsParser::T__41:
                case TracerCommandsParser::T__42:
                case TracerCommandsParser::T__43:
                case TracerCommandsParser::T__44:
                case TracerCommandsParser::T__45:
                case TracerCommandsParser::T__46:
                case TracerCommandsParser::T__47:
                case TracerCommandsParser::T__48:
                case TracerCommandsParser::T__49:
                case TracerCommandsParser::T__50:
                case TracerCommandsParser::T__51:
                case TracerCommandsParser::T__52:
                case TracerCommandsParser::T__155:
                case TracerCommandsParser::T__158:
                case TracerCommandsParser::T__159: {
                  setState(232);
                  deployOption();
                  break;
                }

                case TracerCommandsParser::CWD: {
                  setState(233);
                  cwdOption();
                  break;
                }

              default:
                throw NoViableAltException(this);
              }
              setState(238);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__34: {
            setState(239);
            match(TracerCommandsParser::T__34);
            setState(240);
            value();
            setState(241);
            match(TracerCommandsParser::T__33);
            setState(242);
            value();
            setState(247);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 2249325929299968) != 0) || ((((_la - 159) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 159)) & 259) != 0)) {
              setState(245);
              _errHandler->sync(this);
              switch (_input->LA(1)) {
                case TracerCommandsParser::T__23:
                case TracerCommandsParser::T__37:
                case TracerCommandsParser::T__38:
                case TracerCommandsParser::T__39:
                case TracerCommandsParser::T__41:
                case TracerCommandsParser::T__42:
                case TracerCommandsParser::T__43:
                case TracerCommandsParser::T__44:
                case TracerCommandsParser::T__45:
                case TracerCommandsParser::T__46:
                case TracerCommandsParser::T__47:
                case TracerCommandsParser::T__48:
                case TracerCommandsParser::T__49:
                case TracerCommandsParser::T__158:
                case TracerCommandsParser::T__159: {
                  setState(243);
                  validateOption();
                  break;
                }

                case TracerCommandsParser::CWD: {
                  setState(244);
                  cwdOption();
                  break;
                }

              default:
                throw NoViableAltException(this);
              }
              setState(249);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__35: {
            setState(250);
            match(TracerCommandsParser::T__35);
            setState(251);
            match(TracerCommandsParser::UINT);
            setState(253);
            _errHandler->sync(this);

            _la = _input->LA(1);
            if (_la == TracerCommandsParser::CWD) {
              setState(252);
              cwdOption();
            }
            break;
          }

        default:
          throw NoViableAltException(this);
        }
        break;
      }

      case TracerCommandsParser::T__36: {
        enterOuterAlt(_localctx, 2);
        setState(257);
        match(TracerCommandsParser::T__36);
        setState(259);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::CWD) {
          setState(258);
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

tree::TerminalNode* TracerCommandsParser::PrepareOptionContext::METADATA_ONLY() {
  return getToken(TracerCommandsParser::METADATA_ONLY, 0);
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
  enterRule(_localctx, 30, TracerCommandsParser::RulePrepareOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(268);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__23:
      case TracerCommandsParser::T__37:
      case TracerCommandsParser::T__38:
      case TracerCommandsParser::T__39:
      case TracerCommandsParser::T__40: {
        enterOuterAlt(_localctx, 1);
        setState(263);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 4123185381376) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(264);
        value();
        break;
      }

      case TracerCommandsParser::METADATA_ONLY: {
        enterOuterAlt(_localctx, 2);
        setState(265);
        match(TracerCommandsParser::METADATA_ONLY);
        break;
      }

      case TracerCommandsParser::T__158:
      case TracerCommandsParser::T__159: {
        enterOuterAlt(_localctx, 3);
        setState(266);
        extraCompileOption();
        break;
      }

      case TracerCommandsParser::CWD: {
        enterOuterAlt(_localctx, 4);
        setState(267);
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
  enterRule(_localctx, 32, TracerCommandsParser::RuleResolveOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(273);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__23:
      case TracerCommandsParser::T__37:
      case TracerCommandsParser::T__38:
      case TracerCommandsParser::T__39:
      case TracerCommandsParser::T__41:
      case TracerCommandsParser::T__42:
      case TracerCommandsParser::T__43: {
        enterOuterAlt(_localctx, 1);
        setState(270);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 32710487703552) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(271);
        value();
        break;
      }

      case TracerCommandsParser::T__158:
      case TracerCommandsParser::T__159: {
        enterOuterAlt(_localctx, 2);
        setState(272);
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
  enterRule(_localctx, 34, TracerCommandsParser::RuleValidateOption);

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
      case TracerCommandsParser::T__23:
      case TracerCommandsParser::T__37:
      case TracerCommandsParser::T__38:
      case TracerCommandsParser::T__39:
      case TracerCommandsParser::T__41:
      case TracerCommandsParser::T__42:
      case TracerCommandsParser::T__43:
      case TracerCommandsParser::T__158:
      case TracerCommandsParser::T__159: {
        enterOuterAlt(_localctx, 1);
        setState(275);
        resolveOption();
        break;
      }

      case TracerCommandsParser::T__44: {
        enterOuterAlt(_localctx, 2);
        setState(276);
        match(TracerCommandsParser::T__44);
        break;
      }

      case TracerCommandsParser::T__45: {
        enterOuterAlt(_localctx, 3);
        setState(277);
        match(TracerCommandsParser::T__45);
        break;
      }

      case TracerCommandsParser::T__46: {
        enterOuterAlt(_localctx, 4);
        setState(278);
        match(TracerCommandsParser::T__46);
        break;
      }

      case TracerCommandsParser::T__47: {
        enterOuterAlt(_localctx, 5);
        setState(279);
        match(TracerCommandsParser::T__47);
        break;
      }

      case TracerCommandsParser::T__48: {
        enterOuterAlt(_localctx, 6);
        setState(280);
        match(TracerCommandsParser::T__48);
        break;
      }

      case TracerCommandsParser::T__49: {
        enterOuterAlt(_localctx, 7);
        setState(281);
        match(TracerCommandsParser::T__49);
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
  enterRule(_localctx, 36, TracerCommandsParser::RuleDeployOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(290);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__23:
      case TracerCommandsParser::T__37:
      case TracerCommandsParser::T__38:
      case TracerCommandsParser::T__39:
      case TracerCommandsParser::T__41:
      case TracerCommandsParser::T__42:
      case TracerCommandsParser::T__43:
      case TracerCommandsParser::T__44:
      case TracerCommandsParser::T__45:
      case TracerCommandsParser::T__46:
      case TracerCommandsParser::T__47:
      case TracerCommandsParser::T__48:
      case TracerCommandsParser::T__49:
      case TracerCommandsParser::T__158:
      case TracerCommandsParser::T__159: {
        enterOuterAlt(_localctx, 1);
        setState(284);
        validateOption();
        break;
      }

      case TracerCommandsParser::T__50: {
        enterOuterAlt(_localctx, 2);
        setState(285);
        match(TracerCommandsParser::T__50);
        setState(286);
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

      case TracerCommandsParser::T__51: {
        enterOuterAlt(_localctx, 3);
        setState(287);
        match(TracerCommandsParser::T__51);
        break;
      }

      case TracerCommandsParser::T__52: {
        enterOuterAlt(_localctx, 4);
        setState(288);
        match(TracerCommandsParser::T__52);
        break;
      }

      case TracerCommandsParser::T__155: {
        enterOuterAlt(_localctx, 5);
        setState(289);
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
  enterRule(_localctx, 38, TracerCommandsParser::RuleInterceptor);
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
    setState(292);
    match(TracerCommandsParser::T__53);
    setState(322);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__54: {
        setState(293);
        match(TracerCommandsParser::T__54);
        setState(294);
        value();
        setState(295);
        match(TracerCommandsParser::T__55);
        setState(296);
        value();
        break;
      }

      case TracerCommandsParser::T__56: {
        setState(298);
        match(TracerCommandsParser::T__56);
        setState(299);
        value();
        break;
      }

      case TracerCommandsParser::T__57: {
        setState(300);
        match(TracerCommandsParser::T__57);
        setState(301);
        value();
        break;
      }

      case TracerCommandsParser::T__34: {
        setState(302);
        match(TracerCommandsParser::T__34);
        setState(303);
        value();
        setState(304);
        match(TracerCommandsParser::T__58);
        setState(305);
        value();
        setState(309);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 2249325929299968) != 0) || _la == TracerCommandsParser::T__158

        || _la == TracerCommandsParser::T__159) {
          setState(306);
          validateOption();
          setState(311);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__59: {
        setState(312);
        match(TracerCommandsParser::T__59);
        setState(313);
        value();
        setState(314);
        match(TracerCommandsParser::T__58);
        setState(315);
        value();
        setState(319);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 18011924625096704) != 0) || ((((_la - 156) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 156)) & 25) != 0)) {
          setState(316);
          deployOption();
          setState(321);
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
  enterRule(_localctx, 40, TracerCommandsParser::RuleEvents);
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
    setState(324);
    match(TracerCommandsParser::T__60);
    setState(365);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::EOF:
      case TracerCommandsParser::T__19:
      case TracerCommandsParser::T__20:
      case TracerCommandsParser::T__55:
      case TracerCommandsParser::T__72:
      case TracerCommandsParser::T__73:
      case TracerCommandsParser::T__74:
      case TracerCommandsParser::T__78:
      case TracerCommandsParser::T__79:
      case TracerCommandsParser::T__80: {
        setState(328);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (((((_la - 20) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 20)) & 4098275729626628099) != 0)) {
          setState(325);
          streamOption();
          setState(330);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__61: {
        setState(331);
        match(TracerCommandsParser::T__61);
        setState(332);
        value();
        setState(335);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__27) {
          setState(333);
          match(TracerCommandsParser::T__27);
          setState(334);
          match(TracerCommandsParser::UINT);
        }
        break;
      }

      case TracerCommandsParser::T__57: {
        setState(337);
        match(TracerCommandsParser::T__57);
        setState(338);
        value();
        setState(339);
        match(TracerCommandsParser::T__62);
        setState(340);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::STACK: {
        setState(342);
        match(TracerCommandsParser::STACK);
        setState(343);
        value();
        setState(344);
        match(TracerCommandsParser::T__62);
        setState(345);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__63: {
        setState(347);
        match(TracerCommandsParser::T__63);
        setState(348);
        value();
        break;
      }

      case TracerCommandsParser::T__64: {
        setState(349);
        match(TracerCommandsParser::T__64);
        setState(350);
        value();
        break;
      }

      case TracerCommandsParser::T__65: {
        setState(351);
        match(TracerCommandsParser::T__65);
        setState(352);
        value();
        break;
      }

      case TracerCommandsParser::T__66: {
        setState(353);
        match(TracerCommandsParser::T__66);
        setState(354);
        value();
        setState(355);
        match(TracerCommandsParser::T__55);
        setState(356);
        value();
        break;
      }

      case TracerCommandsParser::T__67: {
        setState(358);
        match(TracerCommandsParser::T__67);
        setState(359);
        value();
        setState(360);
        match(TracerCommandsParser::T__68);
        setState(361);
        match(TracerCommandsParser::T__69);
        setState(362);
        match(TracerCommandsParser::T__40);
        setState(363);
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
  enterRule(_localctx, 42, TracerCommandsParser::RuleCollect);
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
    setState(367);
    match(TracerCommandsParser::T__70);
    setState(371);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (((((_la - 20) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 20)) & 4098275729626628099) != 0)) {
      setState(368);
      streamOption();
      setState(373);
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
  enterRule(_localctx, 44, TracerCommandsParser::RuleRecord);
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
    setState(374);
    match(TracerCommandsParser::T__71);
    setState(375);
    value();
    setState(380);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (((((_la - 20) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 20)) & 4098275729626628099) != 0) || _la == TracerCommandsParser::SQLITE) {
      setState(378);
      _errHandler->sync(this);
      switch (_input->LA(1)) {
        case TracerCommandsParser::T__19:
        case TracerCommandsParser::T__20:
        case TracerCommandsParser::T__55:
        case TracerCommandsParser::T__72:
        case TracerCommandsParser::T__73:
        case TracerCommandsParser::T__74:
        case TracerCommandsParser::T__78:
        case TracerCommandsParser::T__79:
        case TracerCommandsParser::T__80: {
          setState(376);
          streamOption();
          break;
        }

        case TracerCommandsParser::SQLITE: {
          setState(377);
          match(TracerCommandsParser::SQLITE);
          break;
        }

      default:
        throw NoViableAltException(this);
      }
      setState(382);
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
  enterRule(_localctx, 46, TracerCommandsParser::RuleStreamOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(390);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__19:
      case TracerCommandsParser::T__20:
      case TracerCommandsParser::T__72:
      case TracerCommandsParser::T__73: {
        enterOuterAlt(_localctx, 1);
        setState(383);
        _la = _input->LA(1);
        if (!(((((_la - 20) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 20)) & 27021597764222979) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(384);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__74: {
        enterOuterAlt(_localctx, 2);
        setState(385);
        match(TracerCommandsParser::T__74);
        setState(386);
        _la = _input->LA(1);
        if (!(((((_la - 76) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 76)) & 7) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        break;
      }

      case TracerCommandsParser::T__55:
      case TracerCommandsParser::T__78:
      case TracerCommandsParser::T__79: {
        enterOuterAlt(_localctx, 3);
        setState(387);
        _la = _input->LA(1);
        if (!(((((_la - 56) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 56)) & 25165825) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(388);
        value();
        break;
      }

      case TracerCommandsParser::T__80: {
        enterOuterAlt(_localctx, 4);
        setState(389);
        match(TracerCommandsParser::T__80);
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
  enterRule(_localctx, 48, TracerCommandsParser::RuleCpp);
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
    setState(392);
    match(TracerCommandsParser::T__81);
    setState(421);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__54: {
        setState(393);
        match(TracerCommandsParser::T__54);
        setState(394);
        value();
        break;
      }

      case TracerCommandsParser::T__82: {
        setState(395);
        match(TracerCommandsParser::T__82);
        break;
      }

      case TracerCommandsParser::T__83:
      case TracerCommandsParser::T__84: {
        setState(396);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__83

        || _la == TracerCommandsParser::T__84)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(397);
        value();
        setState(402);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__40

        || _la == TracerCommandsParser::T__85) {
          setState(398);
          _la = _input->LA(1);
          if (!(_la == TracerCommandsParser::T__40

          || _la == TracerCommandsParser::T__85)) {
          _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(399);
          value();
          setState(404);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__86: {
        setState(405);
        match(TracerCommandsParser::T__86);
        setState(419);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__87: {
            setState(406);
            match(TracerCommandsParser::T__87);
            setState(409);
            _errHandler->sync(this);
            _la = _input->LA(1);
            do {
              setState(409);
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
                case TracerCommandsParser::T__29:
                case TracerCommandsParser::T__30:
                case TracerCommandsParser::T__31:
                case TracerCommandsParser::T__32:
                case TracerCommandsParser::T__34:
                case TracerCommandsParser::T__35:
                case TracerCommandsParser::T__36:
                case TracerCommandsParser::T__53:
                case TracerCommandsParser::T__54:
                case TracerCommandsParser::T__56:
                case TracerCommandsParser::T__57:
                case TracerCommandsParser::T__59:
                case TracerCommandsParser::T__60:
                case TracerCommandsParser::T__61:
                case TracerCommandsParser::T__63:
                case TracerCommandsParser::T__64:
                case TracerCommandsParser::T__65:
                case TracerCommandsParser::T__66:
                case TracerCommandsParser::T__67:
                case TracerCommandsParser::T__69:
                case TracerCommandsParser::T__70:
                case TracerCommandsParser::T__71:
                case TracerCommandsParser::T__75:
                case TracerCommandsParser::T__76:
                case TracerCommandsParser::T__77:
                case TracerCommandsParser::T__81:
                case TracerCommandsParser::T__82:
                case TracerCommandsParser::T__83:
                case TracerCommandsParser::T__84:
                case TracerCommandsParser::T__86:
                case TracerCommandsParser::T__87:
                case TracerCommandsParser::T__88:
                case TracerCommandsParser::T__90:
                case TracerCommandsParser::T__91:
                case TracerCommandsParser::T__92:
                case TracerCommandsParser::T__93:
                case TracerCommandsParser::T__94:
                case TracerCommandsParser::T__95:
                case TracerCommandsParser::T__96:
                case TracerCommandsParser::T__97:
                case TracerCommandsParser::T__98:
                case TracerCommandsParser::T__103:
                case TracerCommandsParser::T__104:
                case TracerCommandsParser::T__105:
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
                case TracerCommandsParser::T__122:
                case TracerCommandsParser::T__123:
                case TracerCommandsParser::T__124:
                case TracerCommandsParser::T__125:
                case TracerCommandsParser::T__126:
                case TracerCommandsParser::T__127:
                case TracerCommandsParser::T__128:
                case TracerCommandsParser::T__129:
                case TracerCommandsParser::T__130:
                case TracerCommandsParser::T__131:
                case TracerCommandsParser::UINT:
                case TracerCommandsParser::STRING:
                case TracerCommandsParser::STACK:
                case TracerCommandsParser::WORD: {
                  setState(407);
                  value();
                  break;
                }

                case TracerCommandsParser::SHELL: {
                  setState(408);
                  match(TracerCommandsParser::SHELL);
                  break;
                }

              default:
                throw NoViableAltException(this);
              }
              setState(411);
              _errHandler->sync(this);
              _la = _input->LA(1);
            } while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 8556839548640946812) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 64)) & -1030863687201) != 0) || ((((_la - 128) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 128)) & 1434519076895) != 0));
            break;
          }

          case TracerCommandsParser::T__65: {
            setState(413);
            match(TracerCommandsParser::T__65);
            break;
          }

          case TracerCommandsParser::T__88: {
            setState(414);
            match(TracerCommandsParser::T__88);
            setState(415);
            value();
            setState(416);
            match(TracerCommandsParser::T__89);
            setState(417);
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
  enterRule(_localctx, 50, TracerCommandsParser::RuleShell);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(423);
    match(TracerCommandsParser::T__90);
    setState(424);
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
  enterRule(_localctx, 52, TracerCommandsParser::RuleComplete);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(426);
    match(TracerCommandsParser::T__91);
    setState(427);
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
  enterRule(_localctx, 54, TracerCommandsParser::RuleType);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(429);
    match(TracerCommandsParser::T__92);
    setState(430);
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
  enterRule(_localctx, 56, TracerCommandsParser::RuleHelp);
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
    setState(432);
    match(TracerCommandsParser::T__93);
    setState(441);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 8556839548640946814) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 64)) & -1030863687201) != 0) || ((((_la - 128) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 128)) & 1400159338527) != 0)) {
      setState(434);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == TracerCommandsParser::T__0) {
        setState(433);
        match(TracerCommandsParser::T__0);
      }
      setState(437);
      _errHandler->sync(this);
      _la = _input->LA(1);
      do {
        setState(436);
        value();
        setState(439);
        _errHandler->sync(this);
        _la = _input->LA(1);
      } while ((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & 8556839548640946812) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 64)) & -1030863687201) != 0) || ((((_la - 128) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 128)) & 1400159338527) != 0));
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
  enterRule(_localctx, 58, TracerCommandsParser::RuleQuit);
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
    setState(443);
    _la = _input->LA(1);
    if (!(_la == TracerCommandsParser::T__94

    || _la == TracerCommandsParser::T__95)) {
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
  enterRule(_localctx, 60, TracerCommandsParser::RuleBulkTrace);
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
    setState(445);
    match(TracerCommandsParser::T__96);
    setState(461);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__97: {
        setState(446);
        match(TracerCommandsParser::T__97);
        setState(448);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(447);
          bulkTraceOption();
          setState(450);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while (_la == TracerCommandsParser::T__23

        || _la == TracerCommandsParser::T__37 || ((((_la - 100) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 100)) & 15) != 0));
        break;
      }

      case TracerCommandsParser::T__98: {
        setState(452);
        match(TracerCommandsParser::T__98);
        setState(453);
        match(TracerCommandsParser::T__99);
        setState(454);
        value();
        setState(457);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__33) {
          setState(455);
          match(TracerCommandsParser::T__33);
          setState(456);
          value();
        }
        break;
      }

      case TracerCommandsParser::T__35: {
        setState(459);
        match(TracerCommandsParser::T__35);
        setState(460);
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
  enterRule(_localctx, 62, TracerCommandsParser::RuleBulkTraceOption);
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
    setState(463);
    _la = _input->LA(1);
    if (!(_la == TracerCommandsParser::T__23

    || _la == TracerCommandsParser::T__37 || ((((_la - 100) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 100)) & 15) != 0))) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(464);
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
  enterRule(_localctx, 64, TracerCommandsParser::RuleValue);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(470);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::STRING: {
        enterOuterAlt(_localctx, 1);
        setState(466);
        match(TracerCommandsParser::STRING);
        break;
      }

      case TracerCommandsParser::WORD: {
        enterOuterAlt(_localctx, 2);
        setState(467);
        match(TracerCommandsParser::WORD);
        break;
      }

      case TracerCommandsParser::UINT: {
        enterOuterAlt(_localctx, 3);
        setState(468);
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
      case TracerCommandsParser::T__29:
      case TracerCommandsParser::T__30:
      case TracerCommandsParser::T__31:
      case TracerCommandsParser::T__32:
      case TracerCommandsParser::T__34:
      case TracerCommandsParser::T__35:
      case TracerCommandsParser::T__36:
      case TracerCommandsParser::T__53:
      case TracerCommandsParser::T__54:
      case TracerCommandsParser::T__56:
      case TracerCommandsParser::T__57:
      case TracerCommandsParser::T__59:
      case TracerCommandsParser::T__60:
      case TracerCommandsParser::T__61:
      case TracerCommandsParser::T__63:
      case TracerCommandsParser::T__64:
      case TracerCommandsParser::T__65:
      case TracerCommandsParser::T__66:
      case TracerCommandsParser::T__67:
      case TracerCommandsParser::T__69:
      case TracerCommandsParser::T__70:
      case TracerCommandsParser::T__71:
      case TracerCommandsParser::T__75:
      case TracerCommandsParser::T__76:
      case TracerCommandsParser::T__77:
      case TracerCommandsParser::T__81:
      case TracerCommandsParser::T__82:
      case TracerCommandsParser::T__83:
      case TracerCommandsParser::T__84:
      case TracerCommandsParser::T__86:
      case TracerCommandsParser::T__87:
      case TracerCommandsParser::T__88:
      case TracerCommandsParser::T__90:
      case TracerCommandsParser::T__91:
      case TracerCommandsParser::T__92:
      case TracerCommandsParser::T__93:
      case TracerCommandsParser::T__94:
      case TracerCommandsParser::T__95:
      case TracerCommandsParser::T__96:
      case TracerCommandsParser::T__97:
      case TracerCommandsParser::T__98:
      case TracerCommandsParser::T__103:
      case TracerCommandsParser::T__104:
      case TracerCommandsParser::T__105:
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
      case TracerCommandsParser::T__122:
      case TracerCommandsParser::T__123:
      case TracerCommandsParser::T__124:
      case TracerCommandsParser::T__125:
      case TracerCommandsParser::T__126:
      case TracerCommandsParser::T__127:
      case TracerCommandsParser::T__128:
      case TracerCommandsParser::T__129:
      case TracerCommandsParser::T__130:
      case TracerCommandsParser::T__131:
      case TracerCommandsParser::STACK: {
        enterOuterAlt(_localctx, 4);
        setState(469);
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
  enterRule(_localctx, 66, TracerCommandsParser::RuleKeyword);
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
    setState(472);
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 8556839548640946812) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 64)) & -1030863687201) != 0) || ((((_la - 128) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 128)) & 274877906975) != 0))) {
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
  enterRule(_localctx, 68, TracerCommandsParser::RuleMemory);
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
    setState(474);
    match(TracerCommandsParser::T__103);
    setState(549);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__104: {
        setState(475);
        match(TracerCommandsParser::T__104);
        break;
      }

      case TracerCommandsParser::T__105: {
        setState(476);
        match(TracerCommandsParser::T__105);
        setState(477);
        value();
        setState(478);
        match(TracerCommandsParser::T__26);
        setState(479);
        value();
        setState(484);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__20: {
            setState(480);
            match(TracerCommandsParser::T__20);
            setState(481);
            match(TracerCommandsParser::UINT);
            break;
          }

          case TracerCommandsParser::T__85: {
            setState(482);
            match(TracerCommandsParser::T__85);
            setState(483);
            value();
            break;
          }

        default:
          throw NoViableAltException(this);
        }
        break;
      }

      case TracerCommandsParser::T__106: {
        setState(486);
        match(TracerCommandsParser::T__106);
        setState(487);
        value();
        setState(488);
        match(TracerCommandsParser::T__26);
        setState(489);
        value();
        setState(490);
        match(TracerCommandsParser::T__20);
        setState(491);
        match(TracerCommandsParser::UINT);
        setState(492);
        match(TracerCommandsParser::T__132);
        setState(493);
        value();
        setState(496);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__27) {
          setState(494);
          match(TracerCommandsParser::T__27);
          setState(495);
          match(TracerCommandsParser::UINT);
        }
        break;
      }

      case TracerCommandsParser::T__107: {
        setState(498);
        match(TracerCommandsParser::T__107);
        setState(499);
        match(TracerCommandsParser::T__23);
        setState(500);
        value();
        setState(501);
        match(TracerCommandsParser::T__133);
        setState(502);
        value();
        setState(503);
        match(TracerCommandsParser::T__26);
        setState(504);
        value();
        setState(505);
        match(TracerCommandsParser::T__134);
        setState(506);
        value();
        setState(507);
        match(TracerCommandsParser::T__135);
        setState(508);
        value();
        break;
      }

      case TracerCommandsParser::T__127: {
        setState(510);
        match(TracerCommandsParser::T__127);
        setState(511);
        value();
        setState(512);
        match(TracerCommandsParser::T__26);
        setState(513);
        value();
        setState(514);
        match(TracerCommandsParser::T__134);
        setState(515);
        value();
        setState(516);
        match(TracerCommandsParser::T__135);
        setState(517);
        value();
        break;
      }

      case TracerCommandsParser::T__108:
      case TracerCommandsParser::T__125:
      case TracerCommandsParser::T__126: {
        setState(519);
        _la = _input->LA(1);
        if (!(((((_la - 109) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 109)) & 393217) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(520);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__110: {
        setState(521);
        match(TracerCommandsParser::T__110);
        setState(522);
        match(TracerCommandsParser::T__136);
        setState(523);
        match(TracerCommandsParser::UINT);
        setState(524);
        match(TracerCommandsParser::T__25);
        setState(525);
        value();
        setState(526);
        match(TracerCommandsParser::T__137);
        setState(527);
        match(TracerCommandsParser::UINT);
        setState(528);
        match(TracerCommandsParser::T__26);
        setState(529);
        value();
        setState(532);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__19) {
          setState(530);
          match(TracerCommandsParser::T__19);
          setState(531);
          match(TracerCommandsParser::UINT);
        }
        break;
      }

      case TracerCommandsParser::T__111: {
        setState(534);
        match(TracerCommandsParser::T__111);
        break;
      }

      case TracerCommandsParser::T__109: {
        setState(535);
        match(TracerCommandsParser::T__109);
        setState(536);
        match(TracerCommandsParser::T__23);
        setState(537);
        value();
        setState(538);
        match(TracerCommandsParser::T__138);
        setState(539);
        value();
        setState(540);
        match(TracerCommandsParser::T__139);
        setState(541);
        value();
        setState(542);
        match(TracerCommandsParser::T__25);
        setState(543);
        value();
        setState(544);
        match(TracerCommandsParser::T__26);
        setState(545);
        value();
        setState(546);
        match(TracerCommandsParser::T__140);
        setState(547);
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
  enterRule(_localctx, 70, TracerCommandsParser::RuleProbeEntry);
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
    setState(551);
    match(TracerCommandsParser::T__29);
    setState(552);
    match(TracerCommandsParser::T__112);
    setState(569);
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
      case TracerCommandsParser::T__29:
      case TracerCommandsParser::T__30:
      case TracerCommandsParser::T__31:
      case TracerCommandsParser::T__32:
      case TracerCommandsParser::T__34:
      case TracerCommandsParser::T__35:
      case TracerCommandsParser::T__36:
      case TracerCommandsParser::T__53:
      case TracerCommandsParser::T__54:
      case TracerCommandsParser::T__56:
      case TracerCommandsParser::T__57:
      case TracerCommandsParser::T__59:
      case TracerCommandsParser::T__60:
      case TracerCommandsParser::T__61:
      case TracerCommandsParser::T__63:
      case TracerCommandsParser::T__64:
      case TracerCommandsParser::T__65:
      case TracerCommandsParser::T__66:
      case TracerCommandsParser::T__67:
      case TracerCommandsParser::T__69:
      case TracerCommandsParser::T__70:
      case TracerCommandsParser::T__71:
      case TracerCommandsParser::T__75:
      case TracerCommandsParser::T__76:
      case TracerCommandsParser::T__77:
      case TracerCommandsParser::T__81:
      case TracerCommandsParser::T__82:
      case TracerCommandsParser::T__83:
      case TracerCommandsParser::T__84:
      case TracerCommandsParser::T__86:
      case TracerCommandsParser::T__87:
      case TracerCommandsParser::T__88:
      case TracerCommandsParser::T__90:
      case TracerCommandsParser::T__91:
      case TracerCommandsParser::T__92:
      case TracerCommandsParser::T__93:
      case TracerCommandsParser::T__94:
      case TracerCommandsParser::T__95:
      case TracerCommandsParser::T__96:
      case TracerCommandsParser::T__97:
      case TracerCommandsParser::T__98:
      case TracerCommandsParser::T__103:
      case TracerCommandsParser::T__104:
      case TracerCommandsParser::T__105:
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
      case TracerCommandsParser::T__122:
      case TracerCommandsParser::T__123:
      case TracerCommandsParser::T__124:
      case TracerCommandsParser::T__125:
      case TracerCommandsParser::T__126:
      case TracerCommandsParser::T__127:
      case TracerCommandsParser::T__128:
      case TracerCommandsParser::T__129:
      case TracerCommandsParser::T__130:
      case TracerCommandsParser::T__131:
      case TracerCommandsParser::UINT:
      case TracerCommandsParser::STRING:
      case TracerCommandsParser::STACK:
      case TracerCommandsParser::WORD: {
        setState(553);
        value();
        setState(554);
        match(TracerCommandsParser::T__26);
        setState(555);
        value();
        setState(558);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__23) {
          setState(556);
          match(TracerCommandsParser::T__23);
          setState(557);
          value();
        }
        break;
      }

      case TracerCommandsParser::T__23: {
        setState(560);
        match(TracerCommandsParser::T__23);
        setState(561);
        value();
        setState(562);
        match(TracerCommandsParser::T__133);
        setState(563);
        value();
        setState(564);
        match(TracerCommandsParser::T__140);
        setState(565);
        value();
        setState(566);
        match(TracerCommandsParser::T__26);
        setState(567);
        value();
        break;
      }

    default:
      throw NoViableAltException(this);
    }
    setState(572);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::CWD) {
      setState(571);
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
  enterRule(_localctx, 72, TracerCommandsParser::RuleProbeReload);
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
    setState(574);
    match(TracerCommandsParser::T__29);
    setState(575);
    match(TracerCommandsParser::T__113);
    setState(576);
    match(TracerCommandsParser::UINT);
    setState(577);
    match(TracerCommandsParser::T__33);
    setState(578);
    value();
    setState(590);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == TracerCommandsParser::T__18 || _la == TracerCommandsParser::CWD) {
      setState(588);
      _errHandler->sync(this);
      switch (_input->LA(1)) {
        case TracerCommandsParser::CWD: {
          setState(579);
          cwdOption();
          break;
        }

        case TracerCommandsParser::T__18: {
          setState(580);
          match(TracerCommandsParser::T__18);
          setState(585);
          _errHandler->sync(this);
          _la = _input->LA(1);
          while (_la == TracerCommandsParser::T__19

          || _la == TracerCommandsParser::T__20) {
            setState(581);
            _la = _input->LA(1);
            if (!(_la == TracerCommandsParser::T__19

            || _la == TracerCommandsParser::T__20)) {
            _errHandler->recoverInline(this);
            }
            else {
              _errHandler->reportMatch(this);
              consume();
            }
            setState(582);
            match(TracerCommandsParser::UINT);
            setState(587);
            _errHandler->sync(this);
            _la = _input->LA(1);
          }
          break;
        }

      default:
        throw NoViableAltException(this);
      }
      setState(592);
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
  enterRule(_localctx, 74, TracerCommandsParser::RuleTargetLaunch);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(605);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__114: {
        enterOuterAlt(_localctx, 1);
        setState(593);
        match(TracerCommandsParser::T__114);
        setState(594);
        value();
        setState(598);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__19 || ((((_la - 142) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 142)) & 33554687) != 0)) {
          setState(595);
          launchOption();
          setState(600);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__115:
      case TracerCommandsParser::T__116: {
        enterOuterAlt(_localctx, 2);
        setState(601);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__115

        || _la == TracerCommandsParser::T__116)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(603);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::UINT) {
          setState(602);
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
  enterRule(_localctx, 76, TracerCommandsParser::RuleLaunchOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(613);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__141:
      case TracerCommandsParser::T__142:
      case TracerCommandsParser::T__143:
      case TracerCommandsParser::T__144:
      case TracerCommandsParser::T__145:
      case TracerCommandsParser::T__146:
      case TracerCommandsParser::CWD: {
        enterOuterAlt(_localctx, 1);
        setState(607);
        _la = _input->LA(1);
        if (!(((((_la - 142) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 142)) & 33554495) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(608);
        value();
        break;
      }

      case TracerCommandsParser::T__19: {
        enterOuterAlt(_localctx, 2);
        setState(609);
        match(TracerCommandsParser::T__19);
        setState(610);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__147: {
        enterOuterAlt(_localctx, 3);
        setState(611);
        match(TracerCommandsParser::T__147);
        break;
      }

      case TracerCommandsParser::T__148: {
        enterOuterAlt(_localctx, 4);
        setState(612);
        match(TracerCommandsParser::T__148);
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
  enterRule(_localctx, 78, TracerCommandsParser::RuleChildList);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(615);
    match(TracerCommandsParser::T__129);

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
  enterRule(_localctx, 80, TracerCommandsParser::RuleTargetCall);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(633);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__117: {
        enterOuterAlt(_localctx, 1);
        setState(617);
        match(TracerCommandsParser::T__117);
        setState(618);
        value();
        setState(625);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__19 || _la == TracerCommandsParser::T__141) {
          setState(623);
          _errHandler->sync(this);
          switch (_input->LA(1)) {
            case TracerCommandsParser::T__141: {
              setState(619);
              match(TracerCommandsParser::T__141);
              setState(620);
              value();
              break;
            }

            case TracerCommandsParser::T__19: {
              setState(621);
              match(TracerCommandsParser::T__19);
              setState(622);
              match(TracerCommandsParser::UINT);
              break;
            }

          default:
            throw NoViableAltException(this);
          }
          setState(627);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__118: {
        enterOuterAlt(_localctx, 2);
        setState(628);
        match(TracerCommandsParser::T__118);
        setState(631);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__19) {
          setState(629);
          match(TracerCommandsParser::T__19);
          setState(630);
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
  enterRule(_localctx, 82, TracerCommandsParser::RuleLocalPerf);
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
    setState(639);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__119: {
        setState(635);
        match(TracerCommandsParser::T__119);
        break;
      }

      case TracerCommandsParser::T__130: {
        setState(636);
        match(TracerCommandsParser::T__130);
        break;
      }

      case TracerCommandsParser::T__120: {
        setState(637);
        match(TracerCommandsParser::T__120);
        setState(638);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__121

        || _la == TracerCommandsParser::T__131)) {
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
    setState(645);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == TracerCommandsParser::T__19

    || _la == TracerCommandsParser::T__20) {
      setState(641);
      _la = _input->LA(1);
      if (!(_la == TracerCommandsParser::T__19

      || _la == TracerCommandsParser::T__20)) {
      _errHandler->recoverInline(this);
      }
      else {
        _errHandler->reportMatch(this);
        consume();
      }
      setState(642);
      match(TracerCommandsParser::UINT);
      setState(647);
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
  enterRule(_localctx, 84, TracerCommandsParser::RuleTargetLibrary);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(648);
    match(TracerCommandsParser::T__124);
    setState(649);
    match(TracerCommandsParser::T__54);
    setState(650);
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
  enterRule(_localctx, 86, TracerCommandsParser::RuleInstructions);
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
    setState(652);
    match(TracerCommandsParser::T__122);
    setState(667);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__2: {
        setState(653);
        match(TracerCommandsParser::T__2);
        setState(657);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 204472320) != 0) || ((((_la - 137) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 137)) & 253955) != 0)) {
          setState(654);
          instructionOption();
          setState(659);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__5: {
        setState(660);
        match(TracerCommandsParser::T__5);
        setState(663);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__102) {
          setState(661);
          match(TracerCommandsParser::T__102);
          setState(662);
          value();
        }
        break;
      }

      case TracerCommandsParser::T__128: {
        setState(665);
        match(TracerCommandsParser::T__128);
        setState(666);
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
  enterRule(_localctx, 88, TracerCommandsParser::RuleInstructionOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(677);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__19:
      case TracerCommandsParser::T__20:
      case TracerCommandsParser::T__136:
      case TracerCommandsParser::T__137: {
        enterOuterAlt(_localctx, 1);
        setState(669);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__19

        || _la == TracerCommandsParser::T__20 || _la == TracerCommandsParser::T__136

        || _la == TracerCommandsParser::T__137)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(670);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__149: {
        enterOuterAlt(_localctx, 2);
        setState(671);
        match(TracerCommandsParser::T__149);
        setState(672);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__150: {
        enterOuterAlt(_localctx, 3);
        setState(673);
        match(TracerCommandsParser::T__150);
        break;
      }

      case TracerCommandsParser::T__151: {
        enterOuterAlt(_localctx, 4);
        setState(674);
        match(TracerCommandsParser::T__151);
        break;
      }

      case TracerCommandsParser::T__25:
      case TracerCommandsParser::T__26:
      case TracerCommandsParser::T__152:
      case TracerCommandsParser::T__153: {
        enterOuterAlt(_localctx, 5);
        setState(675);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__25

        || _la == TracerCommandsParser::T__26 || _la == TracerCommandsParser::T__152

        || _la == TracerCommandsParser::T__153)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(676);
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
  enterRule(_localctx, 90, TracerCommandsParser::RuleWatchpoint);
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
    setState(679);
    match(TracerCommandsParser::T__123);
    setState(686);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 204472320) != 0) || ((((_la - 137) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 137)) & 262147) != 0)) {
      setState(684);
      _errHandler->sync(this);
      switch (_input->LA(1)) {
        case TracerCommandsParser::T__19:
        case TracerCommandsParser::T__20:
        case TracerCommandsParser::T__136:
        case TracerCommandsParser::T__137: {
          setState(680);
          _la = _input->LA(1);
          if (!(_la == TracerCommandsParser::T__19

          || _la == TracerCommandsParser::T__20 || _la == TracerCommandsParser::T__136

          || _la == TracerCommandsParser::T__137)) {
          _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(681);
          match(TracerCommandsParser::UINT);
          break;
        }

        case TracerCommandsParser::T__25:
        case TracerCommandsParser::T__26:
        case TracerCommandsParser::T__154: {
          setState(682);
          _la = _input->LA(1);
          if (!(_la == TracerCommandsParser::T__25

          || _la == TracerCommandsParser::T__26 || _la == TracerCommandsParser::T__154)) {
          _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(683);
          value();
          break;
        }

      default:
        throw NoViableAltException(this);
      }
      setState(688);
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
  enterRule(_localctx, 92, TracerCommandsParser::RuleStackFilter);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(689);
    match(TracerCommandsParser::T__155);
    setState(690);
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
  enterRule(_localctx, 94, TracerCommandsParser::RuleContextEventBytes);
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
    setState(692);
    _la = _input->LA(1);
    if (!(_la == TracerCommandsParser::T__156

    || _la == TracerCommandsParser::T__157)) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(693);
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
  enterRule(_localctx, 96, TracerCommandsParser::RuleExtraCompileOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(704);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__158: {
        enterOuterAlt(_localctx, 1);
        setState(695);
        match(TracerCommandsParser::T__158);
        setState(696);
        value();
        break;
      }

      case TracerCommandsParser::T__159: {
        enterOuterAlt(_localctx, 2);
        setState(697);
        match(TracerCommandsParser::T__159);
        setState(700);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(700);
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
            case TracerCommandsParser::T__29:
            case TracerCommandsParser::T__30:
            case TracerCommandsParser::T__31:
            case TracerCommandsParser::T__32:
            case TracerCommandsParser::T__34:
            case TracerCommandsParser::T__35:
            case TracerCommandsParser::T__36:
            case TracerCommandsParser::T__53:
            case TracerCommandsParser::T__54:
            case TracerCommandsParser::T__56:
            case TracerCommandsParser::T__57:
            case TracerCommandsParser::T__59:
            case TracerCommandsParser::T__60:
            case TracerCommandsParser::T__61:
            case TracerCommandsParser::T__63:
            case TracerCommandsParser::T__64:
            case TracerCommandsParser::T__65:
            case TracerCommandsParser::T__66:
            case TracerCommandsParser::T__67:
            case TracerCommandsParser::T__69:
            case TracerCommandsParser::T__70:
            case TracerCommandsParser::T__71:
            case TracerCommandsParser::T__75:
            case TracerCommandsParser::T__76:
            case TracerCommandsParser::T__77:
            case TracerCommandsParser::T__81:
            case TracerCommandsParser::T__82:
            case TracerCommandsParser::T__83:
            case TracerCommandsParser::T__84:
            case TracerCommandsParser::T__86:
            case TracerCommandsParser::T__87:
            case TracerCommandsParser::T__88:
            case TracerCommandsParser::T__90:
            case TracerCommandsParser::T__91:
            case TracerCommandsParser::T__92:
            case TracerCommandsParser::T__93:
            case TracerCommandsParser::T__94:
            case TracerCommandsParser::T__95:
            case TracerCommandsParser::T__96:
            case TracerCommandsParser::T__97:
            case TracerCommandsParser::T__98:
            case TracerCommandsParser::T__103:
            case TracerCommandsParser::T__104:
            case TracerCommandsParser::T__105:
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
            case TracerCommandsParser::T__122:
            case TracerCommandsParser::T__123:
            case TracerCommandsParser::T__124:
            case TracerCommandsParser::T__125:
            case TracerCommandsParser::T__126:
            case TracerCommandsParser::T__127:
            case TracerCommandsParser::T__128:
            case TracerCommandsParser::T__129:
            case TracerCommandsParser::T__130:
            case TracerCommandsParser::T__131:
            case TracerCommandsParser::UINT:
            case TracerCommandsParser::STRING:
            case TracerCommandsParser::STACK:
            case TracerCommandsParser::WORD: {
              setState(698);
              value();
              break;
            }

            case TracerCommandsParser::SHELL: {
              setState(699);
              match(TracerCommandsParser::SHELL);
              break;
            }

          default:
            throw NoViableAltException(this);
          }
          setState(702);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 8556839548640946812) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 64)) & -1030863687201) != 0) || ((((_la - 128) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 128)) & 1434519076895) != 0));
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
  enterRule(_localctx, 98, TracerCommandsParser::RuleCwdOption);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(706);
    match(TracerCommandsParser::CWD);
    setState(707);
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
