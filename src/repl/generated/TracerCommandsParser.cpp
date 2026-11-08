
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
      "contextEventBytes", "extraCompileOption"
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
      "'--original'", "'--abi'", "'--arg'", "'--cwd'", "'--env'", "'--stdin'",
      "'--stdout'", "'--stderr'", "'--preload'", "'--supervise'", "'--gate-children'",
      "'--memory-size'", "'--registers'", "'--discover'", "'--callback-library'",
      "'--memory-address'", "'--access'", "'--stack-application-only'",
      "'--context-event-bytes'", "'--context-transport-memory-bytes'", "'--extra-arg'",
      "'--extra-args'", "", "", "", "'--metadata-only'", "'--sqlite'", "'stack'"
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
      "", "", "", "", "", "", "", "", "", "UINT", "STRING", "SHELL", "METADATA_ONLY",
      "SQLITE", "STACK", "WORD", "WS"
    }
  );
  static const int32_t serializedATNSegment[] = {
    4,1,169,688,2,0,7,0,2,1,7,1,2,2,7,2,2,3,7,3,2,4,7,4,2,5,7,5,2,6,7,6,2,
    7,7,7,2,8,7,8,2,9,7,9,2,10,7,10,2,11,7,11,2,12,7,12,2,13,7,13,2,14,7,
    14,2,15,7,15,2,16,7,16,2,17,7,17,2,18,7,18,2,19,7,19,2,20,7,20,2,21,7,
    21,2,22,7,22,2,23,7,23,2,24,7,24,2,25,7,25,2,26,7,26,2,27,7,27,2,28,7,
    28,2,29,7,29,2,30,7,30,2,31,7,31,2,32,7,32,2,33,7,33,2,34,7,34,2,35,7,
    35,2,36,7,36,2,37,7,37,2,38,7,38,2,39,7,39,2,40,7,40,2,41,7,41,2,42,7,
    42,2,43,7,43,2,44,7,44,2,45,7,45,2,46,7,46,2,47,7,47,2,48,7,48,1,0,1,
    0,1,0,1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,3,
    1,135,8,1,1,2,1,2,1,2,5,2,140,8,2,10,2,12,2,143,9,2,1,2,1,2,1,2,1,2,3,
    2,149,8,2,1,3,1,3,1,3,3,3,154,8,3,1,4,1,4,1,4,1,4,3,4,160,8,4,3,4,162,
    8,4,1,5,1,5,1,5,3,5,167,8,5,1,6,1,6,1,7,1,7,1,7,1,8,1,8,1,9,1,9,3,9,178,
    8,9,1,10,1,10,1,10,1,10,1,10,1,10,1,10,5,10,187,8,10,10,10,12,10,190,
    9,10,3,10,192,8,10,1,11,1,11,1,12,1,12,5,12,198,8,12,10,12,12,12,201,
    9,12,1,13,1,13,1,13,1,13,1,13,3,13,208,8,13,1,14,1,14,1,14,1,14,4,14,
    214,8,14,11,14,12,14,215,1,14,1,14,1,14,5,14,221,8,14,10,14,12,14,224,
    9,14,1,14,1,14,1,14,1,14,1,14,5,14,231,8,14,10,14,12,14,234,9,14,1,14,
    1,14,1,14,1,14,1,14,5,14,241,8,14,10,14,12,14,244,9,14,1,14,1,14,3,14,
    248,8,14,1,14,3,14,251,8,14,1,15,1,15,1,15,1,15,3,15,257,8,15,1,16,1,
    16,1,16,3,16,262,8,16,1,17,1,17,1,17,1,17,1,17,1,17,1,17,3,17,271,8,17,
    1,18,1,18,1,18,1,18,1,18,1,18,3,18,279,8,18,1,19,1,19,1,19,1,19,1,19,
    1,19,1,19,1,19,1,19,1,19,1,19,1,19,1,19,1,19,1,19,5,19,296,8,19,10,19,
    12,19,299,9,19,1,19,1,19,1,19,1,19,1,19,5,19,306,8,19,10,19,12,19,309,
    9,19,3,19,311,8,19,1,20,1,20,5,20,315,8,20,10,20,12,20,318,9,20,1,20,
    1,20,1,20,1,20,3,20,324,8,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,
    1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,
    1,20,1,20,1,20,1,20,1,20,1,20,3,20,354,8,20,1,21,1,21,5,21,358,8,21,10,
    21,12,21,361,9,21,1,22,1,22,1,22,1,22,5,22,367,8,22,10,22,12,22,370,9,
    22,1,23,1,23,1,23,1,23,1,23,1,23,1,23,3,23,379,8,23,1,24,1,24,1,24,1,
    24,1,24,1,24,1,24,1,24,5,24,389,8,24,10,24,12,24,392,9,24,1,24,1,24,1,
    24,1,24,4,24,398,8,24,11,24,12,24,399,1,24,1,24,1,24,1,24,1,24,1,24,3,
    24,408,8,24,3,24,410,8,24,1,25,1,25,1,25,1,26,1,26,1,26,1,27,1,27,1,27,
    1,28,1,28,3,28,423,8,28,1,28,4,28,426,8,28,11,28,12,28,427,3,28,430,8,
    28,1,29,1,29,1,30,1,30,1,30,4,30,437,8,30,11,30,12,30,438,1,30,1,30,1,
    30,1,30,1,30,3,30,446,8,30,1,30,1,30,3,30,450,8,30,1,31,1,31,1,31,1,32,
    1,32,1,32,1,32,3,32,459,8,32,1,33,1,33,1,34,1,34,1,34,1,34,1,34,1,34,
    1,34,1,34,1,34,1,34,3,34,473,8,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,
    1,34,1,34,1,34,3,34,485,8,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,
    1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,
    1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,3,34,521,
    8,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,
    1,34,1,34,3,34,538,8,34,1,35,1,35,1,35,1,35,1,35,1,35,1,35,3,35,547,8,
    35,1,35,1,35,1,35,1,35,1,35,1,35,1,35,1,35,1,35,3,35,558,8,35,1,36,1,
    36,1,36,1,36,1,36,1,36,1,36,1,36,5,36,568,8,36,10,36,12,36,571,9,36,3,
    36,573,8,36,1,37,1,37,1,37,5,37,578,8,37,10,37,12,37,581,9,37,1,37,1,
    37,3,37,585,8,37,3,37,587,8,37,1,38,1,38,1,38,1,38,1,38,1,38,3,38,595,
    8,38,1,39,1,39,1,40,1,40,1,40,1,40,1,40,1,40,5,40,605,8,40,10,40,12,40,
    608,9,40,1,40,1,40,1,40,3,40,613,8,40,3,40,615,8,40,1,41,1,41,1,41,1,
    41,3,41,621,8,41,1,41,1,41,5,41,625,8,41,10,41,12,41,628,9,41,1,42,1,
    42,1,42,1,42,1,43,1,43,1,43,5,43,637,8,43,10,43,12,43,640,9,43,1,43,1,
    43,1,43,3,43,645,8,43,1,43,1,43,3,43,649,8,43,1,44,1,44,1,44,1,44,1,44,
    1,44,1,44,1,44,3,44,659,8,44,1,45,1,45,1,45,1,45,1,45,5,45,666,8,45,10,
    45,12,45,669,9,45,1,46,1,46,1,46,1,47,1,47,1,47,1,48,1,48,1,48,1,48,1,
    48,4,48,682,8,48,11,48,12,48,683,3,48,686,8,48,1,48,0,0,49,0,2,4,6,8,
    10,12,14,16,18,20,22,24,26,28,30,32,34,36,38,40,42,44,46,48,50,52,54,
    56,58,60,62,64,66,68,70,72,74,76,78,80,82,84,86,88,90,92,94,96,0,22,1,
    0,7,8,1,0,20,21,2,0,12,12,24,27,2,0,24,24,38,41,3,0,24,24,38,40,42,44,
    2,0,162,162,168,168,2,0,20,21,73,74,1,0,76,78,2,0,56,56,79,80,1,0,84,
    85,2,0,41,41,86,86,1,0,95,96,3,0,24,24,38,38,100,103,18,0,2,6,9,9,11,
    11,13,16,22,23,30,33,35,37,54,55,57,58,60,62,64,68,70,72,76,78,82,85,
    87,89,91,99,104,132,167,167,2,0,109,109,126,127,1,0,116,117,1,0,142,148,
    2,0,122,122,132,132,2,0,20,21,137,138,2,0,26,27,154,155,2,0,26,27,156,
    156,1,0,158,159,789,0,98,1,0,0,0,2,134,1,0,0,0,4,136,1,0,0,0,6,153,1,
    0,0,0,8,155,1,0,0,0,10,163,1,0,0,0,12,168,1,0,0,0,14,170,1,0,0,0,16,173,
    1,0,0,0,18,175,1,0,0,0,20,179,1,0,0,0,22,193,1,0,0,0,24,195,1,0,0,0,26,
    207,1,0,0,0,28,250,1,0,0,0,30,256,1,0,0,0,32,261,1,0,0,0,34,270,1,0,0,
    0,36,278,1,0,0,0,38,280,1,0,0,0,40,312,1,0,0,0,42,355,1,0,0,0,44,362,
    1,0,0,0,46,378,1,0,0,0,48,380,1,0,0,0,50,411,1,0,0,0,52,414,1,0,0,0,54,
    417,1,0,0,0,56,420,1,0,0,0,58,431,1,0,0,0,60,433,1,0,0,0,62,451,1,0,0,
    0,64,458,1,0,0,0,66,460,1,0,0,0,68,462,1,0,0,0,70,539,1,0,0,0,72,559,
    1,0,0,0,74,586,1,0,0,0,76,594,1,0,0,0,78,596,1,0,0,0,80,614,1,0,0,0,82,
    620,1,0,0,0,84,629,1,0,0,0,86,633,1,0,0,0,88,658,1,0,0,0,90,660,1,0,0,
    0,92,670,1,0,0,0,94,673,1,0,0,0,96,685,1,0,0,0,98,99,5,1,0,0,99,100,3,
    2,1,0,100,101,5,0,0,1,101,1,1,0,0,0,102,135,3,4,2,0,103,135,3,8,4,0,104,
    135,3,10,5,0,105,135,3,12,6,0,106,135,3,14,7,0,107,135,3,16,8,0,108,135,
    3,18,9,0,109,135,3,20,10,0,110,135,3,22,11,0,111,135,3,24,12,0,112,135,
    3,28,14,0,113,135,3,38,19,0,114,135,3,40,20,0,115,135,3,42,21,0,116,135,
    3,44,22,0,117,135,3,48,24,0,118,135,3,50,25,0,119,135,3,52,26,0,120,135,
    3,54,27,0,121,135,3,56,28,0,122,135,3,58,29,0,123,135,3,60,30,0,124,135,
    3,68,34,0,125,135,3,70,35,0,126,135,3,72,36,0,127,135,3,74,37,0,128,135,
    3,80,40,0,129,135,3,82,41,0,130,135,3,86,43,0,131,135,3,90,45,0,132,135,
    3,84,42,0,133,135,3,78,39,0,134,102,1,0,0,0,134,103,1,0,0,0,134,104,1,
    0,0,0,134,105,1,0,0,0,134,106,1,0,0,0,134,107,1,0,0,0,134,108,1,0,0,0,
    134,109,1,0,0,0,134,110,1,0,0,0,134,111,1,0,0,0,134,112,1,0,0,0,134,113,
    1,0,0,0,134,114,1,0,0,0,134,115,1,0,0,0,134,116,1,0,0,0,134,117,1,0,0,
    0,134,118,1,0,0,0,134,119,1,0,0,0,134,120,1,0,0,0,134,121,1,0,0,0,134,
    122,1,0,0,0,134,123,1,0,0,0,134,124,1,0,0,0,134,125,1,0,0,0,134,126,1,
    0,0,0,134,127,1,0,0,0,134,128,1,0,0,0,134,129,1,0,0,0,134,130,1,0,0,0,
    134,131,1,0,0,0,134,132,1,0,0,0,134,133,1,0,0,0,135,3,1,0,0,0,136,148,
    5,2,0,0,137,141,5,3,0,0,138,140,3,6,3,0,139,138,1,0,0,0,140,143,1,0,0,
    0,141,139,1,0,0,0,141,142,1,0,0,0,142,149,1,0,0,0,143,141,1,0,0,0,144,
    145,5,4,0,0,145,149,3,64,32,0,146,149,5,5,0,0,147,149,5,6,0,0,148,137,
    1,0,0,0,148,144,1,0,0,0,148,146,1,0,0,0,148,147,1,0,0,0,149,5,1,0,0,0,
    150,151,7,0,0,0,151,154,3,64,32,0,152,154,3,94,47,0,153,150,1,0,0,0,153,
    152,1,0,0,0,154,7,1,0,0,0,155,161,5,9,0,0,156,159,5,162,0,0,157,158,5,
    10,0,0,158,160,5,162,0,0,159,157,1,0,0,0,159,160,1,0,0,0,160,162,1,0,
    0,0,161,156,1,0,0,0,161,162,1,0,0,0,162,9,1,0,0,0,163,166,5,11,0,0,164,
    165,5,12,0,0,165,167,3,64,32,0,166,164,1,0,0,0,166,167,1,0,0,0,167,11,
    1,0,0,0,168,169,5,13,0,0,169,13,1,0,0,0,170,171,5,14,0,0,171,172,3,64,
    32,0,172,15,1,0,0,0,173,174,5,5,0,0,174,17,1,0,0,0,175,177,5,15,0,0,176,
    178,3,64,32,0,177,176,1,0,0,0,177,178,1,0,0,0,178,19,1,0,0,0,179,191,
    5,16,0,0,180,192,5,17,0,0,181,182,5,18,0,0,182,192,3,64,32,0,183,188,
    5,19,0,0,184,185,7,1,0,0,185,187,5,162,0,0,186,184,1,0,0,0,187,190,1,
    0,0,0,188,186,1,0,0,0,188,189,1,0,0,0,189,192,1,0,0,0,190,188,1,0,0,0,
    191,180,1,0,0,0,191,181,1,0,0,0,191,183,1,0,0,0,191,192,1,0,0,0,192,21,
    1,0,0,0,193,194,5,22,0,0,194,23,1,0,0,0,195,199,5,23,0,0,196,198,3,26,
    13,0,197,196,1,0,0,0,198,201,1,0,0,0,199,197,1,0,0,0,199,200,1,0,0,0,
    200,25,1,0,0,0,201,199,1,0,0,0,202,203,7,2,0,0,203,208,3,64,32,0,204,
    205,5,28,0,0,205,208,5,162,0,0,206,208,5,29,0,0,207,202,1,0,0,0,207,204,
    1,0,0,0,207,206,1,0,0,0,208,27,1,0,0,0,209,247,5,30,0,0,210,211,5,31,
    0,0,211,213,3,64,32,0,212,214,3,30,15,0,213,212,1,0,0,0,214,215,1,0,0,
    0,215,213,1,0,0,0,215,216,1,0,0,0,216,248,1,0,0,0,217,218,5,32,0,0,218,
    222,3,64,32,0,219,221,3,32,16,0,220,219,1,0,0,0,221,224,1,0,0,0,222,220,
    1,0,0,0,222,223,1,0,0,0,223,248,1,0,0,0,224,222,1,0,0,0,225,226,5,33,
    0,0,226,227,3,64,32,0,227,228,5,34,0,0,228,232,3,64,32,0,229,231,3,36,
    18,0,230,229,1,0,0,0,231,234,1,0,0,0,232,230,1,0,0,0,232,233,1,0,0,0,
    233,248,1,0,0,0,234,232,1,0,0,0,235,236,5,35,0,0,236,237,3,64,32,0,237,
    238,5,34,0,0,238,242,3,64,32,0,239,241,3,34,17,0,240,239,1,0,0,0,241,
    244,1,0,0,0,242,240,1,0,0,0,242,243,1,0,0,0,243,248,1,0,0,0,244,242,1,
    0,0,0,245,246,5,36,0,0,246,248,5,162,0,0,247,210,1,0,0,0,247,217,1,0,
    0,0,247,225,1,0,0,0,247,235,1,0,0,0,247,245,1,0,0,0,248,251,1,0,0,0,249,
    251,5,37,0,0,250,209,1,0,0,0,250,249,1,0,0,0,251,29,1,0,0,0,252,253,7,
    3,0,0,253,257,3,64,32,0,254,257,5,165,0,0,255,257,3,96,48,0,256,252,1,
    0,0,0,256,254,1,0,0,0,256,255,1,0,0,0,257,31,1,0,0,0,258,259,7,4,0,0,
    259,262,3,64,32,0,260,262,3,96,48,0,261,258,1,0,0,0,261,260,1,0,0,0,262,
    33,1,0,0,0,263,271,3,32,16,0,264,271,5,45,0,0,265,271,5,46,0,0,266,271,
    5,47,0,0,267,271,5,48,0,0,268,271,5,49,0,0,269,271,5,50,0,0,270,263,1,
    0,0,0,270,264,1,0,0,0,270,265,1,0,0,0,270,266,1,0,0,0,270,267,1,0,0,0,
    270,268,1,0,0,0,270,269,1,0,0,0,271,35,1,0,0,0,272,279,3,34,17,0,273,
    274,5,51,0,0,274,279,7,5,0,0,275,279,5,52,0,0,276,279,5,53,0,0,277,279,
    3,92,46,0,278,272,1,0,0,0,278,273,1,0,0,0,278,275,1,0,0,0,278,276,1,0,
    0,0,278,277,1,0,0,0,279,37,1,0,0,0,280,310,5,54,0,0,281,282,5,55,0,0,
    282,283,3,64,32,0,283,284,5,56,0,0,284,285,3,64,32,0,285,311,1,0,0,0,
    286,287,5,57,0,0,287,311,3,64,32,0,288,289,5,58,0,0,289,311,3,64,32,0,
    290,291,5,35,0,0,291,292,3,64,32,0,292,293,5,59,0,0,293,297,3,64,32,0,
    294,296,3,34,17,0,295,294,1,0,0,0,296,299,1,0,0,0,297,295,1,0,0,0,297,
    298,1,0,0,0,298,311,1,0,0,0,299,297,1,0,0,0,300,301,5,60,0,0,301,302,
    3,64,32,0,302,303,5,59,0,0,303,307,3,64,32,0,304,306,3,36,18,0,305,304,
    1,0,0,0,306,309,1,0,0,0,307,305,1,0,0,0,307,308,1,0,0,0,308,311,1,0,0,
    0,309,307,1,0,0,0,310,281,1,0,0,0,310,286,1,0,0,0,310,288,1,0,0,0,310,
    290,1,0,0,0,310,300,1,0,0,0,311,39,1,0,0,0,312,353,5,61,0,0,313,315,3,
    46,23,0,314,313,1,0,0,0,315,318,1,0,0,0,316,314,1,0,0,0,316,317,1,0,0,
    0,317,354,1,0,0,0,318,316,1,0,0,0,319,320,5,62,0,0,320,323,3,64,32,0,
    321,322,5,28,0,0,322,324,5,162,0,0,323,321,1,0,0,0,323,324,1,0,0,0,324,
    354,1,0,0,0,325,326,5,58,0,0,326,327,3,64,32,0,327,328,5,63,0,0,328,329,
    5,162,0,0,329,354,1,0,0,0,330,331,5,167,0,0,331,332,3,64,32,0,332,333,
    5,63,0,0,333,334,5,162,0,0,334,354,1,0,0,0,335,336,5,64,0,0,336,354,3,
    64,32,0,337,338,5,65,0,0,338,354,3,64,32,0,339,340,5,66,0,0,340,354,3,
    64,32,0,341,342,5,67,0,0,342,343,3,64,32,0,343,344,5,56,0,0,344,345,3,
    64,32,0,345,354,1,0,0,0,346,347,5,68,0,0,347,348,3,64,32,0,348,349,5,
    69,0,0,349,350,5,70,0,0,350,351,5,41,0,0,351,352,3,64,32,0,352,354,1,
    0,0,0,353,316,1,0,0,0,353,319,1,0,0,0,353,325,1,0,0,0,353,330,1,0,0,0,
    353,335,1,0,0,0,353,337,1,0,0,0,353,339,1,0,0,0,353,341,1,0,0,0,353,346,
    1,0,0,0,354,41,1,0,0,0,355,359,5,71,0,0,356,358,3,46,23,0,357,356,1,0,
    0,0,358,361,1,0,0,0,359,357,1,0,0,0,359,360,1,0,0,0,360,43,1,0,0,0,361,
    359,1,0,0,0,362,363,5,72,0,0,363,368,3,64,32,0,364,367,3,46,23,0,365,
    367,5,166,0,0,366,364,1,0,0,0,366,365,1,0,0,0,367,370,1,0,0,0,368,366,
    1,0,0,0,368,369,1,0,0,0,369,45,1,0,0,0,370,368,1,0,0,0,371,372,7,6,0,
    0,372,379,5,162,0,0,373,374,5,75,0,0,374,379,7,7,0,0,375,376,7,8,0,0,
    376,379,3,64,32,0,377,379,5,81,0,0,378,371,1,0,0,0,378,373,1,0,0,0,378,
    375,1,0,0,0,378,377,1,0,0,0,379,47,1,0,0,0,380,409,5,82,0,0,381,382,5,
    55,0,0,382,410,3,64,32,0,383,410,5,83,0,0,384,385,7,9,0,0,385,390,3,64,
    32,0,386,387,7,10,0,0,387,389,3,64,32,0,388,386,1,0,0,0,389,392,1,0,0,
    0,390,388,1,0,0,0,390,391,1,0,0,0,391,410,1,0,0,0,392,390,1,0,0,0,393,
    407,5,87,0,0,394,397,5,88,0,0,395,398,3,64,32,0,396,398,5,164,0,0,397,
    395,1,0,0,0,397,396,1,0,0,0,398,399,1,0,0,0,399,397,1,0,0,0,399,400,1,
    0,0,0,400,408,1,0,0,0,401,408,5,66,0,0,402,403,5,89,0,0,403,404,3,64,
    32,0,404,405,5,90,0,0,405,406,3,64,32,0,406,408,1,0,0,0,407,394,1,0,0,
    0,407,401,1,0,0,0,407,402,1,0,0,0,407,408,1,0,0,0,408,410,1,0,0,0,409,
    381,1,0,0,0,409,383,1,0,0,0,409,384,1,0,0,0,409,393,1,0,0,0,410,49,1,
    0,0,0,411,412,5,91,0,0,412,413,3,64,32,0,413,51,1,0,0,0,414,415,5,92,
    0,0,415,416,3,64,32,0,416,53,1,0,0,0,417,418,5,93,0,0,418,419,3,64,32,
    0,419,55,1,0,0,0,420,429,5,94,0,0,421,423,5,1,0,0,422,421,1,0,0,0,422,
    423,1,0,0,0,423,425,1,0,0,0,424,426,3,64,32,0,425,424,1,0,0,0,426,427,
    1,0,0,0,427,425,1,0,0,0,427,428,1,0,0,0,428,430,1,0,0,0,429,422,1,0,0,
    0,429,430,1,0,0,0,430,57,1,0,0,0,431,432,7,11,0,0,432,59,1,0,0,0,433,
    449,5,97,0,0,434,436,5,98,0,0,435,437,3,62,31,0,436,435,1,0,0,0,437,438,
    1,0,0,0,438,436,1,0,0,0,438,439,1,0,0,0,439,450,1,0,0,0,440,441,5,99,
    0,0,441,442,5,100,0,0,442,445,3,64,32,0,443,444,5,34,0,0,444,446,3,64,
    32,0,445,443,1,0,0,0,445,446,1,0,0,0,446,450,1,0,0,0,447,448,5,36,0,0,
    448,450,3,64,32,0,449,434,1,0,0,0,449,440,1,0,0,0,449,447,1,0,0,0,450,
    61,1,0,0,0,451,452,7,12,0,0,452,453,3,64,32,0,453,63,1,0,0,0,454,459,
    5,163,0,0,455,459,5,168,0,0,456,459,5,162,0,0,457,459,3,66,33,0,458,454,
    1,0,0,0,458,455,1,0,0,0,458,456,1,0,0,0,458,457,1,0,0,0,459,65,1,0,0,
    0,460,461,7,13,0,0,461,67,1,0,0,0,462,537,5,104,0,0,463,538,5,105,0,0,
    464,465,5,106,0,0,465,466,3,64,32,0,466,467,5,27,0,0,467,472,3,64,32,
    0,468,469,5,21,0,0,469,473,5,162,0,0,470,471,5,86,0,0,471,473,3,64,32,
    0,472,468,1,0,0,0,472,470,1,0,0,0,473,538,1,0,0,0,474,475,5,107,0,0,475,
    476,3,64,32,0,476,477,5,27,0,0,477,478,3,64,32,0,478,479,5,21,0,0,479,
    480,5,162,0,0,480,481,5,133,0,0,481,484,3,64,32,0,482,483,5,28,0,0,483,
    485,5,162,0,0,484,482,1,0,0,0,484,485,1,0,0,0,485,538,1,0,0,0,486,487,
    5,108,0,0,487,488,5,24,0,0,488,489,3,64,32,0,489,490,5,134,0,0,490,491,
    3,64,32,0,491,492,5,27,0,0,492,493,3,64,32,0,493,494,5,135,0,0,494,495,
    3,64,32,0,495,496,5,136,0,0,496,497,3,64,32,0,497,538,1,0,0,0,498,499,
    5,128,0,0,499,500,3,64,32,0,500,501,5,27,0,0,501,502,3,64,32,0,502,503,
    5,135,0,0,503,504,3,64,32,0,504,505,5,136,0,0,505,506,3,64,32,0,506,538,
    1,0,0,0,507,508,7,14,0,0,508,538,5,162,0,0,509,510,5,111,0,0,510,511,
    5,137,0,0,511,512,5,162,0,0,512,513,5,26,0,0,513,514,3,64,32,0,514,515,
    5,138,0,0,515,516,5,162,0,0,516,517,5,27,0,0,517,520,3,64,32,0,518,519,
    5,20,0,0,519,521,5,162,0,0,520,518,1,0,0,0,520,521,1,0,0,0,521,538,1,
    0,0,0,522,538,5,112,0,0,523,524,5,110,0,0,524,525,5,24,0,0,525,526,3,
    64,32,0,526,527,5,139,0,0,527,528,3,64,32,0,528,529,5,140,0,0,529,530,
    3,64,32,0,530,531,5,26,0,0,531,532,3,64,32,0,532,533,5,27,0,0,533,534,
    3,64,32,0,534,535,5,141,0,0,535,536,3,64,32,0,536,538,1,0,0,0,537,463,
    1,0,0,0,537,464,1,0,0,0,537,474,1,0,0,0,537,486,1,0,0,0,537,498,1,0,0,
    0,537,507,1,0,0,0,537,509,1,0,0,0,537,522,1,0,0,0,537,523,1,0,0,0,538,
    69,1,0,0,0,539,540,5,30,0,0,540,557,5,113,0,0,541,542,3,64,32,0,542,543,
    5,27,0,0,543,546,3,64,32,0,544,545,5,24,0,0,545,547,3,64,32,0,546,544,
    1,0,0,0,546,547,1,0,0,0,547,558,1,0,0,0,548,549,5,24,0,0,549,550,3,64,
    32,0,550,551,5,134,0,0,551,552,3,64,32,0,552,553,5,141,0,0,553,554,3,
    64,32,0,554,555,5,27,0,0,555,556,3,64,32,0,556,558,1,0,0,0,557,541,1,
    0,0,0,557,548,1,0,0,0,558,71,1,0,0,0,559,560,5,30,0,0,560,561,5,114,0,
    0,561,562,5,162,0,0,562,563,5,34,0,0,563,572,3,64,32,0,564,569,5,19,0,
    0,565,566,7,1,0,0,566,568,5,162,0,0,567,565,1,0,0,0,568,571,1,0,0,0,569,
    567,1,0,0,0,569,570,1,0,0,0,570,573,1,0,0,0,571,569,1,0,0,0,572,564,1,
    0,0,0,572,573,1,0,0,0,573,73,1,0,0,0,574,575,5,115,0,0,575,579,3,64,32,
    0,576,578,3,76,38,0,577,576,1,0,0,0,578,581,1,0,0,0,579,577,1,0,0,0,579,
    580,1,0,0,0,580,587,1,0,0,0,581,579,1,0,0,0,582,584,7,15,0,0,583,585,
    5,162,0,0,584,583,1,0,0,0,584,585,1,0,0,0,585,587,1,0,0,0,586,574,1,0,
    0,0,586,582,1,0,0,0,587,75,1,0,0,0,588,589,7,16,0,0,589,595,3,64,32,0,
    590,591,5,20,0,0,591,595,5,162,0,0,592,595,5,149,0,0,593,595,5,150,0,
    0,594,588,1,0,0,0,594,590,1,0,0,0,594,592,1,0,0,0,594,593,1,0,0,0,595,
    77,1,0,0,0,596,597,5,130,0,0,597,79,1,0,0,0,598,599,5,118,0,0,599,606,
    3,64,32,0,600,601,5,142,0,0,601,605,3,64,32,0,602,603,5,20,0,0,603,605,
    5,162,0,0,604,600,1,0,0,0,604,602,1,0,0,0,605,608,1,0,0,0,606,604,1,0,
    0,0,606,607,1,0,0,0,607,615,1,0,0,0,608,606,1,0,0,0,609,612,5,119,0,0,
    610,611,5,20,0,0,611,613,5,162,0,0,612,610,1,0,0,0,612,613,1,0,0,0,613,
    615,1,0,0,0,614,598,1,0,0,0,614,609,1,0,0,0,615,81,1,0,0,0,616,621,5,
    120,0,0,617,621,5,131,0,0,618,619,5,121,0,0,619,621,7,17,0,0,620,616,
    1,0,0,0,620,617,1,0,0,0,620,618,1,0,0,0,621,626,1,0,0,0,622,623,7,1,0,
    0,623,625,5,162,0,0,624,622,1,0,0,0,625,628,1,0,0,0,626,624,1,0,0,0,626,
    627,1,0,0,0,627,83,1,0,0,0,628,626,1,0,0,0,629,630,5,125,0,0,630,631,
    5,55,0,0,631,632,3,64,32,0,632,85,1,0,0,0,633,648,5,123,0,0,634,638,5,
    3,0,0,635,637,3,88,44,0,636,635,1,0,0,0,637,640,1,0,0,0,638,636,1,0,0,
    0,638,639,1,0,0,0,639,649,1,0,0,0,640,638,1,0,0,0,641,644,5,6,0,0,642,
    643,5,103,0,0,643,645,3,64,32,0,644,642,1,0,0,0,644,645,1,0,0,0,645,649,
    1,0,0,0,646,647,5,129,0,0,647,649,3,64,32,0,648,634,1,0,0,0,648,641,1,
    0,0,0,648,646,1,0,0,0,649,87,1,0,0,0,650,651,7,18,0,0,651,659,5,162,0,
    0,652,653,5,151,0,0,653,659,5,162,0,0,654,659,5,152,0,0,655,659,5,153,
    0,0,656,657,7,19,0,0,657,659,3,64,32,0,658,650,1,0,0,0,658,652,1,0,0,
    0,658,654,1,0,0,0,658,655,1,0,0,0,658,656,1,0,0,0,659,89,1,0,0,0,660,
    667,5,124,0,0,661,662,7,18,0,0,662,666,5,162,0,0,663,664,7,20,0,0,664,
    666,3,64,32,0,665,661,1,0,0,0,665,663,1,0,0,0,666,669,1,0,0,0,667,665,
    1,0,0,0,667,668,1,0,0,0,668,91,1,0,0,0,669,667,1,0,0,0,670,671,5,157,
    0,0,671,672,3,64,32,0,672,93,1,0,0,0,673,674,7,21,0,0,674,675,5,162,0,
    0,675,95,1,0,0,0,676,677,5,160,0,0,677,686,3,64,32,0,678,681,5,161,0,
    0,679,682,3,64,32,0,680,682,5,164,0,0,681,679,1,0,0,0,681,680,1,0,0,0,
    682,683,1,0,0,0,683,681,1,0,0,0,683,684,1,0,0,0,684,686,1,0,0,0,685,676,
    1,0,0,0,685,678,1,0,0,0,686,97,1,0,0,0,71,134,141,148,153,159,161,166,
    177,188,191,199,207,215,222,232,242,247,250,256,261,270,278,297,307,310,
    316,323,353,359,366,368,378,390,397,399,407,409,422,427,429,438,445,449,
    458,472,484,520,537,546,557,569,572,579,584,586,594,604,606,612,614,620,
    626,638,644,648,658,665,667,681,683,685
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
    setState(98);
    match(TracerCommandsParser::T__0);
    setState(99);
    command();
    setState(100);
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
    setState(134);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 0, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(102);
      server();
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(103);
      attach();
      break;
    }

    case 3: {
      enterOuterAlt(_localctx, 3);
      setState(104);
      processes();
      break;
    }

    case 4: {
      enterOuterAlt(_localctx, 4);
      setState(105);
      sessions();
      break;
    }

    case 5: {
      enterOuterAlt(_localctx, 5);
      setState(106);
      use();
      break;
    }

    case 6: {
      enterOuterAlt(_localctx, 6);
      setState(107);
      status();
      break;
    }

    case 7: {
      enterOuterAlt(_localctx, 7);
      setState(108);
      detach();
      break;
    }

    case 8: {
      enterOuterAlt(_localctx, 8);
      setState(109);
      modules();
      break;
    }

    case 9: {
      enterOuterAlt(_localctx, 9);
      setState(110);
      threads();
      break;
    }

    case 10: {
      enterOuterAlt(_localctx, 10);
      setState(111);
      symbols();
      break;
    }

    case 11: {
      enterOuterAlt(_localctx, 11);
      setState(112);
      probe();
      break;
    }

    case 12: {
      enterOuterAlt(_localctx, 12);
      setState(113);
      interceptor();
      break;
    }

    case 13: {
      enterOuterAlt(_localctx, 13);
      setState(114);
      events();
      break;
    }

    case 14: {
      enterOuterAlt(_localctx, 14);
      setState(115);
      collect();
      break;
    }

    case 15: {
      enterOuterAlt(_localctx, 15);
      setState(116);
      record();
      break;
    }

    case 16: {
      enterOuterAlt(_localctx, 16);
      setState(117);
      cpp();
      break;
    }

    case 17: {
      enterOuterAlt(_localctx, 17);
      setState(118);
      shell();
      break;
    }

    case 18: {
      enterOuterAlt(_localctx, 18);
      setState(119);
      complete();
      break;
    }

    case 19: {
      enterOuterAlt(_localctx, 19);
      setState(120);
      type();
      break;
    }

    case 20: {
      enterOuterAlt(_localctx, 20);
      setState(121);
      help();
      break;
    }

    case 21: {
      enterOuterAlt(_localctx, 21);
      setState(122);
      quit();
      break;
    }

    case 22: {
      enterOuterAlt(_localctx, 22);
      setState(123);
      bulkTrace();
      break;
    }

    case 23: {
      enterOuterAlt(_localctx, 23);
      setState(124);
      memory();
      break;
    }

    case 24: {
      enterOuterAlt(_localctx, 24);
      setState(125);
      probeEntry();
      break;
    }

    case 25: {
      enterOuterAlt(_localctx, 25);
      setState(126);
      probeReload();
      break;
    }

    case 26: {
      enterOuterAlt(_localctx, 26);
      setState(127);
      targetLaunch();
      break;
    }

    case 27: {
      enterOuterAlt(_localctx, 27);
      setState(128);
      targetCall();
      break;
    }

    case 28: {
      enterOuterAlt(_localctx, 28);
      setState(129);
      localPerf();
      break;
    }

    case 29: {
      enterOuterAlt(_localctx, 29);
      setState(130);
      instructions();
      break;
    }

    case 30: {
      enterOuterAlt(_localctx, 30);
      setState(131);
      watchpoint();
      break;
    }

    case 31: {
      enterOuterAlt(_localctx, 31);
      setState(132);
      targetLibrary();
      break;
    }

    case 32: {
      enterOuterAlt(_localctx, 32);
      setState(133);
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
    setState(136);
    match(TracerCommandsParser::T__1);
    setState(148);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__2: {
        setState(137);
        match(TracerCommandsParser::T__2);
        setState(141);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__6

        || _la == TracerCommandsParser::T__7 || _la == TracerCommandsParser::T__157

        || _la == TracerCommandsParser::T__158) {
          setState(138);
          serverOption();
          setState(143);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__3: {
        setState(144);
        match(TracerCommandsParser::T__3);
        setState(145);
        value();
        break;
      }

      case TracerCommandsParser::T__4: {
        setState(146);
        match(TracerCommandsParser::T__4);
        break;
      }

      case TracerCommandsParser::T__5: {
        setState(147);
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
    setState(153);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__6:
      case TracerCommandsParser::T__7: {
        enterOuterAlt(_localctx, 1);
        setState(150);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__6

        || _la == TracerCommandsParser::T__7)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(151);
        value();
        break;
      }

      case TracerCommandsParser::T__157:
      case TracerCommandsParser::T__158: {
        enterOuterAlt(_localctx, 2);
        setState(152);
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
    setState(155);
    match(TracerCommandsParser::T__8);
    setState(161);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::UINT) {
      setState(156);
      match(TracerCommandsParser::UINT);
      setState(159);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == TracerCommandsParser::T__9) {
        setState(157);
        match(TracerCommandsParser::T__9);
        setState(158);
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
    setState(163);
    match(TracerCommandsParser::T__10);
    setState(166);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::T__11) {
      setState(164);
      match(TracerCommandsParser::T__11);
      setState(165);
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
    setState(168);
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
    setState(170);
    match(TracerCommandsParser::T__13);
    setState(171);
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
    setState(173);
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
    setState(175);
    match(TracerCommandsParser::T__14);
    setState(177);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 8556839548640946812) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 64)) & -1030863687201) != 0) || ((((_la - 128) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 128)) & 1700807049247) != 0)) {
      setState(176);
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
    setState(179);
    match(TracerCommandsParser::T__15);
    setState(191);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__16: {
        setState(180);
        match(TracerCommandsParser::T__16);
        break;
      }

      case TracerCommandsParser::T__17: {
        setState(181);
        match(TracerCommandsParser::T__17);
        setState(182);
        value();
        break;
      }

      case TracerCommandsParser::T__18: {
        setState(183);
        match(TracerCommandsParser::T__18);
        setState(188);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__19

        || _la == TracerCommandsParser::T__20) {
          setState(184);
          _la = _input->LA(1);
          if (!(_la == TracerCommandsParser::T__19

          || _la == TracerCommandsParser::T__20)) {
          _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(185);
          match(TracerCommandsParser::UINT);
          setState(190);
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
    setState(193);
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
    setState(195);
    match(TracerCommandsParser::T__22);
    setState(199);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 1056968704) != 0)) {
      setState(196);
      symbolOption();
      setState(201);
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
    setState(207);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__11:
      case TracerCommandsParser::T__23:
      case TracerCommandsParser::T__24:
      case TracerCommandsParser::T__25:
      case TracerCommandsParser::T__26: {
        enterOuterAlt(_localctx, 1);
        setState(202);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 251662336) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(203);
        value();
        break;
      }

      case TracerCommandsParser::T__27: {
        enterOuterAlt(_localctx, 2);
        setState(204);
        match(TracerCommandsParser::T__27);
        setState(205);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__28: {
        enterOuterAlt(_localctx, 3);
        setState(206);
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
    setState(250);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__29: {
        enterOuterAlt(_localctx, 1);
        setState(209);
        match(TracerCommandsParser::T__29);
        setState(247);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__30: {
            setState(210);
            match(TracerCommandsParser::T__30);
            setState(211);
            value();
            setState(213);
            _errHandler->sync(this);
            _la = _input->LA(1);
            do {
              setState(212);
              prepareOption();
              setState(215);
              _errHandler->sync(this);
              _la = _input->LA(1);
            } while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 4123185381376) != 0) || ((((_la - 160) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 160)) & 35) != 0));
            break;
          }

          case TracerCommandsParser::T__31: {
            setState(217);
            match(TracerCommandsParser::T__31);
            setState(218);
            value();
            setState(222);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 32710487703552) != 0) || _la == TracerCommandsParser::T__159

            || _la == TracerCommandsParser::T__160) {
              setState(219);
              resolveOption();
              setState(224);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__32: {
            setState(225);
            match(TracerCommandsParser::T__32);
            setState(226);
            value();
            setState(227);
            match(TracerCommandsParser::T__33);
            setState(228);
            value();
            setState(232);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 18011924625096704) != 0) || ((((_la - 157) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 157)) & 25) != 0)) {
              setState(229);
              deployOption();
              setState(234);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__34: {
            setState(235);
            match(TracerCommandsParser::T__34);
            setState(236);
            value();
            setState(237);
            match(TracerCommandsParser::T__33);
            setState(238);
            value();
            setState(242);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 2249325929299968) != 0) || _la == TracerCommandsParser::T__159

            || _la == TracerCommandsParser::T__160) {
              setState(239);
              validateOption();
              setState(244);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__35: {
            setState(245);
            match(TracerCommandsParser::T__35);
            setState(246);
            match(TracerCommandsParser::UINT);
            break;
          }

        default:
          throw NoViableAltException(this);
        }
        break;
      }

      case TracerCommandsParser::T__36: {
        enterOuterAlt(_localctx, 2);
        setState(249);
        match(TracerCommandsParser::T__36);
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
    setState(256);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__23:
      case TracerCommandsParser::T__37:
      case TracerCommandsParser::T__38:
      case TracerCommandsParser::T__39:
      case TracerCommandsParser::T__40: {
        enterOuterAlt(_localctx, 1);
        setState(252);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 4123185381376) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(253);
        value();
        break;
      }

      case TracerCommandsParser::METADATA_ONLY: {
        enterOuterAlt(_localctx, 2);
        setState(254);
        match(TracerCommandsParser::METADATA_ONLY);
        break;
      }

      case TracerCommandsParser::T__159:
      case TracerCommandsParser::T__160: {
        enterOuterAlt(_localctx, 3);
        setState(255);
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
    setState(261);
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
        setState(258);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 32710487703552) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(259);
        value();
        break;
      }

      case TracerCommandsParser::T__159:
      case TracerCommandsParser::T__160: {
        enterOuterAlt(_localctx, 2);
        setState(260);
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
    setState(270);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__23:
      case TracerCommandsParser::T__37:
      case TracerCommandsParser::T__38:
      case TracerCommandsParser::T__39:
      case TracerCommandsParser::T__41:
      case TracerCommandsParser::T__42:
      case TracerCommandsParser::T__43:
      case TracerCommandsParser::T__159:
      case TracerCommandsParser::T__160: {
        enterOuterAlt(_localctx, 1);
        setState(263);
        resolveOption();
        break;
      }

      case TracerCommandsParser::T__44: {
        enterOuterAlt(_localctx, 2);
        setState(264);
        match(TracerCommandsParser::T__44);
        break;
      }

      case TracerCommandsParser::T__45: {
        enterOuterAlt(_localctx, 3);
        setState(265);
        match(TracerCommandsParser::T__45);
        break;
      }

      case TracerCommandsParser::T__46: {
        enterOuterAlt(_localctx, 4);
        setState(266);
        match(TracerCommandsParser::T__46);
        break;
      }

      case TracerCommandsParser::T__47: {
        enterOuterAlt(_localctx, 5);
        setState(267);
        match(TracerCommandsParser::T__47);
        break;
      }

      case TracerCommandsParser::T__48: {
        enterOuterAlt(_localctx, 6);
        setState(268);
        match(TracerCommandsParser::T__48);
        break;
      }

      case TracerCommandsParser::T__49: {
        enterOuterAlt(_localctx, 7);
        setState(269);
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
    setState(278);
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
      case TracerCommandsParser::T__159:
      case TracerCommandsParser::T__160: {
        enterOuterAlt(_localctx, 1);
        setState(272);
        validateOption();
        break;
      }

      case TracerCommandsParser::T__50: {
        enterOuterAlt(_localctx, 2);
        setState(273);
        match(TracerCommandsParser::T__50);
        setState(274);
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
        setState(275);
        match(TracerCommandsParser::T__51);
        break;
      }

      case TracerCommandsParser::T__52: {
        enterOuterAlt(_localctx, 4);
        setState(276);
        match(TracerCommandsParser::T__52);
        break;
      }

      case TracerCommandsParser::T__156: {
        enterOuterAlt(_localctx, 5);
        setState(277);
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
    setState(280);
    match(TracerCommandsParser::T__53);
    setState(310);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__54: {
        setState(281);
        match(TracerCommandsParser::T__54);
        setState(282);
        value();
        setState(283);
        match(TracerCommandsParser::T__55);
        setState(284);
        value();
        break;
      }

      case TracerCommandsParser::T__56: {
        setState(286);
        match(TracerCommandsParser::T__56);
        setState(287);
        value();
        break;
      }

      case TracerCommandsParser::T__57: {
        setState(288);
        match(TracerCommandsParser::T__57);
        setState(289);
        value();
        break;
      }

      case TracerCommandsParser::T__34: {
        setState(290);
        match(TracerCommandsParser::T__34);
        setState(291);
        value();
        setState(292);
        match(TracerCommandsParser::T__58);
        setState(293);
        value();
        setState(297);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 2249325929299968) != 0) || _la == TracerCommandsParser::T__159

        || _la == TracerCommandsParser::T__160) {
          setState(294);
          validateOption();
          setState(299);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__59: {
        setState(300);
        match(TracerCommandsParser::T__59);
        setState(301);
        value();
        setState(302);
        match(TracerCommandsParser::T__58);
        setState(303);
        value();
        setState(307);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 18011924625096704) != 0) || ((((_la - 157) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 157)) & 25) != 0)) {
          setState(304);
          deployOption();
          setState(309);
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
    setState(312);
    match(TracerCommandsParser::T__60);
    setState(353);
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
        setState(316);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (((((_la - 20) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 20)) & 4098275729626628099) != 0)) {
          setState(313);
          streamOption();
          setState(318);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__61: {
        setState(319);
        match(TracerCommandsParser::T__61);
        setState(320);
        value();
        setState(323);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__27) {
          setState(321);
          match(TracerCommandsParser::T__27);
          setState(322);
          match(TracerCommandsParser::UINT);
        }
        break;
      }

      case TracerCommandsParser::T__57: {
        setState(325);
        match(TracerCommandsParser::T__57);
        setState(326);
        value();
        setState(327);
        match(TracerCommandsParser::T__62);
        setState(328);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::STACK: {
        setState(330);
        match(TracerCommandsParser::STACK);
        setState(331);
        value();
        setState(332);
        match(TracerCommandsParser::T__62);
        setState(333);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__63: {
        setState(335);
        match(TracerCommandsParser::T__63);
        setState(336);
        value();
        break;
      }

      case TracerCommandsParser::T__64: {
        setState(337);
        match(TracerCommandsParser::T__64);
        setState(338);
        value();
        break;
      }

      case TracerCommandsParser::T__65: {
        setState(339);
        match(TracerCommandsParser::T__65);
        setState(340);
        value();
        break;
      }

      case TracerCommandsParser::T__66: {
        setState(341);
        match(TracerCommandsParser::T__66);
        setState(342);
        value();
        setState(343);
        match(TracerCommandsParser::T__55);
        setState(344);
        value();
        break;
      }

      case TracerCommandsParser::T__67: {
        setState(346);
        match(TracerCommandsParser::T__67);
        setState(347);
        value();
        setState(348);
        match(TracerCommandsParser::T__68);
        setState(349);
        match(TracerCommandsParser::T__69);
        setState(350);
        match(TracerCommandsParser::T__40);
        setState(351);
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
    setState(355);
    match(TracerCommandsParser::T__70);
    setState(359);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (((((_la - 20) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 20)) & 4098275729626628099) != 0)) {
      setState(356);
      streamOption();
      setState(361);
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
    setState(362);
    match(TracerCommandsParser::T__71);
    setState(363);
    value();
    setState(368);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (((((_la - 20) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 20)) & 4098275729626628099) != 0) || _la == TracerCommandsParser::SQLITE) {
      setState(366);
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
          setState(364);
          streamOption();
          break;
        }

        case TracerCommandsParser::SQLITE: {
          setState(365);
          match(TracerCommandsParser::SQLITE);
          break;
        }

      default:
        throw NoViableAltException(this);
      }
      setState(370);
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
    setState(378);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__19:
      case TracerCommandsParser::T__20:
      case TracerCommandsParser::T__72:
      case TracerCommandsParser::T__73: {
        enterOuterAlt(_localctx, 1);
        setState(371);
        _la = _input->LA(1);
        if (!(((((_la - 20) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 20)) & 27021597764222979) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(372);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__74: {
        enterOuterAlt(_localctx, 2);
        setState(373);
        match(TracerCommandsParser::T__74);
        setState(374);
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
        setState(375);
        _la = _input->LA(1);
        if (!(((((_la - 56) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 56)) & 25165825) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(376);
        value();
        break;
      }

      case TracerCommandsParser::T__80: {
        enterOuterAlt(_localctx, 4);
        setState(377);
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
    setState(380);
    match(TracerCommandsParser::T__81);
    setState(409);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__54: {
        setState(381);
        match(TracerCommandsParser::T__54);
        setState(382);
        value();
        break;
      }

      case TracerCommandsParser::T__82: {
        setState(383);
        match(TracerCommandsParser::T__82);
        break;
      }

      case TracerCommandsParser::T__83:
      case TracerCommandsParser::T__84: {
        setState(384);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__83

        || _la == TracerCommandsParser::T__84)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(385);
        value();
        setState(390);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__40

        || _la == TracerCommandsParser::T__85) {
          setState(386);
          _la = _input->LA(1);
          if (!(_la == TracerCommandsParser::T__40

          || _la == TracerCommandsParser::T__85)) {
          _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(387);
          value();
          setState(392);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__86: {
        setState(393);
        match(TracerCommandsParser::T__86);
        setState(407);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__87: {
            setState(394);
            match(TracerCommandsParser::T__87);
            setState(397);
            _errHandler->sync(this);
            _la = _input->LA(1);
            do {
              setState(397);
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
                  setState(395);
                  value();
                  break;
                }

                case TracerCommandsParser::SHELL: {
                  setState(396);
                  match(TracerCommandsParser::SHELL);
                  break;
                }

              default:
                throw NoViableAltException(this);
              }
              setState(399);
              _errHandler->sync(this);
              _la = _input->LA(1);
            } while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 8556839548640946812) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 64)) & -1030863687201) != 0) || ((((_la - 128) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 128)) & 1769526525983) != 0));
            break;
          }

          case TracerCommandsParser::T__65: {
            setState(401);
            match(TracerCommandsParser::T__65);
            break;
          }

          case TracerCommandsParser::T__88: {
            setState(402);
            match(TracerCommandsParser::T__88);
            setState(403);
            value();
            setState(404);
            match(TracerCommandsParser::T__89);
            setState(405);
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
    setState(411);
    match(TracerCommandsParser::T__90);
    setState(412);
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
    setState(414);
    match(TracerCommandsParser::T__91);
    setState(415);
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
    setState(417);
    match(TracerCommandsParser::T__92);
    setState(418);
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
    setState(420);
    match(TracerCommandsParser::T__93);
    setState(429);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 8556839548640946814) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 64)) & -1030863687201) != 0) || ((((_la - 128) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 128)) & 1700807049247) != 0)) {
      setState(422);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == TracerCommandsParser::T__0) {
        setState(421);
        match(TracerCommandsParser::T__0);
      }
      setState(425);
      _errHandler->sync(this);
      _la = _input->LA(1);
      do {
        setState(424);
        value();
        setState(427);
        _errHandler->sync(this);
        _la = _input->LA(1);
      } while ((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & 8556839548640946812) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 64)) & -1030863687201) != 0) || ((((_la - 128) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 128)) & 1700807049247) != 0));
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
    setState(431);
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
    setState(433);
    match(TracerCommandsParser::T__96);
    setState(449);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__97: {
        setState(434);
        match(TracerCommandsParser::T__97);
        setState(436);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(435);
          bulkTraceOption();
          setState(438);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while (_la == TracerCommandsParser::T__23

        || _la == TracerCommandsParser::T__37 || ((((_la - 100) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 100)) & 15) != 0));
        break;
      }

      case TracerCommandsParser::T__98: {
        setState(440);
        match(TracerCommandsParser::T__98);
        setState(441);
        match(TracerCommandsParser::T__99);
        setState(442);
        value();
        setState(445);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__33) {
          setState(443);
          match(TracerCommandsParser::T__33);
          setState(444);
          value();
        }
        break;
      }

      case TracerCommandsParser::T__35: {
        setState(447);
        match(TracerCommandsParser::T__35);
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
    setState(451);
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
    setState(452);
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
    setState(458);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::STRING: {
        enterOuterAlt(_localctx, 1);
        setState(454);
        match(TracerCommandsParser::STRING);
        break;
      }

      case TracerCommandsParser::WORD: {
        enterOuterAlt(_localctx, 2);
        setState(455);
        match(TracerCommandsParser::WORD);
        break;
      }

      case TracerCommandsParser::UINT: {
        enterOuterAlt(_localctx, 3);
        setState(456);
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
        setState(457);
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
    setState(460);
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 8556839548640946812) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 64)) & -1030863687201) != 0) || ((((_la - 128) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 128)) & 549755813919) != 0))) {
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
    setState(462);
    match(TracerCommandsParser::T__103);
    setState(537);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__104: {
        setState(463);
        match(TracerCommandsParser::T__104);
        break;
      }

      case TracerCommandsParser::T__105: {
        setState(464);
        match(TracerCommandsParser::T__105);
        setState(465);
        value();
        setState(466);
        match(TracerCommandsParser::T__26);
        setState(467);
        value();
        setState(472);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__20: {
            setState(468);
            match(TracerCommandsParser::T__20);
            setState(469);
            match(TracerCommandsParser::UINT);
            break;
          }

          case TracerCommandsParser::T__85: {
            setState(470);
            match(TracerCommandsParser::T__85);
            setState(471);
            value();
            break;
          }

        default:
          throw NoViableAltException(this);
        }
        break;
      }

      case TracerCommandsParser::T__106: {
        setState(474);
        match(TracerCommandsParser::T__106);
        setState(475);
        value();
        setState(476);
        match(TracerCommandsParser::T__26);
        setState(477);
        value();
        setState(478);
        match(TracerCommandsParser::T__20);
        setState(479);
        match(TracerCommandsParser::UINT);
        setState(480);
        match(TracerCommandsParser::T__132);
        setState(481);
        value();
        setState(484);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__27) {
          setState(482);
          match(TracerCommandsParser::T__27);
          setState(483);
          match(TracerCommandsParser::UINT);
        }
        break;
      }

      case TracerCommandsParser::T__107: {
        setState(486);
        match(TracerCommandsParser::T__107);
        setState(487);
        match(TracerCommandsParser::T__23);
        setState(488);
        value();
        setState(489);
        match(TracerCommandsParser::T__133);
        setState(490);
        value();
        setState(491);
        match(TracerCommandsParser::T__26);
        setState(492);
        value();
        setState(493);
        match(TracerCommandsParser::T__134);
        setState(494);
        value();
        setState(495);
        match(TracerCommandsParser::T__135);
        setState(496);
        value();
        break;
      }

      case TracerCommandsParser::T__127: {
        setState(498);
        match(TracerCommandsParser::T__127);
        setState(499);
        value();
        setState(500);
        match(TracerCommandsParser::T__26);
        setState(501);
        value();
        setState(502);
        match(TracerCommandsParser::T__134);
        setState(503);
        value();
        setState(504);
        match(TracerCommandsParser::T__135);
        setState(505);
        value();
        break;
      }

      case TracerCommandsParser::T__108:
      case TracerCommandsParser::T__125:
      case TracerCommandsParser::T__126: {
        setState(507);
        _la = _input->LA(1);
        if (!(((((_la - 109) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 109)) & 393217) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(508);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__110: {
        setState(509);
        match(TracerCommandsParser::T__110);
        setState(510);
        match(TracerCommandsParser::T__136);
        setState(511);
        match(TracerCommandsParser::UINT);
        setState(512);
        match(TracerCommandsParser::T__25);
        setState(513);
        value();
        setState(514);
        match(TracerCommandsParser::T__137);
        setState(515);
        match(TracerCommandsParser::UINT);
        setState(516);
        match(TracerCommandsParser::T__26);
        setState(517);
        value();
        setState(520);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__19) {
          setState(518);
          match(TracerCommandsParser::T__19);
          setState(519);
          match(TracerCommandsParser::UINT);
        }
        break;
      }

      case TracerCommandsParser::T__111: {
        setState(522);
        match(TracerCommandsParser::T__111);
        break;
      }

      case TracerCommandsParser::T__109: {
        setState(523);
        match(TracerCommandsParser::T__109);
        setState(524);
        match(TracerCommandsParser::T__23);
        setState(525);
        value();
        setState(526);
        match(TracerCommandsParser::T__138);
        setState(527);
        value();
        setState(528);
        match(TracerCommandsParser::T__139);
        setState(529);
        value();
        setState(530);
        match(TracerCommandsParser::T__25);
        setState(531);
        value();
        setState(532);
        match(TracerCommandsParser::T__26);
        setState(533);
        value();
        setState(534);
        match(TracerCommandsParser::T__140);
        setState(535);
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
    setState(539);
    match(TracerCommandsParser::T__29);
    setState(540);
    match(TracerCommandsParser::T__112);
    setState(557);
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
        setState(541);
        value();
        setState(542);
        match(TracerCommandsParser::T__26);
        setState(543);
        value();
        setState(546);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__23) {
          setState(544);
          match(TracerCommandsParser::T__23);
          setState(545);
          value();
        }
        break;
      }

      case TracerCommandsParser::T__23: {
        setState(548);
        match(TracerCommandsParser::T__23);
        setState(549);
        value();
        setState(550);
        match(TracerCommandsParser::T__133);
        setState(551);
        value();
        setState(552);
        match(TracerCommandsParser::T__140);
        setState(553);
        value();
        setState(554);
        match(TracerCommandsParser::T__26);
        setState(555);
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
    setState(559);
    match(TracerCommandsParser::T__29);
    setState(560);
    match(TracerCommandsParser::T__113);
    setState(561);
    match(TracerCommandsParser::UINT);
    setState(562);
    match(TracerCommandsParser::T__33);
    setState(563);
    value();
    setState(572);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::T__18) {
      setState(564);
      match(TracerCommandsParser::T__18);
      setState(569);
      _errHandler->sync(this);
      _la = _input->LA(1);
      while (_la == TracerCommandsParser::T__19

      || _la == TracerCommandsParser::T__20) {
        setState(565);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__19

        || _la == TracerCommandsParser::T__20)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(566);
        match(TracerCommandsParser::UINT);
        setState(571);
        _errHandler->sync(this);
        _la = _input->LA(1);
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
    setState(586);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__114: {
        enterOuterAlt(_localctx, 1);
        setState(574);
        match(TracerCommandsParser::T__114);
        setState(575);
        value();
        setState(579);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__19 || ((((_la - 142) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 142)) & 511) != 0)) {
          setState(576);
          launchOption();
          setState(581);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__115:
      case TracerCommandsParser::T__116: {
        enterOuterAlt(_localctx, 2);
        setState(582);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__115

        || _la == TracerCommandsParser::T__116)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(584);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::UINT) {
          setState(583);
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
    setState(594);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__141:
      case TracerCommandsParser::T__142:
      case TracerCommandsParser::T__143:
      case TracerCommandsParser::T__144:
      case TracerCommandsParser::T__145:
      case TracerCommandsParser::T__146:
      case TracerCommandsParser::T__147: {
        enterOuterAlt(_localctx, 1);
        setState(588);
        _la = _input->LA(1);
        if (!(((((_la - 142) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 142)) & 127) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(589);
        value();
        break;
      }

      case TracerCommandsParser::T__19: {
        enterOuterAlt(_localctx, 2);
        setState(590);
        match(TracerCommandsParser::T__19);
        setState(591);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__148: {
        enterOuterAlt(_localctx, 3);
        setState(592);
        match(TracerCommandsParser::T__148);
        break;
      }

      case TracerCommandsParser::T__149: {
        enterOuterAlt(_localctx, 4);
        setState(593);
        match(TracerCommandsParser::T__149);
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
    setState(596);
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
    setState(614);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__117: {
        enterOuterAlt(_localctx, 1);
        setState(598);
        match(TracerCommandsParser::T__117);
        setState(599);
        value();
        setState(606);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__19 || _la == TracerCommandsParser::T__141) {
          setState(604);
          _errHandler->sync(this);
          switch (_input->LA(1)) {
            case TracerCommandsParser::T__141: {
              setState(600);
              match(TracerCommandsParser::T__141);
              setState(601);
              value();
              break;
            }

            case TracerCommandsParser::T__19: {
              setState(602);
              match(TracerCommandsParser::T__19);
              setState(603);
              match(TracerCommandsParser::UINT);
              break;
            }

          default:
            throw NoViableAltException(this);
          }
          setState(608);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__118: {
        enterOuterAlt(_localctx, 2);
        setState(609);
        match(TracerCommandsParser::T__118);
        setState(612);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__19) {
          setState(610);
          match(TracerCommandsParser::T__19);
          setState(611);
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
    setState(620);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__119: {
        setState(616);
        match(TracerCommandsParser::T__119);
        break;
      }

      case TracerCommandsParser::T__130: {
        setState(617);
        match(TracerCommandsParser::T__130);
        break;
      }

      case TracerCommandsParser::T__120: {
        setState(618);
        match(TracerCommandsParser::T__120);
        setState(619);
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
    setState(626);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == TracerCommandsParser::T__19

    || _la == TracerCommandsParser::T__20) {
      setState(622);
      _la = _input->LA(1);
      if (!(_la == TracerCommandsParser::T__19

      || _la == TracerCommandsParser::T__20)) {
      _errHandler->recoverInline(this);
      }
      else {
        _errHandler->reportMatch(this);
        consume();
      }
      setState(623);
      match(TracerCommandsParser::UINT);
      setState(628);
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
    setState(629);
    match(TracerCommandsParser::T__124);
    setState(630);
    match(TracerCommandsParser::T__54);
    setState(631);
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
    setState(633);
    match(TracerCommandsParser::T__122);
    setState(648);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__2: {
        setState(634);
        match(TracerCommandsParser::T__2);
        setState(638);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 204472320) != 0) || ((((_la - 137) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 137)) & 507907) != 0)) {
          setState(635);
          instructionOption();
          setState(640);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__5: {
        setState(641);
        match(TracerCommandsParser::T__5);
        setState(644);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__102) {
          setState(642);
          match(TracerCommandsParser::T__102);
          setState(643);
          value();
        }
        break;
      }

      case TracerCommandsParser::T__128: {
        setState(646);
        match(TracerCommandsParser::T__128);
        setState(647);
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
    setState(658);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__19:
      case TracerCommandsParser::T__20:
      case TracerCommandsParser::T__136:
      case TracerCommandsParser::T__137: {
        enterOuterAlt(_localctx, 1);
        setState(650);
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
        setState(651);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__150: {
        enterOuterAlt(_localctx, 2);
        setState(652);
        match(TracerCommandsParser::T__150);
        setState(653);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__151: {
        enterOuterAlt(_localctx, 3);
        setState(654);
        match(TracerCommandsParser::T__151);
        break;
      }

      case TracerCommandsParser::T__152: {
        enterOuterAlt(_localctx, 4);
        setState(655);
        match(TracerCommandsParser::T__152);
        break;
      }

      case TracerCommandsParser::T__25:
      case TracerCommandsParser::T__26:
      case TracerCommandsParser::T__153:
      case TracerCommandsParser::T__154: {
        enterOuterAlt(_localctx, 5);
        setState(656);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__25

        || _la == TracerCommandsParser::T__26 || _la == TracerCommandsParser::T__153

        || _la == TracerCommandsParser::T__154)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(657);
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
    setState(660);
    match(TracerCommandsParser::T__123);
    setState(667);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 204472320) != 0) || ((((_la - 137) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 137)) & 524291) != 0)) {
      setState(665);
      _errHandler->sync(this);
      switch (_input->LA(1)) {
        case TracerCommandsParser::T__19:
        case TracerCommandsParser::T__20:
        case TracerCommandsParser::T__136:
        case TracerCommandsParser::T__137: {
          setState(661);
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
          setState(662);
          match(TracerCommandsParser::UINT);
          break;
        }

        case TracerCommandsParser::T__25:
        case TracerCommandsParser::T__26:
        case TracerCommandsParser::T__155: {
          setState(663);
          _la = _input->LA(1);
          if (!(_la == TracerCommandsParser::T__25

          || _la == TracerCommandsParser::T__26 || _la == TracerCommandsParser::T__155)) {
          _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(664);
          value();
          break;
        }

      default:
        throw NoViableAltException(this);
      }
      setState(669);
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
    setState(670);
    match(TracerCommandsParser::T__156);
    setState(671);
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
    setState(673);
    _la = _input->LA(1);
    if (!(_la == TracerCommandsParser::T__157

    || _la == TracerCommandsParser::T__158)) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(674);
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
    setState(685);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__159: {
        enterOuterAlt(_localctx, 1);
        setState(676);
        match(TracerCommandsParser::T__159);
        setState(677);
        value();
        break;
      }

      case TracerCommandsParser::T__160: {
        enterOuterAlt(_localctx, 2);
        setState(678);
        match(TracerCommandsParser::T__160);
        setState(681);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(681);
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
              setState(679);
              value();
              break;
            }

            case TracerCommandsParser::SHELL: {
              setState(680);
              match(TracerCommandsParser::SHELL);
              break;
            }

          default:
            throw NoViableAltException(this);
          }
          setState(683);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 8556839548640946812) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 64)) & -1030863687201) != 0) || ((((_la - 128) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 128)) & 1769526525983) != 0));
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

void TracerCommandsParser::initialize() {
#if ANTLR4_USE_THREAD_LOCAL_CACHE
  tracercommandsParserInitialize();
#else
  ::antlr4::internal::call_once(tracercommandsParserOnceFlag, tracercommandsParserInitialize);
#endif
}
