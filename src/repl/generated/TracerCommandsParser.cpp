
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
      "'--cwd'", "'--signature'", "'--member'"
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
      "SQLITE", "STACK", "CWD", "SIGNATURE", "MEMBER", "WORD", "WS"
    }
  );
  static const int32_t serializedATNSegment[] = {
    4,1,171,713,2,0,7,0,2,1,7,1,2,2,7,2,2,3,7,3,2,4,7,4,2,5,7,5,2,6,7,6,2,
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
    8,14,3,14,262,8,14,1,15,1,15,1,15,1,15,1,15,1,15,1,15,1,15,3,15,272,8,
    15,1,16,1,16,1,16,3,16,277,8,16,1,17,1,17,1,17,1,17,1,17,1,17,1,17,3,
    17,286,8,17,1,18,1,18,1,18,1,18,1,18,1,18,3,18,294,8,18,1,19,1,19,1,19,
    1,19,1,19,1,19,1,19,1,19,1,19,1,19,1,19,1,19,1,19,1,19,1,19,5,19,311,
    8,19,10,19,12,19,314,9,19,1,19,1,19,1,19,1,19,1,19,5,19,321,8,19,10,19,
    12,19,324,9,19,3,19,326,8,19,1,20,1,20,5,20,330,8,20,10,20,12,20,333,
    9,20,1,20,1,20,1,20,1,20,3,20,339,8,20,1,20,1,20,1,20,1,20,1,20,1,20,
    1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,
    1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,3,20,369,8,20,1,21,1,21,5,21,
    373,8,21,10,21,12,21,376,9,21,1,22,1,22,1,22,1,22,5,22,382,8,22,10,22,
    12,22,385,9,22,1,23,1,23,1,23,1,23,1,23,1,23,1,23,3,23,394,8,23,1,24,
    1,24,1,24,1,24,1,24,1,24,1,24,1,24,5,24,404,8,24,10,24,12,24,407,9,24,
    1,24,1,24,1,24,1,24,4,24,413,8,24,11,24,12,24,414,1,24,1,24,1,24,1,24,
    1,24,1,24,3,24,423,8,24,3,24,425,8,24,1,25,1,25,1,25,1,26,1,26,1,26,1,
    27,1,27,1,27,1,28,1,28,3,28,438,8,28,1,28,4,28,441,8,28,11,28,12,28,442,
    3,28,445,8,28,1,29,1,29,1,30,1,30,1,30,4,30,452,8,30,11,30,12,30,453,
    1,30,1,30,1,30,1,30,1,30,3,30,461,8,30,1,30,1,30,3,30,465,8,30,1,31,1,
    31,1,31,1,32,1,32,1,32,1,32,3,32,474,8,32,1,33,1,33,1,34,1,34,1,34,1,
    34,1,34,1,34,1,34,1,34,1,34,1,34,3,34,488,8,34,1,34,1,34,1,34,1,34,1,
    34,1,34,1,34,1,34,1,34,1,34,3,34,500,8,34,1,34,1,34,1,34,1,34,1,34,1,
    34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,
    34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,
    34,3,34,536,8,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,
    34,1,34,1,34,1,34,1,34,3,34,553,8,34,1,35,1,35,1,35,1,35,1,35,1,35,1,
    35,3,35,562,8,35,1,35,1,35,1,35,1,35,1,35,1,35,1,35,1,35,1,35,3,35,573,
    8,35,1,35,3,35,576,8,35,1,36,1,36,1,36,1,36,1,36,1,36,1,36,1,36,1,36,
    5,36,587,8,36,10,36,12,36,590,9,36,5,36,592,8,36,10,36,12,36,595,9,36,
    1,37,1,37,1,37,5,37,600,8,37,10,37,12,37,603,9,37,1,37,1,37,3,37,607,
    8,37,3,37,609,8,37,1,38,1,38,1,38,1,38,1,38,1,38,3,38,617,8,38,1,39,1,
    39,1,40,1,40,1,40,1,40,1,40,1,40,5,40,627,8,40,10,40,12,40,630,9,40,1,
    40,1,40,1,40,3,40,635,8,40,3,40,637,8,40,1,41,1,41,1,41,1,41,3,41,643,
    8,41,1,41,1,41,5,41,647,8,41,10,41,12,41,650,9,41,1,42,1,42,1,42,1,42,
    1,43,1,43,1,43,5,43,659,8,43,10,43,12,43,662,9,43,1,43,1,43,1,43,3,43,
    667,8,43,1,43,1,43,3,43,671,8,43,1,44,1,44,1,44,1,44,1,44,1,44,1,44,1,
    44,3,44,681,8,44,1,45,1,45,1,45,1,45,1,45,5,45,688,8,45,10,45,12,45,691,
    9,45,1,46,1,46,1,46,1,47,1,47,1,47,1,48,1,48,1,48,1,48,1,48,4,48,704,
    8,48,11,48,12,48,705,3,48,708,8,48,1,49,1,49,1,49,1,49,0,0,50,0,2,4,6,
    8,10,12,14,16,18,20,22,24,26,28,30,32,34,36,38,40,42,44,46,48,50,52,54,
    56,58,60,62,64,66,68,70,72,74,76,78,80,82,84,86,88,90,92,94,96,98,0,22,
    1,0,7,8,1,0,20,21,2,0,12,12,24,27,2,0,24,24,38,41,3,0,24,24,38,40,42,
    44,2,0,161,161,170,170,2,0,20,21,73,74,1,0,76,78,2,0,56,56,79,80,1,0,
    84,85,2,0,41,41,86,86,1,0,95,96,3,0,24,24,38,38,100,103,18,0,2,6,9,9,
    11,11,13,16,22,23,30,33,35,37,54,55,57,58,60,62,64,68,70,72,76,78,82,
    85,87,89,91,99,104,132,166,166,2,0,109,109,126,127,1,0,116,117,2,0,142,
    147,167,167,2,0,122,122,132,132,2,0,20,21,137,138,2,0,26,27,153,154,2,
    0,26,27,155,155,1,0,157,158,823,0,100,1,0,0,0,2,136,1,0,0,0,4,138,1,0,
    0,0,6,155,1,0,0,0,8,157,1,0,0,0,10,165,1,0,0,0,12,170,1,0,0,0,14,172,
    1,0,0,0,16,175,1,0,0,0,18,177,1,0,0,0,20,181,1,0,0,0,22,195,1,0,0,0,24,
    197,1,0,0,0,26,209,1,0,0,0,28,261,1,0,0,0,30,271,1,0,0,0,32,276,1,0,0,
    0,34,285,1,0,0,0,36,293,1,0,0,0,38,295,1,0,0,0,40,327,1,0,0,0,42,370,
    1,0,0,0,44,377,1,0,0,0,46,393,1,0,0,0,48,395,1,0,0,0,50,426,1,0,0,0,52,
    429,1,0,0,0,54,432,1,0,0,0,56,435,1,0,0,0,58,446,1,0,0,0,60,448,1,0,0,
    0,62,466,1,0,0,0,64,473,1,0,0,0,66,475,1,0,0,0,68,477,1,0,0,0,70,554,
    1,0,0,0,72,577,1,0,0,0,74,608,1,0,0,0,76,616,1,0,0,0,78,618,1,0,0,0,80,
    636,1,0,0,0,82,642,1,0,0,0,84,651,1,0,0,0,86,655,1,0,0,0,88,680,1,0,0,
    0,90,682,1,0,0,0,92,692,1,0,0,0,94,695,1,0,0,0,96,707,1,0,0,0,98,709,
    1,0,0,0,100,101,5,1,0,0,101,102,3,2,1,0,102,103,5,0,0,1,103,1,1,0,0,0,
    104,137,3,4,2,0,105,137,3,8,4,0,106,137,3,10,5,0,107,137,3,12,6,0,108,
    137,3,14,7,0,109,137,3,16,8,0,110,137,3,18,9,0,111,137,3,20,10,0,112,
    137,3,22,11,0,113,137,3,24,12,0,114,137,3,28,14,0,115,137,3,38,19,0,116,
    137,3,40,20,0,117,137,3,42,21,0,118,137,3,44,22,0,119,137,3,48,24,0,120,
    137,3,50,25,0,121,137,3,52,26,0,122,137,3,54,27,0,123,137,3,56,28,0,124,
    137,3,58,29,0,125,137,3,60,30,0,126,137,3,68,34,0,127,137,3,70,35,0,128,
    137,3,72,36,0,129,137,3,74,37,0,130,137,3,80,40,0,131,137,3,82,41,0,132,
    137,3,86,43,0,133,137,3,90,45,0,134,137,3,84,42,0,135,137,3,78,39,0,136,
    104,1,0,0,0,136,105,1,0,0,0,136,106,1,0,0,0,136,107,1,0,0,0,136,108,1,
    0,0,0,136,109,1,0,0,0,136,110,1,0,0,0,136,111,1,0,0,0,136,112,1,0,0,0,
    136,113,1,0,0,0,136,114,1,0,0,0,136,115,1,0,0,0,136,116,1,0,0,0,136,117,
    1,0,0,0,136,118,1,0,0,0,136,119,1,0,0,0,136,120,1,0,0,0,136,121,1,0,0,
    0,136,122,1,0,0,0,136,123,1,0,0,0,136,124,1,0,0,0,136,125,1,0,0,0,136,
    126,1,0,0,0,136,127,1,0,0,0,136,128,1,0,0,0,136,129,1,0,0,0,136,130,1,
    0,0,0,136,131,1,0,0,0,136,132,1,0,0,0,136,133,1,0,0,0,136,134,1,0,0,0,
    136,135,1,0,0,0,137,3,1,0,0,0,138,150,5,2,0,0,139,143,5,3,0,0,140,142,
    3,6,3,0,141,140,1,0,0,0,142,145,1,0,0,0,143,141,1,0,0,0,143,144,1,0,0,
    0,144,151,1,0,0,0,145,143,1,0,0,0,146,147,5,4,0,0,147,151,3,64,32,0,148,
    151,5,5,0,0,149,151,5,6,0,0,150,139,1,0,0,0,150,146,1,0,0,0,150,148,1,
    0,0,0,150,149,1,0,0,0,151,5,1,0,0,0,152,153,7,0,0,0,153,156,3,64,32,0,
    154,156,3,94,47,0,155,152,1,0,0,0,155,154,1,0,0,0,156,7,1,0,0,0,157,163,
    5,9,0,0,158,161,5,161,0,0,159,160,5,10,0,0,160,162,5,161,0,0,161,159,
    1,0,0,0,161,162,1,0,0,0,162,164,1,0,0,0,163,158,1,0,0,0,163,164,1,0,0,
    0,164,9,1,0,0,0,165,168,5,11,0,0,166,167,5,12,0,0,167,169,3,64,32,0,168,
    166,1,0,0,0,168,169,1,0,0,0,169,11,1,0,0,0,170,171,5,13,0,0,171,13,1,
    0,0,0,172,173,5,14,0,0,173,174,3,64,32,0,174,15,1,0,0,0,175,176,5,5,0,
    0,176,17,1,0,0,0,177,179,5,15,0,0,178,180,3,64,32,0,179,178,1,0,0,0,179,
    180,1,0,0,0,180,19,1,0,0,0,181,193,5,16,0,0,182,194,5,17,0,0,183,184,
    5,18,0,0,184,194,3,64,32,0,185,190,5,19,0,0,186,187,7,1,0,0,187,189,5,
    161,0,0,188,186,1,0,0,0,189,192,1,0,0,0,190,188,1,0,0,0,190,191,1,0,0,
    0,191,194,1,0,0,0,192,190,1,0,0,0,193,182,1,0,0,0,193,183,1,0,0,0,193,
    185,1,0,0,0,193,194,1,0,0,0,194,21,1,0,0,0,195,196,5,22,0,0,196,23,1,
    0,0,0,197,201,5,23,0,0,198,200,3,26,13,0,199,198,1,0,0,0,200,203,1,0,
    0,0,201,199,1,0,0,0,201,202,1,0,0,0,202,25,1,0,0,0,203,201,1,0,0,0,204,
    205,7,2,0,0,205,210,3,64,32,0,206,207,5,28,0,0,207,210,5,161,0,0,208,
    210,5,29,0,0,209,204,1,0,0,0,209,206,1,0,0,0,209,208,1,0,0,0,210,27,1,
    0,0,0,211,255,5,30,0,0,212,213,5,31,0,0,213,215,3,64,32,0,214,216,3,30,
    15,0,215,214,1,0,0,0,216,217,1,0,0,0,217,215,1,0,0,0,217,218,1,0,0,0,
    218,256,1,0,0,0,219,220,5,32,0,0,220,225,3,64,32,0,221,224,3,32,16,0,
    222,224,3,98,49,0,223,221,1,0,0,0,223,222,1,0,0,0,224,227,1,0,0,0,225,
    223,1,0,0,0,225,226,1,0,0,0,226,256,1,0,0,0,227,225,1,0,0,0,228,229,5,
    33,0,0,229,230,3,64,32,0,230,231,5,34,0,0,231,236,3,64,32,0,232,235,3,
    36,18,0,233,235,3,98,49,0,234,232,1,0,0,0,234,233,1,0,0,0,235,238,1,0,
    0,0,236,234,1,0,0,0,236,237,1,0,0,0,237,256,1,0,0,0,238,236,1,0,0,0,239,
    240,5,35,0,0,240,241,3,64,32,0,241,242,5,34,0,0,242,247,3,64,32,0,243,
    246,3,34,17,0,244,246,3,98,49,0,245,243,1,0,0,0,245,244,1,0,0,0,246,249,
    1,0,0,0,247,245,1,0,0,0,247,248,1,0,0,0,248,256,1,0,0,0,249,247,1,0,0,
    0,250,251,5,36,0,0,251,253,5,161,0,0,252,254,3,98,49,0,253,252,1,0,0,
    0,253,254,1,0,0,0,254,256,1,0,0,0,255,212,1,0,0,0,255,219,1,0,0,0,255,
    228,1,0,0,0,255,239,1,0,0,0,255,250,1,0,0,0,256,262,1,0,0,0,257,259,5,
    37,0,0,258,260,3,98,49,0,259,258,1,0,0,0,259,260,1,0,0,0,260,262,1,0,
    0,0,261,211,1,0,0,0,261,257,1,0,0,0,262,29,1,0,0,0,263,264,7,3,0,0,264,
    272,3,64,32,0,265,266,5,168,0,0,266,272,3,64,32,0,267,272,5,169,0,0,268,
    272,5,164,0,0,269,272,3,96,48,0,270,272,3,98,49,0,271,263,1,0,0,0,271,
    265,1,0,0,0,271,267,1,0,0,0,271,268,1,0,0,0,271,269,1,0,0,0,271,270,1,
    0,0,0,272,31,1,0,0,0,273,274,7,4,0,0,274,277,3,64,32,0,275,277,3,96,48,
    0,276,273,1,0,0,0,276,275,1,0,0,0,277,33,1,0,0,0,278,286,3,32,16,0,279,
    286,5,45,0,0,280,286,5,46,0,0,281,286,5,47,0,0,282,286,5,48,0,0,283,286,
    5,49,0,0,284,286,5,50,0,0,285,278,1,0,0,0,285,279,1,0,0,0,285,280,1,0,
    0,0,285,281,1,0,0,0,285,282,1,0,0,0,285,283,1,0,0,0,285,284,1,0,0,0,286,
    35,1,0,0,0,287,294,3,34,17,0,288,289,5,51,0,0,289,294,7,5,0,0,290,294,
    5,52,0,0,291,294,5,53,0,0,292,294,3,92,46,0,293,287,1,0,0,0,293,288,1,
    0,0,0,293,290,1,0,0,0,293,291,1,0,0,0,293,292,1,0,0,0,294,37,1,0,0,0,
    295,325,5,54,0,0,296,297,5,55,0,0,297,298,3,64,32,0,298,299,5,56,0,0,
    299,300,3,64,32,0,300,326,1,0,0,0,301,302,5,57,0,0,302,326,3,64,32,0,
    303,304,5,58,0,0,304,326,3,64,32,0,305,306,5,35,0,0,306,307,3,64,32,0,
    307,308,5,59,0,0,308,312,3,64,32,0,309,311,3,34,17,0,310,309,1,0,0,0,
    311,314,1,0,0,0,312,310,1,0,0,0,312,313,1,0,0,0,313,326,1,0,0,0,314,312,
    1,0,0,0,315,316,5,60,0,0,316,317,3,64,32,0,317,318,5,59,0,0,318,322,3,
    64,32,0,319,321,3,36,18,0,320,319,1,0,0,0,321,324,1,0,0,0,322,320,1,0,
    0,0,322,323,1,0,0,0,323,326,1,0,0,0,324,322,1,0,0,0,325,296,1,0,0,0,325,
    301,1,0,0,0,325,303,1,0,0,0,325,305,1,0,0,0,325,315,1,0,0,0,326,39,1,
    0,0,0,327,368,5,61,0,0,328,330,3,46,23,0,329,328,1,0,0,0,330,333,1,0,
    0,0,331,329,1,0,0,0,331,332,1,0,0,0,332,369,1,0,0,0,333,331,1,0,0,0,334,
    335,5,62,0,0,335,338,3,64,32,0,336,337,5,28,0,0,337,339,5,161,0,0,338,
    336,1,0,0,0,338,339,1,0,0,0,339,369,1,0,0,0,340,341,5,58,0,0,341,342,
    3,64,32,0,342,343,5,63,0,0,343,344,5,161,0,0,344,369,1,0,0,0,345,346,
    5,166,0,0,346,347,3,64,32,0,347,348,5,63,0,0,348,349,5,161,0,0,349,369,
    1,0,0,0,350,351,5,64,0,0,351,369,3,64,32,0,352,353,5,65,0,0,353,369,3,
    64,32,0,354,355,5,66,0,0,355,369,3,64,32,0,356,357,5,67,0,0,357,358,3,
    64,32,0,358,359,5,56,0,0,359,360,3,64,32,0,360,369,1,0,0,0,361,362,5,
    68,0,0,362,363,3,64,32,0,363,364,5,69,0,0,364,365,5,70,0,0,365,366,5,
    41,0,0,366,367,3,64,32,0,367,369,1,0,0,0,368,331,1,0,0,0,368,334,1,0,
    0,0,368,340,1,0,0,0,368,345,1,0,0,0,368,350,1,0,0,0,368,352,1,0,0,0,368,
    354,1,0,0,0,368,356,1,0,0,0,368,361,1,0,0,0,369,41,1,0,0,0,370,374,5,
    71,0,0,371,373,3,46,23,0,372,371,1,0,0,0,373,376,1,0,0,0,374,372,1,0,
    0,0,374,375,1,0,0,0,375,43,1,0,0,0,376,374,1,0,0,0,377,378,5,72,0,0,378,
    383,3,64,32,0,379,382,3,46,23,0,380,382,5,165,0,0,381,379,1,0,0,0,381,
    380,1,0,0,0,382,385,1,0,0,0,383,381,1,0,0,0,383,384,1,0,0,0,384,45,1,
    0,0,0,385,383,1,0,0,0,386,387,7,6,0,0,387,394,5,161,0,0,388,389,5,75,
    0,0,389,394,7,7,0,0,390,391,7,8,0,0,391,394,3,64,32,0,392,394,5,81,0,
    0,393,386,1,0,0,0,393,388,1,0,0,0,393,390,1,0,0,0,393,392,1,0,0,0,394,
    47,1,0,0,0,395,424,5,82,0,0,396,397,5,55,0,0,397,425,3,64,32,0,398,425,
    5,83,0,0,399,400,7,9,0,0,400,405,3,64,32,0,401,402,7,10,0,0,402,404,3,
    64,32,0,403,401,1,0,0,0,404,407,1,0,0,0,405,403,1,0,0,0,405,406,1,0,0,
    0,406,425,1,0,0,0,407,405,1,0,0,0,408,422,5,87,0,0,409,412,5,88,0,0,410,
    413,3,64,32,0,411,413,5,163,0,0,412,410,1,0,0,0,412,411,1,0,0,0,413,414,
    1,0,0,0,414,412,1,0,0,0,414,415,1,0,0,0,415,423,1,0,0,0,416,423,5,66,
    0,0,417,418,5,89,0,0,418,419,3,64,32,0,419,420,5,90,0,0,420,421,3,64,
    32,0,421,423,1,0,0,0,422,409,1,0,0,0,422,416,1,0,0,0,422,417,1,0,0,0,
    422,423,1,0,0,0,423,425,1,0,0,0,424,396,1,0,0,0,424,398,1,0,0,0,424,399,
    1,0,0,0,424,408,1,0,0,0,425,49,1,0,0,0,426,427,5,91,0,0,427,428,3,64,
    32,0,428,51,1,0,0,0,429,430,5,92,0,0,430,431,3,64,32,0,431,53,1,0,0,0,
    432,433,5,93,0,0,433,434,3,64,32,0,434,55,1,0,0,0,435,444,5,94,0,0,436,
    438,5,1,0,0,437,436,1,0,0,0,437,438,1,0,0,0,438,440,1,0,0,0,439,441,3,
    64,32,0,440,439,1,0,0,0,441,442,1,0,0,0,442,440,1,0,0,0,442,443,1,0,0,
    0,443,445,1,0,0,0,444,437,1,0,0,0,444,445,1,0,0,0,445,57,1,0,0,0,446,
    447,7,11,0,0,447,59,1,0,0,0,448,464,5,97,0,0,449,451,5,98,0,0,450,452,
    3,62,31,0,451,450,1,0,0,0,452,453,1,0,0,0,453,451,1,0,0,0,453,454,1,0,
    0,0,454,465,1,0,0,0,455,456,5,99,0,0,456,457,5,100,0,0,457,460,3,64,32,
    0,458,459,5,34,0,0,459,461,3,64,32,0,460,458,1,0,0,0,460,461,1,0,0,0,
    461,465,1,0,0,0,462,463,5,36,0,0,463,465,3,64,32,0,464,449,1,0,0,0,464,
    455,1,0,0,0,464,462,1,0,0,0,465,61,1,0,0,0,466,467,7,12,0,0,467,468,3,
    64,32,0,468,63,1,0,0,0,469,474,5,162,0,0,470,474,5,170,0,0,471,474,5,
    161,0,0,472,474,3,66,33,0,473,469,1,0,0,0,473,470,1,0,0,0,473,471,1,0,
    0,0,473,472,1,0,0,0,474,65,1,0,0,0,475,476,7,13,0,0,476,67,1,0,0,0,477,
    552,5,104,0,0,478,553,5,105,0,0,479,480,5,106,0,0,480,481,3,64,32,0,481,
    482,5,27,0,0,482,487,3,64,32,0,483,484,5,21,0,0,484,488,5,161,0,0,485,
    486,5,86,0,0,486,488,3,64,32,0,487,483,1,0,0,0,487,485,1,0,0,0,488,553,
    1,0,0,0,489,490,5,107,0,0,490,491,3,64,32,0,491,492,5,27,0,0,492,493,
    3,64,32,0,493,494,5,21,0,0,494,495,5,161,0,0,495,496,5,133,0,0,496,499,
    3,64,32,0,497,498,5,28,0,0,498,500,5,161,0,0,499,497,1,0,0,0,499,500,
    1,0,0,0,500,553,1,0,0,0,501,502,5,108,0,0,502,503,5,24,0,0,503,504,3,
    64,32,0,504,505,5,134,0,0,505,506,3,64,32,0,506,507,5,27,0,0,507,508,
    3,64,32,0,508,509,5,135,0,0,509,510,3,64,32,0,510,511,5,136,0,0,511,512,
    3,64,32,0,512,553,1,0,0,0,513,514,5,128,0,0,514,515,3,64,32,0,515,516,
    5,27,0,0,516,517,3,64,32,0,517,518,5,135,0,0,518,519,3,64,32,0,519,520,
    5,136,0,0,520,521,3,64,32,0,521,553,1,0,0,0,522,523,7,14,0,0,523,553,
    5,161,0,0,524,525,5,111,0,0,525,526,5,137,0,0,526,527,5,161,0,0,527,528,
    5,26,0,0,528,529,3,64,32,0,529,530,5,138,0,0,530,531,5,161,0,0,531,532,
    5,27,0,0,532,535,3,64,32,0,533,534,5,20,0,0,534,536,5,161,0,0,535,533,
    1,0,0,0,535,536,1,0,0,0,536,553,1,0,0,0,537,553,5,112,0,0,538,539,5,110,
    0,0,539,540,5,24,0,0,540,541,3,64,32,0,541,542,5,139,0,0,542,543,3,64,
    32,0,543,544,5,140,0,0,544,545,3,64,32,0,545,546,5,26,0,0,546,547,3,64,
    32,0,547,548,5,27,0,0,548,549,3,64,32,0,549,550,5,141,0,0,550,551,3,64,
    32,0,551,553,1,0,0,0,552,478,1,0,0,0,552,479,1,0,0,0,552,489,1,0,0,0,
    552,501,1,0,0,0,552,513,1,0,0,0,552,522,1,0,0,0,552,524,1,0,0,0,552,537,
    1,0,0,0,552,538,1,0,0,0,553,69,1,0,0,0,554,555,5,30,0,0,555,572,5,113,
    0,0,556,557,3,64,32,0,557,558,5,27,0,0,558,561,3,64,32,0,559,560,5,24,
    0,0,560,562,3,64,32,0,561,559,1,0,0,0,561,562,1,0,0,0,562,573,1,0,0,0,
    563,564,5,24,0,0,564,565,3,64,32,0,565,566,5,134,0,0,566,567,3,64,32,
    0,567,568,5,141,0,0,568,569,3,64,32,0,569,570,5,27,0,0,570,571,3,64,32,
    0,571,573,1,0,0,0,572,556,1,0,0,0,572,563,1,0,0,0,573,575,1,0,0,0,574,
    576,3,98,49,0,575,574,1,0,0,0,575,576,1,0,0,0,576,71,1,0,0,0,577,578,
    5,30,0,0,578,579,5,114,0,0,579,580,5,161,0,0,580,581,5,34,0,0,581,593,
    3,64,32,0,582,592,3,98,49,0,583,588,5,19,0,0,584,585,7,1,0,0,585,587,
    5,161,0,0,586,584,1,0,0,0,587,590,1,0,0,0,588,586,1,0,0,0,588,589,1,0,
    0,0,589,592,1,0,0,0,590,588,1,0,0,0,591,582,1,0,0,0,591,583,1,0,0,0,592,
    595,1,0,0,0,593,591,1,0,0,0,593,594,1,0,0,0,594,73,1,0,0,0,595,593,1,
    0,0,0,596,597,5,115,0,0,597,601,3,64,32,0,598,600,3,76,38,0,599,598,1,
    0,0,0,600,603,1,0,0,0,601,599,1,0,0,0,601,602,1,0,0,0,602,609,1,0,0,0,
    603,601,1,0,0,0,604,606,7,15,0,0,605,607,5,161,0,0,606,605,1,0,0,0,606,
    607,1,0,0,0,607,609,1,0,0,0,608,596,1,0,0,0,608,604,1,0,0,0,609,75,1,
    0,0,0,610,611,7,16,0,0,611,617,3,64,32,0,612,613,5,20,0,0,613,617,5,161,
    0,0,614,617,5,148,0,0,615,617,5,149,0,0,616,610,1,0,0,0,616,612,1,0,0,
    0,616,614,1,0,0,0,616,615,1,0,0,0,617,77,1,0,0,0,618,619,5,130,0,0,619,
    79,1,0,0,0,620,621,5,118,0,0,621,628,3,64,32,0,622,623,5,142,0,0,623,
    627,3,64,32,0,624,625,5,20,0,0,625,627,5,161,0,0,626,622,1,0,0,0,626,
    624,1,0,0,0,627,630,1,0,0,0,628,626,1,0,0,0,628,629,1,0,0,0,629,637,1,
    0,0,0,630,628,1,0,0,0,631,634,5,119,0,0,632,633,5,20,0,0,633,635,5,161,
    0,0,634,632,1,0,0,0,634,635,1,0,0,0,635,637,1,0,0,0,636,620,1,0,0,0,636,
    631,1,0,0,0,637,81,1,0,0,0,638,643,5,120,0,0,639,643,5,131,0,0,640,641,
    5,121,0,0,641,643,7,17,0,0,642,638,1,0,0,0,642,639,1,0,0,0,642,640,1,
    0,0,0,643,648,1,0,0,0,644,645,7,1,0,0,645,647,5,161,0,0,646,644,1,0,0,
    0,647,650,1,0,0,0,648,646,1,0,0,0,648,649,1,0,0,0,649,83,1,0,0,0,650,
    648,1,0,0,0,651,652,5,125,0,0,652,653,5,55,0,0,653,654,3,64,32,0,654,
    85,1,0,0,0,655,670,5,123,0,0,656,660,5,3,0,0,657,659,3,88,44,0,658,657,
    1,0,0,0,659,662,1,0,0,0,660,658,1,0,0,0,660,661,1,0,0,0,661,671,1,0,0,
    0,662,660,1,0,0,0,663,666,5,6,0,0,664,665,5,103,0,0,665,667,3,64,32,0,
    666,664,1,0,0,0,666,667,1,0,0,0,667,671,1,0,0,0,668,669,5,129,0,0,669,
    671,3,64,32,0,670,656,1,0,0,0,670,663,1,0,0,0,670,668,1,0,0,0,671,87,
    1,0,0,0,672,673,7,18,0,0,673,681,5,161,0,0,674,675,5,150,0,0,675,681,
    5,161,0,0,676,681,5,151,0,0,677,681,5,152,0,0,678,679,7,19,0,0,679,681,
    3,64,32,0,680,672,1,0,0,0,680,674,1,0,0,0,680,676,1,0,0,0,680,677,1,0,
    0,0,680,678,1,0,0,0,681,89,1,0,0,0,682,689,5,124,0,0,683,684,7,18,0,0,
    684,688,5,161,0,0,685,686,7,20,0,0,686,688,3,64,32,0,687,683,1,0,0,0,
    687,685,1,0,0,0,688,691,1,0,0,0,689,687,1,0,0,0,689,690,1,0,0,0,690,91,
    1,0,0,0,691,689,1,0,0,0,692,693,5,156,0,0,693,694,3,64,32,0,694,93,1,
    0,0,0,695,696,7,21,0,0,696,697,5,161,0,0,697,95,1,0,0,0,698,699,5,159,
    0,0,699,708,3,64,32,0,700,703,5,160,0,0,701,704,3,64,32,0,702,704,5,163,
    0,0,703,701,1,0,0,0,703,702,1,0,0,0,704,705,1,0,0,0,705,703,1,0,0,0,705,
    706,1,0,0,0,706,708,1,0,0,0,707,698,1,0,0,0,707,700,1,0,0,0,708,97,1,
    0,0,0,709,710,5,167,0,0,710,711,3,64,32,0,711,99,1,0,0,0,78,136,143,150,
    155,161,163,168,179,190,193,201,209,217,223,225,234,236,245,247,253,255,
    259,261,271,276,285,293,312,322,325,331,338,368,374,381,383,393,405,412,
    414,422,424,437,442,444,453,460,464,473,487,499,535,552,561,572,575,588,
    591,593,601,606,608,616,626,628,634,636,642,648,660,666,670,680,687,689,
    703,705,707
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
      ((1ULL << (_la - 128)) & 4698694221855) != 0)) {
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
              ((1ULL << (_la - 159)) & 1827) != 0));
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

tree::TerminalNode* TracerCommandsParser::PrepareOptionContext::SIGNATURE() {
  return getToken(TracerCommandsParser::SIGNATURE, 0);
}

tree::TerminalNode* TracerCommandsParser::PrepareOptionContext::MEMBER() {
  return getToken(TracerCommandsParser::MEMBER, 0);
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
    setState(271);
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

      case TracerCommandsParser::SIGNATURE: {
        enterOuterAlt(_localctx, 2);
        setState(265);
        match(TracerCommandsParser::SIGNATURE);
        setState(266);
        value();
        break;
      }

      case TracerCommandsParser::MEMBER: {
        enterOuterAlt(_localctx, 3);
        setState(267);
        match(TracerCommandsParser::MEMBER);
        break;
      }

      case TracerCommandsParser::METADATA_ONLY: {
        enterOuterAlt(_localctx, 4);
        setState(268);
        match(TracerCommandsParser::METADATA_ONLY);
        break;
      }

      case TracerCommandsParser::T__158:
      case TracerCommandsParser::T__159: {
        enterOuterAlt(_localctx, 5);
        setState(269);
        extraCompileOption();
        break;
      }

      case TracerCommandsParser::CWD: {
        enterOuterAlt(_localctx, 6);
        setState(270);
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
    setState(276);
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
        setState(273);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 32710487703552) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(274);
        value();
        break;
      }

      case TracerCommandsParser::T__158:
      case TracerCommandsParser::T__159: {
        enterOuterAlt(_localctx, 2);
        setState(275);
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
    setState(285);
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
        setState(278);
        resolveOption();
        break;
      }

      case TracerCommandsParser::T__44: {
        enterOuterAlt(_localctx, 2);
        setState(279);
        match(TracerCommandsParser::T__44);
        break;
      }

      case TracerCommandsParser::T__45: {
        enterOuterAlt(_localctx, 3);
        setState(280);
        match(TracerCommandsParser::T__45);
        break;
      }

      case TracerCommandsParser::T__46: {
        enterOuterAlt(_localctx, 4);
        setState(281);
        match(TracerCommandsParser::T__46);
        break;
      }

      case TracerCommandsParser::T__47: {
        enterOuterAlt(_localctx, 5);
        setState(282);
        match(TracerCommandsParser::T__47);
        break;
      }

      case TracerCommandsParser::T__48: {
        enterOuterAlt(_localctx, 6);
        setState(283);
        match(TracerCommandsParser::T__48);
        break;
      }

      case TracerCommandsParser::T__49: {
        enterOuterAlt(_localctx, 7);
        setState(284);
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
    setState(293);
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
        setState(287);
        validateOption();
        break;
      }

      case TracerCommandsParser::T__50: {
        enterOuterAlt(_localctx, 2);
        setState(288);
        match(TracerCommandsParser::T__50);
        setState(289);
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
        setState(290);
        match(TracerCommandsParser::T__51);
        break;
      }

      case TracerCommandsParser::T__52: {
        enterOuterAlt(_localctx, 4);
        setState(291);
        match(TracerCommandsParser::T__52);
        break;
      }

      case TracerCommandsParser::T__155: {
        enterOuterAlt(_localctx, 5);
        setState(292);
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
    setState(295);
    match(TracerCommandsParser::T__53);
    setState(325);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__54: {
        setState(296);
        match(TracerCommandsParser::T__54);
        setState(297);
        value();
        setState(298);
        match(TracerCommandsParser::T__55);
        setState(299);
        value();
        break;
      }

      case TracerCommandsParser::T__56: {
        setState(301);
        match(TracerCommandsParser::T__56);
        setState(302);
        value();
        break;
      }

      case TracerCommandsParser::T__57: {
        setState(303);
        match(TracerCommandsParser::T__57);
        setState(304);
        value();
        break;
      }

      case TracerCommandsParser::T__34: {
        setState(305);
        match(TracerCommandsParser::T__34);
        setState(306);
        value();
        setState(307);
        match(TracerCommandsParser::T__58);
        setState(308);
        value();
        setState(312);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 2249325929299968) != 0) || _la == TracerCommandsParser::T__158

        || _la == TracerCommandsParser::T__159) {
          setState(309);
          validateOption();
          setState(314);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__59: {
        setState(315);
        match(TracerCommandsParser::T__59);
        setState(316);
        value();
        setState(317);
        match(TracerCommandsParser::T__58);
        setState(318);
        value();
        setState(322);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 18011924625096704) != 0) || ((((_la - 156) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 156)) & 25) != 0)) {
          setState(319);
          deployOption();
          setState(324);
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
    setState(327);
    match(TracerCommandsParser::T__60);
    setState(368);
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
        setState(331);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (((((_la - 20) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 20)) & 4098275729626628099) != 0)) {
          setState(328);
          streamOption();
          setState(333);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__61: {
        setState(334);
        match(TracerCommandsParser::T__61);
        setState(335);
        value();
        setState(338);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__27) {
          setState(336);
          match(TracerCommandsParser::T__27);
          setState(337);
          match(TracerCommandsParser::UINT);
        }
        break;
      }

      case TracerCommandsParser::T__57: {
        setState(340);
        match(TracerCommandsParser::T__57);
        setState(341);
        value();
        setState(342);
        match(TracerCommandsParser::T__62);
        setState(343);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::STACK: {
        setState(345);
        match(TracerCommandsParser::STACK);
        setState(346);
        value();
        setState(347);
        match(TracerCommandsParser::T__62);
        setState(348);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__63: {
        setState(350);
        match(TracerCommandsParser::T__63);
        setState(351);
        value();
        break;
      }

      case TracerCommandsParser::T__64: {
        setState(352);
        match(TracerCommandsParser::T__64);
        setState(353);
        value();
        break;
      }

      case TracerCommandsParser::T__65: {
        setState(354);
        match(TracerCommandsParser::T__65);
        setState(355);
        value();
        break;
      }

      case TracerCommandsParser::T__66: {
        setState(356);
        match(TracerCommandsParser::T__66);
        setState(357);
        value();
        setState(358);
        match(TracerCommandsParser::T__55);
        setState(359);
        value();
        break;
      }

      case TracerCommandsParser::T__67: {
        setState(361);
        match(TracerCommandsParser::T__67);
        setState(362);
        value();
        setState(363);
        match(TracerCommandsParser::T__68);
        setState(364);
        match(TracerCommandsParser::T__69);
        setState(365);
        match(TracerCommandsParser::T__40);
        setState(366);
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
    setState(370);
    match(TracerCommandsParser::T__70);
    setState(374);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (((((_la - 20) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 20)) & 4098275729626628099) != 0)) {
      setState(371);
      streamOption();
      setState(376);
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
    setState(377);
    match(TracerCommandsParser::T__71);
    setState(378);
    value();
    setState(383);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (((((_la - 20) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 20)) & 4098275729626628099) != 0) || _la == TracerCommandsParser::SQLITE) {
      setState(381);
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
          setState(379);
          streamOption();
          break;
        }

        case TracerCommandsParser::SQLITE: {
          setState(380);
          match(TracerCommandsParser::SQLITE);
          break;
        }

      default:
        throw NoViableAltException(this);
      }
      setState(385);
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
    setState(393);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__19:
      case TracerCommandsParser::T__20:
      case TracerCommandsParser::T__72:
      case TracerCommandsParser::T__73: {
        enterOuterAlt(_localctx, 1);
        setState(386);
        _la = _input->LA(1);
        if (!(((((_la - 20) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 20)) & 27021597764222979) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(387);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__74: {
        enterOuterAlt(_localctx, 2);
        setState(388);
        match(TracerCommandsParser::T__74);
        setState(389);
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
        setState(390);
        _la = _input->LA(1);
        if (!(((((_la - 56) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 56)) & 25165825) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(391);
        value();
        break;
      }

      case TracerCommandsParser::T__80: {
        enterOuterAlt(_localctx, 4);
        setState(392);
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
    setState(395);
    match(TracerCommandsParser::T__81);
    setState(424);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__54: {
        setState(396);
        match(TracerCommandsParser::T__54);
        setState(397);
        value();
        break;
      }

      case TracerCommandsParser::T__82: {
        setState(398);
        match(TracerCommandsParser::T__82);
        break;
      }

      case TracerCommandsParser::T__83:
      case TracerCommandsParser::T__84: {
        setState(399);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__83

        || _la == TracerCommandsParser::T__84)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(400);
        value();
        setState(405);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__40

        || _la == TracerCommandsParser::T__85) {
          setState(401);
          _la = _input->LA(1);
          if (!(_la == TracerCommandsParser::T__40

          || _la == TracerCommandsParser::T__85)) {
          _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(402);
          value();
          setState(407);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__86: {
        setState(408);
        match(TracerCommandsParser::T__86);
        setState(422);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__87: {
            setState(409);
            match(TracerCommandsParser::T__87);
            setState(412);
            _errHandler->sync(this);
            _la = _input->LA(1);
            do {
              setState(412);
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
                  setState(410);
                  value();
                  break;
                }

                case TracerCommandsParser::SHELL: {
                  setState(411);
                  match(TracerCommandsParser::SHELL);
                  break;
                }

              default:
                throw NoViableAltException(this);
              }
              setState(414);
              _errHandler->sync(this);
              _la = _input->LA(1);
            } while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 8556839548640946812) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 64)) & -1030863687201) != 0) || ((((_la - 128) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 128)) & 4733053960223) != 0));
            break;
          }

          case TracerCommandsParser::T__65: {
            setState(416);
            match(TracerCommandsParser::T__65);
            break;
          }

          case TracerCommandsParser::T__88: {
            setState(417);
            match(TracerCommandsParser::T__88);
            setState(418);
            value();
            setState(419);
            match(TracerCommandsParser::T__89);
            setState(420);
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
    setState(426);
    match(TracerCommandsParser::T__90);
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
    setState(429);
    match(TracerCommandsParser::T__91);
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
    setState(432);
    match(TracerCommandsParser::T__92);
    setState(433);
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
    setState(435);
    match(TracerCommandsParser::T__93);
    setState(444);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 8556839548640946814) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 64)) & -1030863687201) != 0) || ((((_la - 128) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 128)) & 4698694221855) != 0)) {
      setState(437);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == TracerCommandsParser::T__0) {
        setState(436);
        match(TracerCommandsParser::T__0);
      }
      setState(440);
      _errHandler->sync(this);
      _la = _input->LA(1);
      do {
        setState(439);
        value();
        setState(442);
        _errHandler->sync(this);
        _la = _input->LA(1);
      } while ((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & 8556839548640946812) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 64)) & -1030863687201) != 0) || ((((_la - 128) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 128)) & 4698694221855) != 0));
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
    setState(446);
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
    setState(448);
    match(TracerCommandsParser::T__96);
    setState(464);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__97: {
        setState(449);
        match(TracerCommandsParser::T__97);
        setState(451);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(450);
          bulkTraceOption();
          setState(453);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while (_la == TracerCommandsParser::T__23

        || _la == TracerCommandsParser::T__37 || ((((_la - 100) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 100)) & 15) != 0));
        break;
      }

      case TracerCommandsParser::T__98: {
        setState(455);
        match(TracerCommandsParser::T__98);
        setState(456);
        match(TracerCommandsParser::T__99);
        setState(457);
        value();
        setState(460);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__33) {
          setState(458);
          match(TracerCommandsParser::T__33);
          setState(459);
          value();
        }
        break;
      }

      case TracerCommandsParser::T__35: {
        setState(462);
        match(TracerCommandsParser::T__35);
        setState(463);
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
    setState(466);
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
    setState(467);
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
    setState(473);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::STRING: {
        enterOuterAlt(_localctx, 1);
        setState(469);
        match(TracerCommandsParser::STRING);
        break;
      }

      case TracerCommandsParser::WORD: {
        enterOuterAlt(_localctx, 2);
        setState(470);
        match(TracerCommandsParser::WORD);
        break;
      }

      case TracerCommandsParser::UINT: {
        enterOuterAlt(_localctx, 3);
        setState(471);
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
        setState(472);
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
    setState(475);
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
    setState(477);
    match(TracerCommandsParser::T__103);
    setState(552);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__104: {
        setState(478);
        match(TracerCommandsParser::T__104);
        break;
      }

      case TracerCommandsParser::T__105: {
        setState(479);
        match(TracerCommandsParser::T__105);
        setState(480);
        value();
        setState(481);
        match(TracerCommandsParser::T__26);
        setState(482);
        value();
        setState(487);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__20: {
            setState(483);
            match(TracerCommandsParser::T__20);
            setState(484);
            match(TracerCommandsParser::UINT);
            break;
          }

          case TracerCommandsParser::T__85: {
            setState(485);
            match(TracerCommandsParser::T__85);
            setState(486);
            value();
            break;
          }

        default:
          throw NoViableAltException(this);
        }
        break;
      }

      case TracerCommandsParser::T__106: {
        setState(489);
        match(TracerCommandsParser::T__106);
        setState(490);
        value();
        setState(491);
        match(TracerCommandsParser::T__26);
        setState(492);
        value();
        setState(493);
        match(TracerCommandsParser::T__20);
        setState(494);
        match(TracerCommandsParser::UINT);
        setState(495);
        match(TracerCommandsParser::T__132);
        setState(496);
        value();
        setState(499);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__27) {
          setState(497);
          match(TracerCommandsParser::T__27);
          setState(498);
          match(TracerCommandsParser::UINT);
        }
        break;
      }

      case TracerCommandsParser::T__107: {
        setState(501);
        match(TracerCommandsParser::T__107);
        setState(502);
        match(TracerCommandsParser::T__23);
        setState(503);
        value();
        setState(504);
        match(TracerCommandsParser::T__133);
        setState(505);
        value();
        setState(506);
        match(TracerCommandsParser::T__26);
        setState(507);
        value();
        setState(508);
        match(TracerCommandsParser::T__134);
        setState(509);
        value();
        setState(510);
        match(TracerCommandsParser::T__135);
        setState(511);
        value();
        break;
      }

      case TracerCommandsParser::T__127: {
        setState(513);
        match(TracerCommandsParser::T__127);
        setState(514);
        value();
        setState(515);
        match(TracerCommandsParser::T__26);
        setState(516);
        value();
        setState(517);
        match(TracerCommandsParser::T__134);
        setState(518);
        value();
        setState(519);
        match(TracerCommandsParser::T__135);
        setState(520);
        value();
        break;
      }

      case TracerCommandsParser::T__108:
      case TracerCommandsParser::T__125:
      case TracerCommandsParser::T__126: {
        setState(522);
        _la = _input->LA(1);
        if (!(((((_la - 109) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 109)) & 393217) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(523);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__110: {
        setState(524);
        match(TracerCommandsParser::T__110);
        setState(525);
        match(TracerCommandsParser::T__136);
        setState(526);
        match(TracerCommandsParser::UINT);
        setState(527);
        match(TracerCommandsParser::T__25);
        setState(528);
        value();
        setState(529);
        match(TracerCommandsParser::T__137);
        setState(530);
        match(TracerCommandsParser::UINT);
        setState(531);
        match(TracerCommandsParser::T__26);
        setState(532);
        value();
        setState(535);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__19) {
          setState(533);
          match(TracerCommandsParser::T__19);
          setState(534);
          match(TracerCommandsParser::UINT);
        }
        break;
      }

      case TracerCommandsParser::T__111: {
        setState(537);
        match(TracerCommandsParser::T__111);
        break;
      }

      case TracerCommandsParser::T__109: {
        setState(538);
        match(TracerCommandsParser::T__109);
        setState(539);
        match(TracerCommandsParser::T__23);
        setState(540);
        value();
        setState(541);
        match(TracerCommandsParser::T__138);
        setState(542);
        value();
        setState(543);
        match(TracerCommandsParser::T__139);
        setState(544);
        value();
        setState(545);
        match(TracerCommandsParser::T__25);
        setState(546);
        value();
        setState(547);
        match(TracerCommandsParser::T__26);
        setState(548);
        value();
        setState(549);
        match(TracerCommandsParser::T__140);
        setState(550);
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
    setState(554);
    match(TracerCommandsParser::T__29);
    setState(555);
    match(TracerCommandsParser::T__112);
    setState(572);
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
        setState(556);
        value();
        setState(557);
        match(TracerCommandsParser::T__26);
        setState(558);
        value();
        setState(561);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__23) {
          setState(559);
          match(TracerCommandsParser::T__23);
          setState(560);
          value();
        }
        break;
      }

      case TracerCommandsParser::T__23: {
        setState(563);
        match(TracerCommandsParser::T__23);
        setState(564);
        value();
        setState(565);
        match(TracerCommandsParser::T__133);
        setState(566);
        value();
        setState(567);
        match(TracerCommandsParser::T__140);
        setState(568);
        value();
        setState(569);
        match(TracerCommandsParser::T__26);
        setState(570);
        value();
        break;
      }

    default:
      throw NoViableAltException(this);
    }
    setState(575);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::CWD) {
      setState(574);
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
    setState(577);
    match(TracerCommandsParser::T__29);
    setState(578);
    match(TracerCommandsParser::T__113);
    setState(579);
    match(TracerCommandsParser::UINT);
    setState(580);
    match(TracerCommandsParser::T__33);
    setState(581);
    value();
    setState(593);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == TracerCommandsParser::T__18 || _la == TracerCommandsParser::CWD) {
      setState(591);
      _errHandler->sync(this);
      switch (_input->LA(1)) {
        case TracerCommandsParser::CWD: {
          setState(582);
          cwdOption();
          break;
        }

        case TracerCommandsParser::T__18: {
          setState(583);
          match(TracerCommandsParser::T__18);
          setState(588);
          _errHandler->sync(this);
          _la = _input->LA(1);
          while (_la == TracerCommandsParser::T__19

          || _la == TracerCommandsParser::T__20) {
            setState(584);
            _la = _input->LA(1);
            if (!(_la == TracerCommandsParser::T__19

            || _la == TracerCommandsParser::T__20)) {
            _errHandler->recoverInline(this);
            }
            else {
              _errHandler->reportMatch(this);
              consume();
            }
            setState(585);
            match(TracerCommandsParser::UINT);
            setState(590);
            _errHandler->sync(this);
            _la = _input->LA(1);
          }
          break;
        }

      default:
        throw NoViableAltException(this);
      }
      setState(595);
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
    setState(608);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__114: {
        enterOuterAlt(_localctx, 1);
        setState(596);
        match(TracerCommandsParser::T__114);
        setState(597);
        value();
        setState(601);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__19 || ((((_la - 142) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 142)) & 33554687) != 0)) {
          setState(598);
          launchOption();
          setState(603);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__115:
      case TracerCommandsParser::T__116: {
        enterOuterAlt(_localctx, 2);
        setState(604);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__115

        || _la == TracerCommandsParser::T__116)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(606);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::UINT) {
          setState(605);
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
    setState(616);
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
        setState(610);
        _la = _input->LA(1);
        if (!(((((_la - 142) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 142)) & 33554495) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(611);
        value();
        break;
      }

      case TracerCommandsParser::T__19: {
        enterOuterAlt(_localctx, 2);
        setState(612);
        match(TracerCommandsParser::T__19);
        setState(613);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__147: {
        enterOuterAlt(_localctx, 3);
        setState(614);
        match(TracerCommandsParser::T__147);
        break;
      }

      case TracerCommandsParser::T__148: {
        enterOuterAlt(_localctx, 4);
        setState(615);
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
    setState(618);
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
    setState(636);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__117: {
        enterOuterAlt(_localctx, 1);
        setState(620);
        match(TracerCommandsParser::T__117);
        setState(621);
        value();
        setState(628);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__19 || _la == TracerCommandsParser::T__141) {
          setState(626);
          _errHandler->sync(this);
          switch (_input->LA(1)) {
            case TracerCommandsParser::T__141: {
              setState(622);
              match(TracerCommandsParser::T__141);
              setState(623);
              value();
              break;
            }

            case TracerCommandsParser::T__19: {
              setState(624);
              match(TracerCommandsParser::T__19);
              setState(625);
              match(TracerCommandsParser::UINT);
              break;
            }

          default:
            throw NoViableAltException(this);
          }
          setState(630);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__118: {
        enterOuterAlt(_localctx, 2);
        setState(631);
        match(TracerCommandsParser::T__118);
        setState(634);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__19) {
          setState(632);
          match(TracerCommandsParser::T__19);
          setState(633);
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
    setState(642);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__119: {
        setState(638);
        match(TracerCommandsParser::T__119);
        break;
      }

      case TracerCommandsParser::T__130: {
        setState(639);
        match(TracerCommandsParser::T__130);
        break;
      }

      case TracerCommandsParser::T__120: {
        setState(640);
        match(TracerCommandsParser::T__120);
        setState(641);
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
    setState(648);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == TracerCommandsParser::T__19

    || _la == TracerCommandsParser::T__20) {
      setState(644);
      _la = _input->LA(1);
      if (!(_la == TracerCommandsParser::T__19

      || _la == TracerCommandsParser::T__20)) {
      _errHandler->recoverInline(this);
      }
      else {
        _errHandler->reportMatch(this);
        consume();
      }
      setState(645);
      match(TracerCommandsParser::UINT);
      setState(650);
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
    setState(651);
    match(TracerCommandsParser::T__124);
    setState(652);
    match(TracerCommandsParser::T__54);
    setState(653);
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
    setState(655);
    match(TracerCommandsParser::T__122);
    setState(670);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__2: {
        setState(656);
        match(TracerCommandsParser::T__2);
        setState(660);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 204472320) != 0) || ((((_la - 137) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 137)) & 253955) != 0)) {
          setState(657);
          instructionOption();
          setState(662);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__5: {
        setState(663);
        match(TracerCommandsParser::T__5);
        setState(666);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__102) {
          setState(664);
          match(TracerCommandsParser::T__102);
          setState(665);
          value();
        }
        break;
      }

      case TracerCommandsParser::T__128: {
        setState(668);
        match(TracerCommandsParser::T__128);
        setState(669);
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
    setState(680);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__19:
      case TracerCommandsParser::T__20:
      case TracerCommandsParser::T__136:
      case TracerCommandsParser::T__137: {
        enterOuterAlt(_localctx, 1);
        setState(672);
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
        setState(673);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__149: {
        enterOuterAlt(_localctx, 2);
        setState(674);
        match(TracerCommandsParser::T__149);
        setState(675);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__150: {
        enterOuterAlt(_localctx, 3);
        setState(676);
        match(TracerCommandsParser::T__150);
        break;
      }

      case TracerCommandsParser::T__151: {
        enterOuterAlt(_localctx, 4);
        setState(677);
        match(TracerCommandsParser::T__151);
        break;
      }

      case TracerCommandsParser::T__25:
      case TracerCommandsParser::T__26:
      case TracerCommandsParser::T__152:
      case TracerCommandsParser::T__153: {
        enterOuterAlt(_localctx, 5);
        setState(678);
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
        setState(679);
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
    setState(682);
    match(TracerCommandsParser::T__123);
    setState(689);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 204472320) != 0) || ((((_la - 137) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 137)) & 262147) != 0)) {
      setState(687);
      _errHandler->sync(this);
      switch (_input->LA(1)) {
        case TracerCommandsParser::T__19:
        case TracerCommandsParser::T__20:
        case TracerCommandsParser::T__136:
        case TracerCommandsParser::T__137: {
          setState(683);
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
          setState(684);
          match(TracerCommandsParser::UINT);
          break;
        }

        case TracerCommandsParser::T__25:
        case TracerCommandsParser::T__26:
        case TracerCommandsParser::T__154: {
          setState(685);
          _la = _input->LA(1);
          if (!(_la == TracerCommandsParser::T__25

          || _la == TracerCommandsParser::T__26 || _la == TracerCommandsParser::T__154)) {
          _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(686);
          value();
          break;
        }

      default:
        throw NoViableAltException(this);
      }
      setState(691);
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
    setState(692);
    match(TracerCommandsParser::T__155);
    setState(693);
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
    setState(695);
    _la = _input->LA(1);
    if (!(_la == TracerCommandsParser::T__156

    || _la == TracerCommandsParser::T__157)) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(696);
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
    setState(707);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__158: {
        enterOuterAlt(_localctx, 1);
        setState(698);
        match(TracerCommandsParser::T__158);
        setState(699);
        value();
        break;
      }

      case TracerCommandsParser::T__159: {
        enterOuterAlt(_localctx, 2);
        setState(700);
        match(TracerCommandsParser::T__159);
        setState(703);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(703);
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
              setState(701);
              value();
              break;
            }

            case TracerCommandsParser::SHELL: {
              setState(702);
              match(TracerCommandsParser::SHELL);
              break;
            }

          default:
            throw NoViableAltException(this);
          }
          setState(705);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 8556839548640946812) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 64)) & -1030863687201) != 0) || ((((_la - 128) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 128)) & 4733053960223) != 0));
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
    setState(709);
    match(TracerCommandsParser::CWD);
    setState(710);
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
