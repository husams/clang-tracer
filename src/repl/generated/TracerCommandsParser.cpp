
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
      "symbols", "symbolOption", "probe", "prepareOption", "resolveOption",
      "deployOption", "interceptor", "events", "collect", "record", "streamOption",
      "cpp", "shell", "complete", "type", "help", "quit", "bulkTrace", "bulkTraceOption",
      "value", "keyword", "objectWatch", "objectOption", "memory", "probeEntry",
      "probeReload", "stackFilter", "contextEventBytes", "extraCompileOption",
      "cwdOption"
    },
    std::vector<std::string>{
      "", "'%'", "'server'", "'start'", "'connect'", "'status'", "'stop'",
      "'--executable'", "'--agent'", "'attach'", "'--start-time'", "'processes'",
      "'--filter'", "'sessions'", "'use'", "'detach'", "'modules'", "'--details'",
      "'module'", "'index'", "'types'", "'symbols'", "'show'", "'emit'",
      "'--file'", "'--module'", "'--mapping'", "'--output'", "'--debug-file'",
      "'--max-entries'", "'--max-depth'", "'--index'", "'--page'", "'--limit'",
      "'--format'", "'json'", "'--exported'", "'--type'", "'--symbol'",
      "'--id'", "'threads'", "'--address'", "'--addresses'", "'probe'",
      "'prepare'", "'resolve'", "'inject'", "'--callback'", "'validate'",
      "'remove'", "'probes'", "'--include'", "'--compdb'", "'--translation-unit'",
      "'--module-index'", "'--generated'", "'--compile-option'", "'--prepared'",
      "'--prepared-mode'", "'--stack-depth'", "'--print-arguments'", "'--print-result'",
      "'--snapshot-receiver-at-return'", "'--backend'", "'--caller'", "'interceptor'",
      "'load'", "'--as'", "'edit'", "'--target'", "'upload'", "'events'",
      "'list'", "'diff'", "'--pre'", "'--post'", "'--key'", "'calls'", "'stats'",
      "'clear'", "'open'", "'export'", "'collect'", "'record'", "'--count'",
      "'--timeout'", "'--after'", "'--probe'", "'--phase'", "'PRE'", "'POST'",
      "'--name'", "'--session'", "'--all'", "'cpp'", "'reset'", "'import'",
      "'generate'", "'options'", "'add'", "'compdb'", "'shell'", "'complete'",
      "'type'", "'help'", "'quit'", "'exit'", "'trace'", "'preview'", "'apply'",
      "'--manifest'", "'--exclude'", "'--exact'", "'--save'", "'memory'",
      "'maps'", "'read'", "'scan'", "'entry'", "'reload'", "'object'", "'--base'",
      "'--owner'", "'--field'", "'--collection'", "'--snapshot-field'",
      "'--code'", "'--plan'", "'--mode'", "'--policy'", "'--on'", "'--emit-source'",
      "'--vector'", "'--element-key'", "'--map'", "'--map-key'", "'--pointee'",
      "'--mutation'", "'--synchronization'", "'--union-tag'", "'--union-case'",
      "'--rendezvous'", "'--overflow'", "'--shared-file'", "'--max-elements'",
      "'--max-string-bytes'", "'--max-snapshot-bytes'", "'--max-instances'",
      "'--max-events'", "'--event-buffer'", "'--tid'", "'--async-signal'",
      "'--serialize-wait-us'", "'--runtime-limit'", "'--no-values'", "'--first-seen'",
      "'--writer-entry'", "'--discover-writers'", "'--follow-calls'", "'--syscall-output'",
      "'--stack-cfi'", "'--pattern'", "'--elf-address'", "'--abi'", "'--stack-application-only'",
      "'--context-event-bytes'", "'--context-transport-memory-bytes'", "'--extra-arg'",
      "'--extra-args'", "", "", "", "'--metadata-only'", "'--sqlite'", "'stack'",
      "'--cwd'", "'--signature'", "'--member'", "'--snapshot-return-pointee'"
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
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "UINT",
      "STRING", "SHELL", "METADATA_ONLY", "SQLITE", "STACK", "CWD", "SIGNATURE",
      "MEMBER", "SNAPSHOT_RETURN_POINTEE", "WORD", "WS"
    }
  );
  static const int32_t serializedATNSegment[] = {
    4,1,180,632,2,0,7,0,2,1,7,1,2,2,7,2,2,3,7,3,2,4,7,4,2,5,7,5,2,6,7,6,2,
    7,7,7,2,8,7,8,2,9,7,9,2,10,7,10,2,11,7,11,2,12,7,12,2,13,7,13,2,14,7,
    14,2,15,7,15,2,16,7,16,2,17,7,17,2,18,7,18,2,19,7,19,2,20,7,20,2,21,7,
    21,2,22,7,22,2,23,7,23,2,24,7,24,2,25,7,25,2,26,7,26,2,27,7,27,2,28,7,
    28,2,29,7,29,2,30,7,30,2,31,7,31,2,32,7,32,2,33,7,33,2,34,7,34,2,35,7,
    35,2,36,7,36,2,37,7,37,2,38,7,38,2,39,7,39,2,40,7,40,2,41,7,41,2,42,7,
    42,2,43,7,43,2,44,7,44,2,45,7,45,2,46,7,46,1,0,1,0,1,0,1,0,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,3,1,126,8,1,1,2,1,2,1,2,5,2,131,8,2,10,2,
    12,2,134,9,2,1,2,1,2,1,2,1,2,3,2,140,8,2,1,3,1,3,1,3,3,3,145,8,3,1,4,
    1,4,1,4,1,4,3,4,151,8,4,3,4,153,8,4,1,5,1,5,1,5,3,5,158,8,5,1,6,1,6,1,
    7,1,7,1,7,1,8,1,8,1,9,1,9,3,9,169,8,9,1,10,1,10,3,10,173,8,10,1,11,1,
    11,1,11,4,11,178,8,11,11,11,12,11,179,1,11,1,11,1,11,1,11,5,11,186,8,
    11,10,11,12,11,189,9,11,1,11,1,11,4,11,193,8,11,11,11,12,11,194,1,11,
    1,11,4,11,199,8,11,11,11,12,11,200,3,11,203,8,11,1,12,1,12,1,12,1,12,
    3,12,209,8,12,1,13,1,13,1,13,1,13,1,13,1,13,1,13,3,13,218,8,13,1,14,1,
    14,1,14,1,15,1,15,1,15,3,15,226,8,15,1,16,1,16,1,17,1,17,5,17,232,8,17,
    10,17,12,17,235,9,17,1,18,1,18,1,18,1,18,1,18,3,18,242,8,18,1,19,1,19,
    1,19,1,19,4,19,248,8,19,11,19,12,19,249,1,19,1,19,1,19,1,19,5,19,256,
    8,19,10,19,12,19,259,9,19,1,19,1,19,1,19,1,19,3,19,265,8,19,1,19,1,19,
    5,19,269,8,19,10,19,12,19,272,9,19,1,19,1,19,1,19,1,19,1,19,1,19,5,19,
    280,8,19,10,19,12,19,283,9,19,1,19,1,19,1,19,3,19,288,8,19,3,19,290,8,
    19,1,19,1,19,3,19,294,8,19,3,19,296,8,19,1,20,1,20,1,20,1,20,1,20,1,20,
    1,20,1,20,3,20,306,8,20,1,21,1,21,1,21,3,21,311,8,21,1,22,1,22,1,22,1,
    22,1,22,1,22,1,22,1,22,1,22,1,22,3,22,323,8,22,1,23,1,23,1,23,1,23,1,
    23,1,23,1,23,1,23,1,23,1,23,1,23,1,23,1,23,1,23,1,23,5,23,340,8,23,10,
    23,12,23,343,9,23,1,23,1,23,1,23,1,23,1,23,5,23,350,8,23,10,23,12,23,
    353,9,23,3,23,355,8,23,1,24,1,24,5,24,359,8,24,10,24,12,24,362,9,24,1,
    24,1,24,1,24,1,24,3,24,368,8,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,
    24,1,24,1,24,1,24,1,24,1,24,5,24,383,8,24,10,24,12,24,386,9,24,1,24,1,
    24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,
    24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,3,24,411,8,24,1,25,1,25,5,25,415,
    8,25,10,25,12,25,418,9,25,1,26,1,26,1,26,1,26,5,26,424,8,26,10,26,12,
    26,427,9,26,1,27,1,27,1,27,1,27,1,27,1,27,1,27,3,27,436,8,27,1,28,1,28,
    1,28,1,28,1,28,1,28,1,28,1,28,5,28,446,8,28,10,28,12,28,449,9,28,1,28,
    1,28,1,28,1,28,4,28,455,8,28,11,28,12,28,456,1,28,1,28,1,28,1,28,1,28,
    1,28,3,28,465,8,28,3,28,467,8,28,1,29,1,29,1,29,1,30,1,30,1,30,1,31,1,
    31,1,31,1,32,1,32,3,32,480,8,32,1,32,4,32,483,8,32,11,32,12,32,484,3,
    32,487,8,32,1,33,1,33,1,34,1,34,1,34,4,34,494,8,34,11,34,12,34,495,1,
    34,1,34,1,34,1,34,1,34,3,34,503,8,34,1,34,1,34,3,34,507,8,34,1,35,1,35,
    1,35,1,36,1,36,1,36,1,36,3,36,516,8,36,1,37,1,37,1,38,1,38,1,38,3,38,
    523,8,38,1,38,4,38,526,8,38,11,38,12,38,527,1,38,1,38,4,38,532,8,38,11,
    38,12,38,533,1,38,1,38,3,38,538,8,38,3,38,540,8,38,1,39,1,39,1,39,1,39,
    1,39,1,39,1,39,1,39,1,39,1,39,1,39,3,39,553,8,39,1,40,1,40,1,40,1,40,
    1,40,1,40,1,40,1,40,1,40,1,40,3,40,565,8,40,1,40,1,40,1,40,1,40,1,40,
    1,40,1,40,1,40,1,40,1,40,3,40,577,8,40,3,40,579,8,40,1,41,1,41,1,41,1,
    41,1,41,1,41,1,41,3,41,588,8,41,1,41,1,41,1,41,1,41,1,41,1,41,1,41,1,
    41,1,41,3,41,599,8,41,1,41,3,41,602,8,41,1,42,1,42,1,42,1,42,1,42,1,42,
    3,42,610,8,42,1,43,1,43,1,43,1,44,1,44,1,44,1,45,1,45,1,45,1,45,1,45,
    4,45,623,8,45,11,45,12,45,624,3,45,627,8,45,1,46,1,46,1,46,1,46,0,0,47,
    0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,34,36,38,40,42,44,46,48,
    50,52,54,56,58,60,62,64,66,68,70,72,74,76,78,80,82,84,86,88,90,92,0,24,
    1,0,7,8,1,0,20,21,1,0,24,28,1,0,29,30,2,0,12,12,31,32,2,0,31,31,37,39,
    4,0,12,12,25,26,32,32,41,41,4,0,25,25,27,27,39,39,51,55,3,0,25,25,51,
    53,56,58,2,0,169,169,179,179,1,0,63,64,1,0,84,87,1,0,89,90,2,0,67,67,
    91,92,1,0,96,97,2,0,27,27,37,37,1,0,105,106,3,0,25,25,51,51,110,113,17,
    0,2,6,9,9,11,11,13,16,18,23,35,35,40,40,43,46,48,50,65,66,68,68,70,72,
    77,83,89,90,94,109,114,120,174,174,2,0,3,3,108,108,1,0,5,6,8,0,7,7,25,
    25,27,27,37,37,51,53,63,64,76,76,121,143,4,0,30,30,59,59,85,85,144,153,
    1,0,165,166,732,0,94,1,0,0,0,2,125,1,0,0,0,4,127,1,0,0,0,6,144,1,0,0,
    0,8,146,1,0,0,0,10,154,1,0,0,0,12,159,1,0,0,0,14,161,1,0,0,0,16,164,1,
    0,0,0,18,166,1,0,0,0,20,170,1,0,0,0,22,174,1,0,0,0,24,208,1,0,0,0,26,
    217,1,0,0,0,28,219,1,0,0,0,30,225,1,0,0,0,32,227,1,0,0,0,34,229,1,0,0,
    0,36,241,1,0,0,0,38,295,1,0,0,0,40,305,1,0,0,0,42,310,1,0,0,0,44,322,
    1,0,0,0,46,324,1,0,0,0,48,356,1,0,0,0,50,412,1,0,0,0,52,419,1,0,0,0,54,
    435,1,0,0,0,56,437,1,0,0,0,58,468,1,0,0,0,60,471,1,0,0,0,62,474,1,0,0,
    0,64,477,1,0,0,0,66,488,1,0,0,0,68,490,1,0,0,0,70,508,1,0,0,0,72,515,
    1,0,0,0,74,517,1,0,0,0,76,519,1,0,0,0,78,552,1,0,0,0,80,554,1,0,0,0,82,
    580,1,0,0,0,84,603,1,0,0,0,86,611,1,0,0,0,88,614,1,0,0,0,90,626,1,0,0,
    0,92,628,1,0,0,0,94,95,5,1,0,0,95,96,3,2,1,0,96,97,5,0,0,1,97,1,1,0,0,
    0,98,126,3,4,2,0,99,126,3,8,4,0,100,126,3,10,5,0,101,126,3,12,6,0,102,
    126,3,14,7,0,103,126,3,16,8,0,104,126,3,18,9,0,105,126,3,20,10,0,106,
    126,3,22,11,0,107,126,3,32,16,0,108,126,3,34,17,0,109,126,3,38,19,0,110,
    126,3,46,23,0,111,126,3,48,24,0,112,126,3,50,25,0,113,126,3,52,26,0,114,
    126,3,56,28,0,115,126,3,58,29,0,116,126,3,60,30,0,117,126,3,62,31,0,118,
    126,3,64,32,0,119,126,3,66,33,0,120,126,3,68,34,0,121,126,3,80,40,0,122,
    126,3,82,41,0,123,126,3,84,42,0,124,126,3,76,38,0,125,98,1,0,0,0,125,
    99,1,0,0,0,125,100,1,0,0,0,125,101,1,0,0,0,125,102,1,0,0,0,125,103,1,
    0,0,0,125,104,1,0,0,0,125,105,1,0,0,0,125,106,1,0,0,0,125,107,1,0,0,0,
    125,108,1,0,0,0,125,109,1,0,0,0,125,110,1,0,0,0,125,111,1,0,0,0,125,112,
    1,0,0,0,125,113,1,0,0,0,125,114,1,0,0,0,125,115,1,0,0,0,125,116,1,0,0,
    0,125,117,1,0,0,0,125,118,1,0,0,0,125,119,1,0,0,0,125,120,1,0,0,0,125,
    121,1,0,0,0,125,122,1,0,0,0,125,123,1,0,0,0,125,124,1,0,0,0,126,3,1,0,
    0,0,127,139,5,2,0,0,128,132,5,3,0,0,129,131,3,6,3,0,130,129,1,0,0,0,131,
    134,1,0,0,0,132,130,1,0,0,0,132,133,1,0,0,0,133,140,1,0,0,0,134,132,1,
    0,0,0,135,136,5,4,0,0,136,140,3,72,36,0,137,140,5,5,0,0,138,140,5,6,0,
    0,139,128,1,0,0,0,139,135,1,0,0,0,139,137,1,0,0,0,139,138,1,0,0,0,140,
    5,1,0,0,0,141,142,7,0,0,0,142,145,3,72,36,0,143,145,3,88,44,0,144,141,
    1,0,0,0,144,143,1,0,0,0,145,7,1,0,0,0,146,152,5,9,0,0,147,150,5,169,0,
    0,148,149,5,10,0,0,149,151,5,169,0,0,150,148,1,0,0,0,150,151,1,0,0,0,
    151,153,1,0,0,0,152,147,1,0,0,0,152,153,1,0,0,0,153,9,1,0,0,0,154,157,
    5,11,0,0,155,156,5,12,0,0,156,158,3,72,36,0,157,155,1,0,0,0,157,158,1,
    0,0,0,158,11,1,0,0,0,159,160,5,13,0,0,160,13,1,0,0,0,161,162,5,14,0,0,
    162,163,3,72,36,0,163,15,1,0,0,0,164,165,5,5,0,0,165,17,1,0,0,0,166,168,
    5,15,0,0,167,169,3,72,36,0,168,167,1,0,0,0,168,169,1,0,0,0,169,19,1,0,
    0,0,170,172,5,16,0,0,171,173,5,17,0,0,172,171,1,0,0,0,172,173,1,0,0,0,
    173,21,1,0,0,0,174,202,5,18,0,0,175,177,5,19,0,0,176,178,3,24,12,0,177,
    176,1,0,0,0,178,179,1,0,0,0,179,177,1,0,0,0,179,180,1,0,0,0,180,203,1,
    0,0,0,181,182,5,14,0,0,182,203,3,72,36,0,183,187,7,1,0,0,184,186,3,26,
    13,0,185,184,1,0,0,0,186,189,1,0,0,0,187,185,1,0,0,0,187,188,1,0,0,0,
    188,203,1,0,0,0,189,187,1,0,0,0,190,192,5,22,0,0,191,193,3,28,14,0,192,
    191,1,0,0,0,193,194,1,0,0,0,194,192,1,0,0,0,194,195,1,0,0,0,195,203,1,
    0,0,0,196,198,5,23,0,0,197,199,3,30,15,0,198,197,1,0,0,0,199,200,1,0,
    0,0,200,198,1,0,0,0,200,201,1,0,0,0,201,203,1,0,0,0,202,175,1,0,0,0,202,
    181,1,0,0,0,202,183,1,0,0,0,202,190,1,0,0,0,202,196,1,0,0,0,203,23,1,
    0,0,0,204,205,7,2,0,0,205,209,3,72,36,0,206,207,7,3,0,0,207,209,5,169,
    0,0,208,204,1,0,0,0,208,206,1,0,0,0,209,25,1,0,0,0,210,211,7,4,0,0,211,
    218,3,72,36,0,212,213,5,33,0,0,213,218,5,169,0,0,214,215,5,34,0,0,215,
    218,5,35,0,0,216,218,5,36,0,0,217,210,1,0,0,0,217,212,1,0,0,0,217,214,
    1,0,0,0,217,216,1,0,0,0,218,27,1,0,0,0,219,220,7,5,0,0,220,221,3,72,36,
    0,221,29,1,0,0,0,222,226,3,28,14,0,223,224,5,27,0,0,224,226,3,72,36,0,
    225,222,1,0,0,0,225,223,1,0,0,0,226,31,1,0,0,0,227,228,5,40,0,0,228,33,
    1,0,0,0,229,233,5,21,0,0,230,232,3,36,18,0,231,230,1,0,0,0,232,235,1,
    0,0,0,233,231,1,0,0,0,233,234,1,0,0,0,234,35,1,0,0,0,235,233,1,0,0,0,
    236,237,7,6,0,0,237,242,3,72,36,0,238,239,5,33,0,0,239,242,5,169,0,0,
    240,242,5,42,0,0,241,236,1,0,0,0,241,238,1,0,0,0,241,240,1,0,0,0,242,
    37,1,0,0,0,243,289,5,43,0,0,244,245,5,44,0,0,245,247,3,72,36,0,246,248,
    3,40,20,0,247,246,1,0,0,0,248,249,1,0,0,0,249,247,1,0,0,0,249,250,1,0,
    0,0,250,290,1,0,0,0,251,252,5,45,0,0,252,257,3,72,36,0,253,256,3,42,21,
    0,254,256,3,92,46,0,255,253,1,0,0,0,255,254,1,0,0,0,256,259,1,0,0,0,257,
    255,1,0,0,0,257,258,1,0,0,0,258,290,1,0,0,0,259,257,1,0,0,0,260,261,5,
    46,0,0,261,264,3,72,36,0,262,263,5,47,0,0,263,265,3,72,36,0,264,262,1,
    0,0,0,264,265,1,0,0,0,265,270,1,0,0,0,266,269,3,44,22,0,267,269,3,92,
    46,0,268,266,1,0,0,0,268,267,1,0,0,0,269,272,1,0,0,0,270,268,1,0,0,0,
    270,271,1,0,0,0,271,290,1,0,0,0,272,270,1,0,0,0,273,274,5,48,0,0,274,
    275,3,72,36,0,275,276,5,47,0,0,276,281,3,72,36,0,277,280,3,42,21,0,278,
    280,3,92,46,0,279,277,1,0,0,0,279,278,1,0,0,0,280,283,1,0,0,0,281,279,
    1,0,0,0,281,282,1,0,0,0,282,290,1,0,0,0,283,281,1,0,0,0,284,285,5,49,
    0,0,285,287,5,169,0,0,286,288,3,92,46,0,287,286,1,0,0,0,287,288,1,0,0,
    0,288,290,1,0,0,0,289,244,1,0,0,0,289,251,1,0,0,0,289,260,1,0,0,0,289,
    273,1,0,0,0,289,284,1,0,0,0,290,296,1,0,0,0,291,293,5,50,0,0,292,294,
    3,92,46,0,293,292,1,0,0,0,293,294,1,0,0,0,294,296,1,0,0,0,295,243,1,0,
    0,0,295,291,1,0,0,0,296,39,1,0,0,0,297,298,7,7,0,0,298,306,3,72,36,0,
    299,300,5,176,0,0,300,306,3,72,36,0,301,306,5,177,0,0,302,306,5,172,0,
    0,303,306,3,90,45,0,304,306,3,92,46,0,305,297,1,0,0,0,305,299,1,0,0,0,
    305,301,1,0,0,0,305,302,1,0,0,0,305,303,1,0,0,0,305,304,1,0,0,0,306,41,
    1,0,0,0,307,308,7,8,0,0,308,311,3,72,36,0,309,311,3,90,45,0,310,307,1,
    0,0,0,310,309,1,0,0,0,311,43,1,0,0,0,312,323,3,42,21,0,313,314,5,59,0,
    0,314,323,7,9,0,0,315,323,5,60,0,0,316,323,5,61,0,0,317,323,5,178,0,0,
    318,323,5,62,0,0,319,323,3,86,43,0,320,321,7,10,0,0,321,323,3,72,36,0,
    322,312,1,0,0,0,322,313,1,0,0,0,322,315,1,0,0,0,322,316,1,0,0,0,322,317,
    1,0,0,0,322,318,1,0,0,0,322,319,1,0,0,0,322,320,1,0,0,0,323,45,1,0,0,
    0,324,354,5,65,0,0,325,326,5,66,0,0,326,327,3,72,36,0,327,328,5,67,0,
    0,328,329,3,72,36,0,329,355,1,0,0,0,330,331,5,68,0,0,331,355,3,72,36,
    0,332,333,5,22,0,0,333,355,3,72,36,0,334,335,5,48,0,0,335,336,3,72,36,
    0,336,337,5,69,0,0,337,341,3,72,36,0,338,340,3,42,21,0,339,338,1,0,0,
    0,340,343,1,0,0,0,341,339,1,0,0,0,341,342,1,0,0,0,342,355,1,0,0,0,343,
    341,1,0,0,0,344,345,5,70,0,0,345,346,3,72,36,0,346,347,5,69,0,0,347,351,
    3,72,36,0,348,350,3,44,22,0,349,348,1,0,0,0,350,353,1,0,0,0,351,349,1,
    0,0,0,351,352,1,0,0,0,352,355,1,0,0,0,353,351,1,0,0,0,354,325,1,0,0,0,
    354,330,1,0,0,0,354,332,1,0,0,0,354,334,1,0,0,0,354,344,1,0,0,0,355,47,
    1,0,0,0,356,410,5,71,0,0,357,359,3,54,27,0,358,357,1,0,0,0,359,362,1,
    0,0,0,360,358,1,0,0,0,360,361,1,0,0,0,361,411,1,0,0,0,362,360,1,0,0,0,
    363,364,5,72,0,0,364,367,3,72,36,0,365,366,5,33,0,0,366,368,5,169,0,0,
    367,365,1,0,0,0,367,368,1,0,0,0,368,411,1,0,0,0,369,370,5,22,0,0,370,
    371,3,72,36,0,371,372,5,31,0,0,372,373,5,169,0,0,373,411,1,0,0,0,374,
    375,5,73,0,0,375,376,3,72,36,0,376,377,5,74,0,0,377,378,5,169,0,0,378,
    379,5,75,0,0,379,384,5,169,0,0,380,381,5,76,0,0,381,383,3,72,36,0,382,
    380,1,0,0,0,383,386,1,0,0,0,384,382,1,0,0,0,384,385,1,0,0,0,385,411,1,
    0,0,0,386,384,1,0,0,0,387,388,5,174,0,0,388,389,3,72,36,0,389,390,5,31,
    0,0,390,391,5,169,0,0,391,411,1,0,0,0,392,393,5,77,0,0,393,411,3,72,36,
    0,394,395,5,78,0,0,395,411,3,72,36,0,396,397,5,79,0,0,397,411,3,72,36,
    0,398,399,5,80,0,0,399,400,3,72,36,0,400,401,5,67,0,0,401,402,3,72,36,
    0,402,411,1,0,0,0,403,404,5,81,0,0,404,405,3,72,36,0,405,406,5,34,0,0,
    406,407,5,35,0,0,407,408,5,27,0,0,408,409,3,72,36,0,409,411,1,0,0,0,410,
    360,1,0,0,0,410,363,1,0,0,0,410,369,1,0,0,0,410,374,1,0,0,0,410,387,1,
    0,0,0,410,392,1,0,0,0,410,394,1,0,0,0,410,396,1,0,0,0,410,398,1,0,0,0,
    410,403,1,0,0,0,411,49,1,0,0,0,412,416,5,82,0,0,413,415,3,54,27,0,414,
    413,1,0,0,0,415,418,1,0,0,0,416,414,1,0,0,0,416,417,1,0,0,0,417,51,1,
    0,0,0,418,416,1,0,0,0,419,420,5,83,0,0,420,425,3,72,36,0,421,424,3,54,
    27,0,422,424,5,173,0,0,423,421,1,0,0,0,423,422,1,0,0,0,424,427,1,0,0,
    0,425,423,1,0,0,0,425,426,1,0,0,0,426,53,1,0,0,0,427,425,1,0,0,0,428,
    429,7,11,0,0,429,436,5,169,0,0,430,431,5,88,0,0,431,436,7,12,0,0,432,
    433,7,13,0,0,433,436,3,72,36,0,434,436,5,93,0,0,435,428,1,0,0,0,435,430,
    1,0,0,0,435,432,1,0,0,0,435,434,1,0,0,0,436,55,1,0,0,0,437,466,5,94,0,
    0,438,439,5,66,0,0,439,467,3,72,36,0,440,467,5,95,0,0,441,442,7,14,0,
    0,442,447,3,72,36,0,443,444,7,15,0,0,444,446,3,72,36,0,445,443,1,0,0,
    0,446,449,1,0,0,0,447,445,1,0,0,0,447,448,1,0,0,0,448,467,1,0,0,0,449,
    447,1,0,0,0,450,464,5,98,0,0,451,454,5,99,0,0,452,455,3,72,36,0,453,455,
    5,171,0,0,454,452,1,0,0,0,454,453,1,0,0,0,455,456,1,0,0,0,456,454,1,0,
    0,0,456,457,1,0,0,0,457,465,1,0,0,0,458,465,5,79,0,0,459,460,5,100,0,
    0,460,461,3,72,36,0,461,462,5,24,0,0,462,463,3,72,36,0,463,465,1,0,0,
    0,464,451,1,0,0,0,464,458,1,0,0,0,464,459,1,0,0,0,464,465,1,0,0,0,465,
    467,1,0,0,0,466,438,1,0,0,0,466,440,1,0,0,0,466,441,1,0,0,0,466,450,1,
    0,0,0,467,57,1,0,0,0,468,469,5,101,0,0,469,470,3,72,36,0,470,59,1,0,0,
    0,471,472,5,102,0,0,472,473,3,72,36,0,473,61,1,0,0,0,474,475,5,103,0,
    0,475,476,3,72,36,0,476,63,1,0,0,0,477,486,5,104,0,0,478,480,5,1,0,0,
    479,478,1,0,0,0,479,480,1,0,0,0,480,482,1,0,0,0,481,483,3,72,36,0,482,
    481,1,0,0,0,483,484,1,0,0,0,484,482,1,0,0,0,484,485,1,0,0,0,485,487,1,
    0,0,0,486,479,1,0,0,0,486,487,1,0,0,0,487,65,1,0,0,0,488,489,7,16,0,0,
    489,67,1,0,0,0,490,506,5,107,0,0,491,493,5,108,0,0,492,494,3,70,35,0,
    493,492,1,0,0,0,494,495,1,0,0,0,495,493,1,0,0,0,495,496,1,0,0,0,496,507,
    1,0,0,0,497,498,5,109,0,0,498,499,5,110,0,0,499,502,3,72,36,0,500,501,
    5,47,0,0,501,503,3,72,36,0,502,500,1,0,0,0,502,503,1,0,0,0,503,507,1,
    0,0,0,504,505,5,49,0,0,505,507,3,72,36,0,506,491,1,0,0,0,506,497,1,0,
    0,0,506,504,1,0,0,0,507,69,1,0,0,0,508,509,7,17,0,0,509,510,3,72,36,0,
    510,71,1,0,0,0,511,516,5,170,0,0,512,516,5,179,0,0,513,516,5,169,0,0,
    514,516,3,74,37,0,515,511,1,0,0,0,515,512,1,0,0,0,515,513,1,0,0,0,515,
    514,1,0,0,0,516,73,1,0,0,0,517,518,7,18,0,0,518,75,1,0,0,0,519,539,5,
    120,0,0,520,522,5,44,0,0,521,523,3,72,36,0,522,521,1,0,0,0,522,523,1,
    0,0,0,523,525,1,0,0,0,524,526,3,78,39,0,525,524,1,0,0,0,526,527,1,0,0,
    0,527,525,1,0,0,0,527,528,1,0,0,0,528,540,1,0,0,0,529,531,7,19,0,0,530,
    532,3,78,39,0,531,530,1,0,0,0,532,533,1,0,0,0,533,531,1,0,0,0,533,534,
    1,0,0,0,534,540,1,0,0,0,535,537,7,20,0,0,536,538,5,169,0,0,537,536,1,
    0,0,0,537,538,1,0,0,0,538,540,1,0,0,0,539,520,1,0,0,0,539,529,1,0,0,0,
    539,535,1,0,0,0,540,77,1,0,0,0,541,542,7,21,0,0,542,553,3,72,36,0,543,
    544,7,22,0,0,544,553,5,169,0,0,545,553,5,154,0,0,546,553,5,155,0,0,547,
    553,5,156,0,0,548,553,5,157,0,0,549,553,5,158,0,0,550,553,5,159,0,0,551,
    553,5,160,0,0,552,541,1,0,0,0,552,543,1,0,0,0,552,545,1,0,0,0,552,546,
    1,0,0,0,552,547,1,0,0,0,552,548,1,0,0,0,552,549,1,0,0,0,552,550,1,0,0,
    0,552,551,1,0,0,0,553,79,1,0,0,0,554,578,5,114,0,0,555,579,5,115,0,0,
    556,557,5,116,0,0,557,558,3,72,36,0,558,559,5,26,0,0,559,564,3,72,36,
    0,560,561,5,84,0,0,561,565,5,169,0,0,562,563,5,37,0,0,563,565,3,72,36,
    0,564,560,1,0,0,0,564,562,1,0,0,0,565,579,1,0,0,0,566,567,5,117,0,0,567,
    568,3,72,36,0,568,569,5,26,0,0,569,570,3,72,36,0,570,571,5,84,0,0,571,
    572,5,169,0,0,572,573,5,161,0,0,573,576,3,72,36,0,574,575,5,33,0,0,575,
    577,5,169,0,0,576,574,1,0,0,0,576,577,1,0,0,0,577,579,1,0,0,0,578,555,
    1,0,0,0,578,556,1,0,0,0,578,566,1,0,0,0,579,81,1,0,0,0,580,581,5,43,0,
    0,581,598,5,118,0,0,582,583,3,72,36,0,583,584,5,26,0,0,584,587,3,72,36,
    0,585,586,5,25,0,0,586,588,3,72,36,0,587,585,1,0,0,0,587,588,1,0,0,0,
    588,599,1,0,0,0,589,590,5,25,0,0,590,591,3,72,36,0,591,592,5,162,0,0,
    592,593,3,72,36,0,593,594,5,163,0,0,594,595,3,72,36,0,595,596,5,26,0,
    0,596,597,3,72,36,0,597,599,1,0,0,0,598,582,1,0,0,0,598,589,1,0,0,0,599,
    601,1,0,0,0,600,602,3,92,46,0,601,600,1,0,0,0,601,602,1,0,0,0,602,83,
    1,0,0,0,603,604,5,43,0,0,604,605,5,119,0,0,605,606,5,169,0,0,606,607,
    5,47,0,0,607,609,3,72,36,0,608,610,3,92,46,0,609,608,1,0,0,0,609,610,
    1,0,0,0,610,85,1,0,0,0,611,612,5,164,0,0,612,613,3,72,36,0,613,87,1,0,
    0,0,614,615,7,23,0,0,615,616,5,169,0,0,616,89,1,0,0,0,617,618,5,167,0,
    0,618,627,3,72,36,0,619,622,5,168,0,0,620,623,3,72,36,0,621,623,5,171,
    0,0,622,620,1,0,0,0,622,621,1,0,0,0,623,624,1,0,0,0,624,622,1,0,0,0,624,
    625,1,0,0,0,625,627,1,0,0,0,626,617,1,0,0,0,626,619,1,0,0,0,627,91,1,
    0,0,0,628,629,5,175,0,0,629,630,3,72,36,0,630,93,1,0,0,0,73,125,132,139,
    144,150,152,157,168,172,179,187,194,200,202,208,217,225,233,241,249,255,
    257,264,268,270,279,281,287,289,293,295,305,310,322,341,351,354,360,367,
    384,410,416,423,425,435,447,454,456,464,466,479,484,486,495,502,506,515,
    522,527,533,537,539,552,564,576,578,587,598,601,609,622,624,626
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
    setState(94);
    match(TracerCommandsParser::T__0);
    setState(95);
    command();
    setState(96);
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

TracerCommandsParser::ObjectWatchContext* TracerCommandsParser::CommandContext::objectWatch() {
  return getRuleContext<TracerCommandsParser::ObjectWatchContext>(0);
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
    setState(125);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 0, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(98);
      server();
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(99);
      attach();
      break;
    }

    case 3: {
      enterOuterAlt(_localctx, 3);
      setState(100);
      processes();
      break;
    }

    case 4: {
      enterOuterAlt(_localctx, 4);
      setState(101);
      sessions();
      break;
    }

    case 5: {
      enterOuterAlt(_localctx, 5);
      setState(102);
      use();
      break;
    }

    case 6: {
      enterOuterAlt(_localctx, 6);
      setState(103);
      status();
      break;
    }

    case 7: {
      enterOuterAlt(_localctx, 7);
      setState(104);
      detach();
      break;
    }

    case 8: {
      enterOuterAlt(_localctx, 8);
      setState(105);
      modules();
      break;
    }

    case 9: {
      enterOuterAlt(_localctx, 9);
      setState(106);
      moduleIndex();
      break;
    }

    case 10: {
      enterOuterAlt(_localctx, 10);
      setState(107);
      threads();
      break;
    }

    case 11: {
      enterOuterAlt(_localctx, 11);
      setState(108);
      symbols();
      break;
    }

    case 12: {
      enterOuterAlt(_localctx, 12);
      setState(109);
      probe();
      break;
    }

    case 13: {
      enterOuterAlt(_localctx, 13);
      setState(110);
      interceptor();
      break;
    }

    case 14: {
      enterOuterAlt(_localctx, 14);
      setState(111);
      events();
      break;
    }

    case 15: {
      enterOuterAlt(_localctx, 15);
      setState(112);
      collect();
      break;
    }

    case 16: {
      enterOuterAlt(_localctx, 16);
      setState(113);
      record();
      break;
    }

    case 17: {
      enterOuterAlt(_localctx, 17);
      setState(114);
      cpp();
      break;
    }

    case 18: {
      enterOuterAlt(_localctx, 18);
      setState(115);
      shell();
      break;
    }

    case 19: {
      enterOuterAlt(_localctx, 19);
      setState(116);
      complete();
      break;
    }

    case 20: {
      enterOuterAlt(_localctx, 20);
      setState(117);
      type();
      break;
    }

    case 21: {
      enterOuterAlt(_localctx, 21);
      setState(118);
      help();
      break;
    }

    case 22: {
      enterOuterAlt(_localctx, 22);
      setState(119);
      quit();
      break;
    }

    case 23: {
      enterOuterAlt(_localctx, 23);
      setState(120);
      bulkTrace();
      break;
    }

    case 24: {
      enterOuterAlt(_localctx, 24);
      setState(121);
      memory();
      break;
    }

    case 25: {
      enterOuterAlt(_localctx, 25);
      setState(122);
      probeEntry();
      break;
    }

    case 26: {
      enterOuterAlt(_localctx, 26);
      setState(123);
      probeReload();
      break;
    }

    case 27: {
      enterOuterAlt(_localctx, 27);
      setState(124);
      objectWatch();
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
    setState(127);
    match(TracerCommandsParser::T__1);
    setState(139);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__2: {
        setState(128);
        match(TracerCommandsParser::T__2);
        setState(132);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__6

        || _la == TracerCommandsParser::T__7 || _la == TracerCommandsParser::T__164

        || _la == TracerCommandsParser::T__165) {
          setState(129);
          serverOption();
          setState(134);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__3: {
        setState(135);
        match(TracerCommandsParser::T__3);
        setState(136);
        value();
        break;
      }

      case TracerCommandsParser::T__4: {
        setState(137);
        match(TracerCommandsParser::T__4);
        break;
      }

      case TracerCommandsParser::T__5: {
        setState(138);
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
    setState(144);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__6:
      case TracerCommandsParser::T__7: {
        enterOuterAlt(_localctx, 1);
        setState(141);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__6

        || _la == TracerCommandsParser::T__7)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(142);
        value();
        break;
      }

      case TracerCommandsParser::T__164:
      case TracerCommandsParser::T__165: {
        enterOuterAlt(_localctx, 2);
        setState(143);
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
    setState(146);
    match(TracerCommandsParser::T__8);
    setState(152);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::UINT) {
      setState(147);
      match(TracerCommandsParser::UINT);
      setState(150);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == TracerCommandsParser::T__9) {
        setState(148);
        match(TracerCommandsParser::T__9);
        setState(149);
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
    setState(154);
    match(TracerCommandsParser::T__10);
    setState(157);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::T__11) {
      setState(155);
      match(TracerCommandsParser::T__11);
      setState(156);
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
    setState(159);
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
    setState(161);
    match(TracerCommandsParser::T__13);
    setState(162);
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
    setState(164);
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
    setState(166);
    match(TracerCommandsParser::T__14);
    setState(168);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 2103400120314492) != 0) || ((((_la - 65) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 65)) & 71529827970576619) != 0) || ((((_la - 169) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 169)) & 1059) != 0)) {
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

//----------------- ModulesContext ------------------------------------------------------------------

TracerCommandsParser::ModulesContext::ModulesContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
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
    setState(170);
    match(TracerCommandsParser::T__15);
    setState(172);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::T__16) {
      setState(171);
      match(TracerCommandsParser::T__16);
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
    setState(174);
    match(TracerCommandsParser::T__17);
    setState(202);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__18: {
        setState(175);
        match(TracerCommandsParser::T__18);
        setState(177);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(176);
          moduleBuildOption();
          setState(179);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 2130706432) != 0));
        break;
      }

      case TracerCommandsParser::T__13: {
        setState(181);
        match(TracerCommandsParser::T__13);
        setState(182);
        value();
        break;
      }

      case TracerCommandsParser::T__19:
      case TracerCommandsParser::T__20: {
        setState(183);
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
        _errHandler->sync(this);
        _la = _input->LA(1);
        while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 100931735552) != 0)) {
          setState(184);
          moduleQueryOption();
          setState(189);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__21: {
        setState(190);
        match(TracerCommandsParser::T__21);
        setState(192);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(191);
          moduleShowOption();
          setState(194);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 964220157952) != 0));
        break;
      }

      case TracerCommandsParser::T__22: {
        setState(196);
        match(TracerCommandsParser::T__22);
        setState(198);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(197);
          moduleEmitOption();
          setState(200);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 964354375680) != 0));
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
    setState(208);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__23:
      case TracerCommandsParser::T__24:
      case TracerCommandsParser::T__25:
      case TracerCommandsParser::T__26:
      case TracerCommandsParser::T__27: {
        enterOuterAlt(_localctx, 1);
        setState(204);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 520093696) != 0))) {
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

      case TracerCommandsParser::T__28:
      case TracerCommandsParser::T__29: {
        enterOuterAlt(_localctx, 2);
        setState(206);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__28

        || _la == TracerCommandsParser::T__29)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(207);
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
    setState(217);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__11:
      case TracerCommandsParser::T__30:
      case TracerCommandsParser::T__31: {
        enterOuterAlt(_localctx, 1);
        setState(210);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 6442455040) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(211);
        value();
        break;
      }

      case TracerCommandsParser::T__32: {
        enterOuterAlt(_localctx, 2);
        setState(212);
        match(TracerCommandsParser::T__32);
        setState(213);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__33: {
        enterOuterAlt(_localctx, 3);
        setState(214);
        match(TracerCommandsParser::T__33);
        setState(215);
        match(TracerCommandsParser::T__34);
        break;
      }

      case TracerCommandsParser::T__35: {
        enterOuterAlt(_localctx, 4);
        setState(216);
        match(TracerCommandsParser::T__35);
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
    setState(219);
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 964220157952) != 0))) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(220);
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
    setState(225);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__30:
      case TracerCommandsParser::T__36:
      case TracerCommandsParser::T__37:
      case TracerCommandsParser::T__38: {
        enterOuterAlt(_localctx, 1);
        setState(222);
        moduleShowOption();
        break;
      }

      case TracerCommandsParser::T__26: {
        enterOuterAlt(_localctx, 2);
        setState(223);
        match(TracerCommandsParser::T__26);
        setState(224);
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
    setState(227);
    match(TracerCommandsParser::T__39);

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
    setState(229);
    match(TracerCommandsParser::T__20);
    setState(233);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 6610055335936) != 0)) {
      setState(230);
      symbolOption();
      setState(235);
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
    setState(241);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__11:
      case TracerCommandsParser::T__24:
      case TracerCommandsParser::T__25:
      case TracerCommandsParser::T__31:
      case TracerCommandsParser::T__40: {
        enterOuterAlt(_localctx, 1);
        setState(236);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 2203418890240) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(237);
        value();
        break;
      }

      case TracerCommandsParser::T__32: {
        enterOuterAlt(_localctx, 2);
        setState(238);
        match(TracerCommandsParser::T__32);
        setState(239);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__41: {
        enterOuterAlt(_localctx, 3);
        setState(240);
        match(TracerCommandsParser::T__41);
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
    setState(295);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__42: {
        enterOuterAlt(_localctx, 1);
        setState(243);
        match(TracerCommandsParser::T__42);
        setState(289);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__43: {
            setState(244);
            match(TracerCommandsParser::T__43);
            setState(245);
            value();
            setState(247);
            _errHandler->sync(this);
            _la = _input->LA(1);
            do {
              setState(246);
              prepareOption();
              setState(249);
              _errHandler->sync(this);
              _la = _input->LA(1);
            } while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 69806344147828736) != 0) || ((((_la - 167) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 167)) & 1827) != 0));
            break;
          }

          case TracerCommandsParser::T__44: {
            setState(251);
            match(TracerCommandsParser::T__44);
            setState(252);
            value();
            setState(257);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 520165756994846720) != 0) || ((((_la - 167) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 167)) & 259) != 0)) {
              setState(255);
              _errHandler->sync(this);
              switch (_input->LA(1)) {
                case TracerCommandsParser::T__24:
                case TracerCommandsParser::T__50:
                case TracerCommandsParser::T__51:
                case TracerCommandsParser::T__52:
                case TracerCommandsParser::T__55:
                case TracerCommandsParser::T__56:
                case TracerCommandsParser::T__57:
                case TracerCommandsParser::T__166:
                case TracerCommandsParser::T__167: {
                  setState(253);
                  resolveOption();
                  break;
                }

                case TracerCommandsParser::CWD: {
                  setState(254);
                  cwdOption();
                  break;
                }

              default:
                throw NoViableAltException(this);
              }
              setState(259);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__45: {
            setState(260);
            match(TracerCommandsParser::T__45);
            setState(261);
            value();
            setState(264);
            _errHandler->sync(this);

            _la = _input->LA(1);
            if (_la == TracerCommandsParser::T__46) {
              setState(262);
              match(TracerCommandsParser::T__46);
              setState(263);
              value();
            }
            setState(270);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while (((((_la - 25) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 25)) & 1097833906177) != 0) || ((((_la - 164) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 164)) & 18457) != 0)) {
              setState(268);
              _errHandler->sync(this);
              switch (_input->LA(1)) {
                case TracerCommandsParser::T__24:
                case TracerCommandsParser::T__50:
                case TracerCommandsParser::T__51:
                case TracerCommandsParser::T__52:
                case TracerCommandsParser::T__55:
                case TracerCommandsParser::T__56:
                case TracerCommandsParser::T__57:
                case TracerCommandsParser::T__58:
                case TracerCommandsParser::T__59:
                case TracerCommandsParser::T__60:
                case TracerCommandsParser::T__61:
                case TracerCommandsParser::T__62:
                case TracerCommandsParser::T__63:
                case TracerCommandsParser::T__163:
                case TracerCommandsParser::T__166:
                case TracerCommandsParser::T__167:
                case TracerCommandsParser::SNAPSHOT_RETURN_POINTEE: {
                  setState(266);
                  deployOption();
                  break;
                }

                case TracerCommandsParser::CWD: {
                  setState(267);
                  cwdOption();
                  break;
                }

              default:
                throw NoViableAltException(this);
              }
              setState(272);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__47: {
            setState(273);
            match(TracerCommandsParser::T__47);
            setState(274);
            value();
            setState(275);
            match(TracerCommandsParser::T__46);
            setState(276);
            value();
            setState(281);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 520165756994846720) != 0) || ((((_la - 167) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 167)) & 259) != 0)) {
              setState(279);
              _errHandler->sync(this);
              switch (_input->LA(1)) {
                case TracerCommandsParser::T__24:
                case TracerCommandsParser::T__50:
                case TracerCommandsParser::T__51:
                case TracerCommandsParser::T__52:
                case TracerCommandsParser::T__55:
                case TracerCommandsParser::T__56:
                case TracerCommandsParser::T__57:
                case TracerCommandsParser::T__166:
                case TracerCommandsParser::T__167: {
                  setState(277);
                  resolveOption();
                  break;
                }

                case TracerCommandsParser::CWD: {
                  setState(278);
                  cwdOption();
                  break;
                }

              default:
                throw NoViableAltException(this);
              }
              setState(283);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__48: {
            setState(284);
            match(TracerCommandsParser::T__48);
            setState(285);
            match(TracerCommandsParser::UINT);
            setState(287);
            _errHandler->sync(this);

            _la = _input->LA(1);
            if (_la == TracerCommandsParser::CWD) {
              setState(286);
              cwdOption();
            }
            break;
          }

        default:
          throw NoViableAltException(this);
        }
        break;
      }

      case TracerCommandsParser::T__49: {
        enterOuterAlt(_localctx, 2);
        setState(291);
        match(TracerCommandsParser::T__49);
        setState(293);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::CWD) {
          setState(292);
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
    setState(305);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__24:
      case TracerCommandsParser::T__26:
      case TracerCommandsParser::T__38:
      case TracerCommandsParser::T__50:
      case TracerCommandsParser::T__51:
      case TracerCommandsParser::T__52:
      case TracerCommandsParser::T__53:
      case TracerCommandsParser::T__54: {
        enterOuterAlt(_localctx, 1);
        setState(297);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 69806344147828736) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(298);
        value();
        break;
      }

      case TracerCommandsParser::SIGNATURE: {
        enterOuterAlt(_localctx, 2);
        setState(299);
        match(TracerCommandsParser::SIGNATURE);
        setState(300);
        value();
        break;
      }

      case TracerCommandsParser::MEMBER: {
        enterOuterAlt(_localctx, 3);
        setState(301);
        match(TracerCommandsParser::MEMBER);
        break;
      }

      case TracerCommandsParser::METADATA_ONLY: {
        enterOuterAlt(_localctx, 4);
        setState(302);
        match(TracerCommandsParser::METADATA_ONLY);
        break;
      }

      case TracerCommandsParser::T__166:
      case TracerCommandsParser::T__167: {
        enterOuterAlt(_localctx, 5);
        setState(303);
        extraCompileOption();
        break;
      }

      case TracerCommandsParser::CWD: {
        enterOuterAlt(_localctx, 6);
        setState(304);
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
  enterRule(_localctx, 42, TracerCommandsParser::RuleResolveOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(310);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__24:
      case TracerCommandsParser::T__50:
      case TracerCommandsParser::T__51:
      case TracerCommandsParser::T__52:
      case TracerCommandsParser::T__55:
      case TracerCommandsParser::T__56:
      case TracerCommandsParser::T__57: {
        enterOuterAlt(_localctx, 1);
        setState(307);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 520165756994846720) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(308);
        value();
        break;
      }

      case TracerCommandsParser::T__166:
      case TracerCommandsParser::T__167: {
        enterOuterAlt(_localctx, 2);
        setState(309);
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

//----------------- DeployOptionContext ------------------------------------------------------------------

TracerCommandsParser::DeployOptionContext::DeployOptionContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

TracerCommandsParser::ResolveOptionContext* TracerCommandsParser::DeployOptionContext::resolveOption() {
  return getRuleContext<TracerCommandsParser::ResolveOptionContext>(0);
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

TracerCommandsParser::ValueContext* TracerCommandsParser::DeployOptionContext::value() {
  return getRuleContext<TracerCommandsParser::ValueContext>(0);
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
  enterRule(_localctx, 44, TracerCommandsParser::RuleDeployOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(322);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__24:
      case TracerCommandsParser::T__50:
      case TracerCommandsParser::T__51:
      case TracerCommandsParser::T__52:
      case TracerCommandsParser::T__55:
      case TracerCommandsParser::T__56:
      case TracerCommandsParser::T__57:
      case TracerCommandsParser::T__166:
      case TracerCommandsParser::T__167: {
        enterOuterAlt(_localctx, 1);
        setState(312);
        resolveOption();
        break;
      }

      case TracerCommandsParser::T__58: {
        enterOuterAlt(_localctx, 2);
        setState(313);
        match(TracerCommandsParser::T__58);
        setState(314);
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

      case TracerCommandsParser::T__59: {
        enterOuterAlt(_localctx, 3);
        setState(315);
        match(TracerCommandsParser::T__59);
        break;
      }

      case TracerCommandsParser::T__60: {
        enterOuterAlt(_localctx, 4);
        setState(316);
        match(TracerCommandsParser::T__60);
        break;
      }

      case TracerCommandsParser::SNAPSHOT_RETURN_POINTEE: {
        enterOuterAlt(_localctx, 5);
        setState(317);
        match(TracerCommandsParser::SNAPSHOT_RETURN_POINTEE);
        break;
      }

      case TracerCommandsParser::T__61: {
        enterOuterAlt(_localctx, 6);
        setState(318);
        match(TracerCommandsParser::T__61);
        break;
      }

      case TracerCommandsParser::T__163: {
        enterOuterAlt(_localctx, 7);
        setState(319);
        stackFilter();
        break;
      }

      case TracerCommandsParser::T__62:
      case TracerCommandsParser::T__63: {
        enterOuterAlt(_localctx, 8);
        setState(320);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__62

        || _la == TracerCommandsParser::T__63)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(321);
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

std::vector<TracerCommandsParser::ResolveOptionContext *> TracerCommandsParser::InterceptorContext::resolveOption() {
  return getRuleContexts<TracerCommandsParser::ResolveOptionContext>();
}

TracerCommandsParser::ResolveOptionContext* TracerCommandsParser::InterceptorContext::resolveOption(size_t i) {
  return getRuleContext<TracerCommandsParser::ResolveOptionContext>(i);
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
  enterRule(_localctx, 46, TracerCommandsParser::RuleInterceptor);
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
    match(TracerCommandsParser::T__64);
    setState(354);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__65: {
        setState(325);
        match(TracerCommandsParser::T__65);
        setState(326);
        value();
        setState(327);
        match(TracerCommandsParser::T__66);
        setState(328);
        value();
        break;
      }

      case TracerCommandsParser::T__67: {
        setState(330);
        match(TracerCommandsParser::T__67);
        setState(331);
        value();
        break;
      }

      case TracerCommandsParser::T__21: {
        setState(332);
        match(TracerCommandsParser::T__21);
        setState(333);
        value();
        break;
      }

      case TracerCommandsParser::T__47: {
        setState(334);
        match(TracerCommandsParser::T__47);
        setState(335);
        value();
        setState(336);
        match(TracerCommandsParser::T__68);
        setState(337);
        value();
        setState(341);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 520165756994846720) != 0) || _la == TracerCommandsParser::T__166

        || _la == TracerCommandsParser::T__167) {
          setState(338);
          resolveOption();
          setState(343);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__69: {
        setState(344);
        match(TracerCommandsParser::T__69);
        setState(345);
        value();
        setState(346);
        match(TracerCommandsParser::T__68);
        setState(347);
        value();
        setState(351);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (((((_la - 25) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 25)) & 1097833906177) != 0) || ((((_la - 164) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 164)) & 16409) != 0)) {
          setState(348);
          deployOption();
          setState(353);
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

std::vector<tree::TerminalNode *> TracerCommandsParser::EventsContext::UINT() {
  return getTokens(TracerCommandsParser::UINT);
}

tree::TerminalNode* TracerCommandsParser::EventsContext::UINT(size_t i) {
  return getToken(TracerCommandsParser::UINT, i);
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
  enterRule(_localctx, 48, TracerCommandsParser::RuleEvents);
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
    setState(356);
    match(TracerCommandsParser::T__70);
    setState(410);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::EOF:
      case TracerCommandsParser::T__66:
      case TracerCommandsParser::T__83:
      case TracerCommandsParser::T__84:
      case TracerCommandsParser::T__85:
      case TracerCommandsParser::T__86:
      case TracerCommandsParser::T__87:
      case TracerCommandsParser::T__90:
      case TracerCommandsParser::T__91:
      case TracerCommandsParser::T__92: {
        setState(360);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (((((_la - 67) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 67)) & 121503745) != 0)) {
          setState(357);
          streamOption();
          setState(362);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__71: {
        setState(363);
        match(TracerCommandsParser::T__71);
        setState(364);
        value();
        setState(367);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__32) {
          setState(365);
          match(TracerCommandsParser::T__32);
          setState(366);
          match(TracerCommandsParser::UINT);
        }
        break;
      }

      case TracerCommandsParser::T__21: {
        setState(369);
        match(TracerCommandsParser::T__21);
        setState(370);
        value();
        setState(371);
        match(TracerCommandsParser::T__30);
        setState(372);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__72: {
        setState(374);
        match(TracerCommandsParser::T__72);
        setState(375);
        value();
        setState(376);
        match(TracerCommandsParser::T__73);
        setState(377);
        match(TracerCommandsParser::UINT);
        setState(378);
        match(TracerCommandsParser::T__74);
        setState(379);
        match(TracerCommandsParser::UINT);
        setState(384);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__75) {
          setState(380);
          match(TracerCommandsParser::T__75);
          setState(381);
          value();
          setState(386);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::STACK: {
        setState(387);
        match(TracerCommandsParser::STACK);
        setState(388);
        value();
        setState(389);
        match(TracerCommandsParser::T__30);
        setState(390);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__76: {
        setState(392);
        match(TracerCommandsParser::T__76);
        setState(393);
        value();
        break;
      }

      case TracerCommandsParser::T__77: {
        setState(394);
        match(TracerCommandsParser::T__77);
        setState(395);
        value();
        break;
      }

      case TracerCommandsParser::T__78: {
        setState(396);
        match(TracerCommandsParser::T__78);
        setState(397);
        value();
        break;
      }

      case TracerCommandsParser::T__79: {
        setState(398);
        match(TracerCommandsParser::T__79);
        setState(399);
        value();
        setState(400);
        match(TracerCommandsParser::T__66);
        setState(401);
        value();
        break;
      }

      case TracerCommandsParser::T__80: {
        setState(403);
        match(TracerCommandsParser::T__80);
        setState(404);
        value();
        setState(405);
        match(TracerCommandsParser::T__33);
        setState(406);
        match(TracerCommandsParser::T__34);
        setState(407);
        match(TracerCommandsParser::T__26);
        setState(408);
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
  enterRule(_localctx, 50, TracerCommandsParser::RuleCollect);
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
    setState(412);
    match(TracerCommandsParser::T__81);
    setState(416);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (((((_la - 67) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 67)) & 121503745) != 0)) {
      setState(413);
      streamOption();
      setState(418);
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
  enterRule(_localctx, 52, TracerCommandsParser::RuleRecord);
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
    setState(419);
    match(TracerCommandsParser::T__82);
    setState(420);
    value();
    setState(425);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (((((_la - 67) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 67)) & 121503745) != 0) || _la == TracerCommandsParser::SQLITE) {
      setState(423);
      _errHandler->sync(this);
      switch (_input->LA(1)) {
        case TracerCommandsParser::T__66:
        case TracerCommandsParser::T__83:
        case TracerCommandsParser::T__84:
        case TracerCommandsParser::T__85:
        case TracerCommandsParser::T__86:
        case TracerCommandsParser::T__87:
        case TracerCommandsParser::T__90:
        case TracerCommandsParser::T__91:
        case TracerCommandsParser::T__92: {
          setState(421);
          streamOption();
          break;
        }

        case TracerCommandsParser::SQLITE: {
          setState(422);
          match(TracerCommandsParser::SQLITE);
          break;
        }

      default:
        throw NoViableAltException(this);
      }
      setState(427);
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
  enterRule(_localctx, 54, TracerCommandsParser::RuleStreamOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(435);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__83:
      case TracerCommandsParser::T__84:
      case TracerCommandsParser::T__85:
      case TracerCommandsParser::T__86: {
        enterOuterAlt(_localctx, 1);
        setState(428);
        _la = _input->LA(1);
        if (!(((((_la - 84) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 84)) & 15) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(429);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__87: {
        enterOuterAlt(_localctx, 2);
        setState(430);
        match(TracerCommandsParser::T__87);
        setState(431);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__88

        || _la == TracerCommandsParser::T__89)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        break;
      }

      case TracerCommandsParser::T__66:
      case TracerCommandsParser::T__90:
      case TracerCommandsParser::T__91: {
        enterOuterAlt(_localctx, 3);
        setState(432);
        _la = _input->LA(1);
        if (!(((((_la - 67) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 67)) & 50331649) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(433);
        value();
        break;
      }

      case TracerCommandsParser::T__92: {
        enterOuterAlt(_localctx, 4);
        setState(434);
        match(TracerCommandsParser::T__92);
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
  enterRule(_localctx, 56, TracerCommandsParser::RuleCpp);
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
    setState(437);
    match(TracerCommandsParser::T__93);
    setState(466);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__65: {
        setState(438);
        match(TracerCommandsParser::T__65);
        setState(439);
        value();
        break;
      }

      case TracerCommandsParser::T__94: {
        setState(440);
        match(TracerCommandsParser::T__94);
        break;
      }

      case TracerCommandsParser::T__95:
      case TracerCommandsParser::T__96: {
        setState(441);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__95

        || _la == TracerCommandsParser::T__96)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(442);
        value();
        setState(447);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__26

        || _la == TracerCommandsParser::T__36) {
          setState(443);
          _la = _input->LA(1);
          if (!(_la == TracerCommandsParser::T__26

          || _la == TracerCommandsParser::T__36)) {
          _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(444);
          value();
          setState(449);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__97: {
        setState(450);
        match(TracerCommandsParser::T__97);
        setState(464);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__98: {
            setState(451);
            match(TracerCommandsParser::T__98);
            setState(454);
            _errHandler->sync(this);
            _la = _input->LA(1);
            do {
              setState(454);
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
                case TracerCommandsParser::T__17:
                case TracerCommandsParser::T__18:
                case TracerCommandsParser::T__19:
                case TracerCommandsParser::T__20:
                case TracerCommandsParser::T__21:
                case TracerCommandsParser::T__22:
                case TracerCommandsParser::T__34:
                case TracerCommandsParser::T__39:
                case TracerCommandsParser::T__42:
                case TracerCommandsParser::T__43:
                case TracerCommandsParser::T__44:
                case TracerCommandsParser::T__45:
                case TracerCommandsParser::T__47:
                case TracerCommandsParser::T__48:
                case TracerCommandsParser::T__49:
                case TracerCommandsParser::T__64:
                case TracerCommandsParser::T__65:
                case TracerCommandsParser::T__67:
                case TracerCommandsParser::T__69:
                case TracerCommandsParser::T__70:
                case TracerCommandsParser::T__71:
                case TracerCommandsParser::T__76:
                case TracerCommandsParser::T__77:
                case TracerCommandsParser::T__78:
                case TracerCommandsParser::T__79:
                case TracerCommandsParser::T__80:
                case TracerCommandsParser::T__81:
                case TracerCommandsParser::T__82:
                case TracerCommandsParser::T__88:
                case TracerCommandsParser::T__89:
                case TracerCommandsParser::T__93:
                case TracerCommandsParser::T__94:
                case TracerCommandsParser::T__95:
                case TracerCommandsParser::T__96:
                case TracerCommandsParser::T__97:
                case TracerCommandsParser::T__98:
                case TracerCommandsParser::T__99:
                case TracerCommandsParser::T__100:
                case TracerCommandsParser::T__101:
                case TracerCommandsParser::T__102:
                case TracerCommandsParser::T__103:
                case TracerCommandsParser::T__104:
                case TracerCommandsParser::T__105:
                case TracerCommandsParser::T__106:
                case TracerCommandsParser::T__107:
                case TracerCommandsParser::T__108:
                case TracerCommandsParser::T__113:
                case TracerCommandsParser::T__114:
                case TracerCommandsParser::T__115:
                case TracerCommandsParser::T__116:
                case TracerCommandsParser::T__117:
                case TracerCommandsParser::T__118:
                case TracerCommandsParser::T__119:
                case TracerCommandsParser::UINT:
                case TracerCommandsParser::STRING:
                case TracerCommandsParser::STACK:
                case TracerCommandsParser::WORD: {
                  setState(452);
                  value();
                  break;
                }

                case TracerCommandsParser::SHELL: {
                  setState(453);
                  match(TracerCommandsParser::SHELL);
                  break;
                }

              default:
                throw NoViableAltException(this);
              }
              setState(456);
              _errHandler->sync(this);
              _la = _input->LA(1);
            } while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 2103400120314492) != 0) || ((((_la - 65) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 65)) & 71529827970576619) != 0) || ((((_la - 169) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 169)) & 1063) != 0));
            break;
          }

          case TracerCommandsParser::T__78: {
            setState(458);
            match(TracerCommandsParser::T__78);
            break;
          }

          case TracerCommandsParser::T__99: {
            setState(459);
            match(TracerCommandsParser::T__99);
            setState(460);
            value();
            setState(461);
            match(TracerCommandsParser::T__23);
            setState(462);
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
  enterRule(_localctx, 58, TracerCommandsParser::RuleShell);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(468);
    match(TracerCommandsParser::T__100);
    setState(469);
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
  enterRule(_localctx, 60, TracerCommandsParser::RuleComplete);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(471);
    match(TracerCommandsParser::T__101);
    setState(472);
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
  enterRule(_localctx, 62, TracerCommandsParser::RuleType);

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
    match(TracerCommandsParser::T__102);
    setState(475);
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
  enterRule(_localctx, 64, TracerCommandsParser::RuleHelp);
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
    setState(486);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 2103400120314494) != 0) || ((((_la - 65) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 65)) & 71529827970576619) != 0) || ((((_la - 169) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 169)) & 1059) != 0)) {
      setState(479);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == TracerCommandsParser::T__0) {
        setState(478);
        match(TracerCommandsParser::T__0);
      }
      setState(482);
      _errHandler->sync(this);
      _la = _input->LA(1);
      do {
        setState(481);
        value();
        setState(484);
        _errHandler->sync(this);
        _la = _input->LA(1);
      } while ((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & 2103400120314492) != 0) || ((((_la - 65) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 65)) & 71529827970576619) != 0) || ((((_la - 169) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 169)) & 1059) != 0));
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
  enterRule(_localctx, 66, TracerCommandsParser::RuleQuit);
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
    setState(488);
    _la = _input->LA(1);
    if (!(_la == TracerCommandsParser::T__104

    || _la == TracerCommandsParser::T__105)) {
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
  enterRule(_localctx, 68, TracerCommandsParser::RuleBulkTrace);
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
    setState(490);
    match(TracerCommandsParser::T__106);
    setState(506);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__107: {
        setState(491);
        match(TracerCommandsParser::T__107);
        setState(493);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(492);
          bulkTraceOption();
          setState(495);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while (_la == TracerCommandsParser::T__24

        || _la == TracerCommandsParser::T__50 || ((((_la - 110) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 110)) & 15) != 0));
        break;
      }

      case TracerCommandsParser::T__108: {
        setState(497);
        match(TracerCommandsParser::T__108);
        setState(498);
        match(TracerCommandsParser::T__109);
        setState(499);
        value();
        setState(502);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__46) {
          setState(500);
          match(TracerCommandsParser::T__46);
          setState(501);
          value();
        }
        break;
      }

      case TracerCommandsParser::T__48: {
        setState(504);
        match(TracerCommandsParser::T__48);
        setState(505);
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
  enterRule(_localctx, 70, TracerCommandsParser::RuleBulkTraceOption);
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
    setState(508);
    _la = _input->LA(1);
    if (!(_la == TracerCommandsParser::T__24

    || _la == TracerCommandsParser::T__50 || ((((_la - 110) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 110)) & 15) != 0))) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
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
  enterRule(_localctx, 72, TracerCommandsParser::RuleValue);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(515);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::STRING: {
        enterOuterAlt(_localctx, 1);
        setState(511);
        match(TracerCommandsParser::STRING);
        break;
      }

      case TracerCommandsParser::WORD: {
        enterOuterAlt(_localctx, 2);
        setState(512);
        match(TracerCommandsParser::WORD);
        break;
      }

      case TracerCommandsParser::UINT: {
        enterOuterAlt(_localctx, 3);
        setState(513);
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
      case TracerCommandsParser::T__17:
      case TracerCommandsParser::T__18:
      case TracerCommandsParser::T__19:
      case TracerCommandsParser::T__20:
      case TracerCommandsParser::T__21:
      case TracerCommandsParser::T__22:
      case TracerCommandsParser::T__34:
      case TracerCommandsParser::T__39:
      case TracerCommandsParser::T__42:
      case TracerCommandsParser::T__43:
      case TracerCommandsParser::T__44:
      case TracerCommandsParser::T__45:
      case TracerCommandsParser::T__47:
      case TracerCommandsParser::T__48:
      case TracerCommandsParser::T__49:
      case TracerCommandsParser::T__64:
      case TracerCommandsParser::T__65:
      case TracerCommandsParser::T__67:
      case TracerCommandsParser::T__69:
      case TracerCommandsParser::T__70:
      case TracerCommandsParser::T__71:
      case TracerCommandsParser::T__76:
      case TracerCommandsParser::T__77:
      case TracerCommandsParser::T__78:
      case TracerCommandsParser::T__79:
      case TracerCommandsParser::T__80:
      case TracerCommandsParser::T__81:
      case TracerCommandsParser::T__82:
      case TracerCommandsParser::T__88:
      case TracerCommandsParser::T__89:
      case TracerCommandsParser::T__93:
      case TracerCommandsParser::T__94:
      case TracerCommandsParser::T__95:
      case TracerCommandsParser::T__96:
      case TracerCommandsParser::T__97:
      case TracerCommandsParser::T__98:
      case TracerCommandsParser::T__99:
      case TracerCommandsParser::T__100:
      case TracerCommandsParser::T__101:
      case TracerCommandsParser::T__102:
      case TracerCommandsParser::T__103:
      case TracerCommandsParser::T__104:
      case TracerCommandsParser::T__105:
      case TracerCommandsParser::T__106:
      case TracerCommandsParser::T__107:
      case TracerCommandsParser::T__108:
      case TracerCommandsParser::T__113:
      case TracerCommandsParser::T__114:
      case TracerCommandsParser::T__115:
      case TracerCommandsParser::T__116:
      case TracerCommandsParser::T__117:
      case TracerCommandsParser::T__118:
      case TracerCommandsParser::T__119:
      case TracerCommandsParser::STACK: {
        enterOuterAlt(_localctx, 4);
        setState(514);
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
  enterRule(_localctx, 74, TracerCommandsParser::RuleKeyword);
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
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 2103400120314492) != 0) || ((((_la - 65) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 65)) & 71529827970576619) != 0) || _la == TracerCommandsParser::STACK)) {
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

//----------------- ObjectWatchContext ------------------------------------------------------------------

TracerCommandsParser::ObjectWatchContext::ObjectWatchContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

TracerCommandsParser::ValueContext* TracerCommandsParser::ObjectWatchContext::value() {
  return getRuleContext<TracerCommandsParser::ValueContext>(0);
}

std::vector<TracerCommandsParser::ObjectOptionContext *> TracerCommandsParser::ObjectWatchContext::objectOption() {
  return getRuleContexts<TracerCommandsParser::ObjectOptionContext>();
}

TracerCommandsParser::ObjectOptionContext* TracerCommandsParser::ObjectWatchContext::objectOption(size_t i) {
  return getRuleContext<TracerCommandsParser::ObjectOptionContext>(i);
}

tree::TerminalNode* TracerCommandsParser::ObjectWatchContext::UINT() {
  return getToken(TracerCommandsParser::UINT, 0);
}


size_t TracerCommandsParser::ObjectWatchContext::getRuleIndex() const {
  return TracerCommandsParser::RuleObjectWatch;
}


std::any TracerCommandsParser::ObjectWatchContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitObjectWatch(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::ObjectWatchContext* TracerCommandsParser::objectWatch() {
  ObjectWatchContext *_localctx = _tracker.createInstance<ObjectWatchContext>(_ctx, getState());
  enterRule(_localctx, 76, TracerCommandsParser::RuleObjectWatch);
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
    setState(519);
    match(TracerCommandsParser::T__119);
    setState(539);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__43: {
        setState(520);
        match(TracerCommandsParser::T__43);
        setState(522);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 2103400120314492) != 0) || ((((_la - 65) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 65)) & 71529827970576619) != 0) || ((((_la - 169) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 169)) & 1059) != 0)) {
          setState(521);
          value();
        }
        setState(525);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(524);
          objectOption();
          setState(527);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & -8631148547175088000) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 64)) & -144115188073754623) != 0) || ((((_la - 128) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 128)) & 8589934591) != 0));
        break;
      }

      case TracerCommandsParser::T__2:
      case TracerCommandsParser::T__107: {
        setState(529);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__2 || _la == TracerCommandsParser::T__107)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(531);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(530);
          objectOption();
          setState(533);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & -8631148547175088000) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 64)) & -144115188073754623) != 0) || ((((_la - 128) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 128)) & 8589934591) != 0));
        break;
      }

      case TracerCommandsParser::T__4:
      case TracerCommandsParser::T__5: {
        setState(535);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__4

        || _la == TracerCommandsParser::T__5)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(537);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::UINT) {
          setState(536);
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

//----------------- ObjectOptionContext ------------------------------------------------------------------

TracerCommandsParser::ObjectOptionContext::ObjectOptionContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

TracerCommandsParser::ValueContext* TracerCommandsParser::ObjectOptionContext::value() {
  return getRuleContext<TracerCommandsParser::ValueContext>(0);
}

tree::TerminalNode* TracerCommandsParser::ObjectOptionContext::UINT() {
  return getToken(TracerCommandsParser::UINT, 0);
}


size_t TracerCommandsParser::ObjectOptionContext::getRuleIndex() const {
  return TracerCommandsParser::RuleObjectOption;
}


std::any TracerCommandsParser::ObjectOptionContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitObjectOption(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::ObjectOptionContext* TracerCommandsParser::objectOption() {
  ObjectOptionContext *_localctx = _tracker.createInstance<ObjectOptionContext>(_ctx, getState());
  enterRule(_localctx, 78, TracerCommandsParser::RuleObjectOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(552);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__6:
      case TracerCommandsParser::T__24:
      case TracerCommandsParser::T__26:
      case TracerCommandsParser::T__36:
      case TracerCommandsParser::T__50:
      case TracerCommandsParser::T__51:
      case TracerCommandsParser::T__52:
      case TracerCommandsParser::T__62:
      case TracerCommandsParser::T__63:
      case TracerCommandsParser::T__75:
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
      case TracerCommandsParser::T__142: {
        enterOuterAlt(_localctx, 1);
        setState(541);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & -9207609300552253312) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 64)) & -144115188075851775) != 0) || ((((_la - 128) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 128)) & 65535) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(542);
        value();
        break;
      }

      case TracerCommandsParser::T__29:
      case TracerCommandsParser::T__58:
      case TracerCommandsParser::T__84:
      case TracerCommandsParser::T__143:
      case TracerCommandsParser::T__144:
      case TracerCommandsParser::T__145:
      case TracerCommandsParser::T__146:
      case TracerCommandsParser::T__147:
      case TracerCommandsParser::T__148:
      case TracerCommandsParser::T__149:
      case TracerCommandsParser::T__150:
      case TracerCommandsParser::T__151:
      case TracerCommandsParser::T__152: {
        enterOuterAlt(_localctx, 2);
        setState(543);
        _la = _input->LA(1);
        if (!(((((_la - 30) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 30)) & 36028797555834881) != 0) || ((((_la - 144) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 144)) & 1023) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(544);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__153: {
        enterOuterAlt(_localctx, 3);
        setState(545);
        match(TracerCommandsParser::T__153);
        break;
      }

      case TracerCommandsParser::T__154: {
        enterOuterAlt(_localctx, 4);
        setState(546);
        match(TracerCommandsParser::T__154);
        break;
      }

      case TracerCommandsParser::T__155: {
        enterOuterAlt(_localctx, 5);
        setState(547);
        match(TracerCommandsParser::T__155);
        break;
      }

      case TracerCommandsParser::T__156: {
        enterOuterAlt(_localctx, 6);
        setState(548);
        match(TracerCommandsParser::T__156);
        break;
      }

      case TracerCommandsParser::T__157: {
        enterOuterAlt(_localctx, 7);
        setState(549);
        match(TracerCommandsParser::T__157);
        break;
      }

      case TracerCommandsParser::T__158: {
        enterOuterAlt(_localctx, 8);
        setState(550);
        match(TracerCommandsParser::T__158);
        break;
      }

      case TracerCommandsParser::T__159: {
        enterOuterAlt(_localctx, 9);
        setState(551);
        match(TracerCommandsParser::T__159);
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
    setState(554);
    match(TracerCommandsParser::T__113);
    setState(578);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__114: {
        setState(555);
        match(TracerCommandsParser::T__114);
        break;
      }

      case TracerCommandsParser::T__115: {
        setState(556);
        match(TracerCommandsParser::T__115);
        setState(557);
        value();
        setState(558);
        match(TracerCommandsParser::T__25);
        setState(559);
        value();
        setState(564);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__83: {
            setState(560);
            match(TracerCommandsParser::T__83);
            setState(561);
            match(TracerCommandsParser::UINT);
            break;
          }

          case TracerCommandsParser::T__36: {
            setState(562);
            match(TracerCommandsParser::T__36);
            setState(563);
            value();
            break;
          }

        default:
          throw NoViableAltException(this);
        }
        break;
      }

      case TracerCommandsParser::T__116: {
        setState(566);
        match(TracerCommandsParser::T__116);
        setState(567);
        value();
        setState(568);
        match(TracerCommandsParser::T__25);
        setState(569);
        value();
        setState(570);
        match(TracerCommandsParser::T__83);
        setState(571);
        match(TracerCommandsParser::UINT);
        setState(572);
        match(TracerCommandsParser::T__160);
        setState(573);
        value();
        setState(576);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__32) {
          setState(574);
          match(TracerCommandsParser::T__32);
          setState(575);
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
    setState(580);
    match(TracerCommandsParser::T__42);
    setState(581);
    match(TracerCommandsParser::T__117);
    setState(598);
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
      case TracerCommandsParser::T__17:
      case TracerCommandsParser::T__18:
      case TracerCommandsParser::T__19:
      case TracerCommandsParser::T__20:
      case TracerCommandsParser::T__21:
      case TracerCommandsParser::T__22:
      case TracerCommandsParser::T__34:
      case TracerCommandsParser::T__39:
      case TracerCommandsParser::T__42:
      case TracerCommandsParser::T__43:
      case TracerCommandsParser::T__44:
      case TracerCommandsParser::T__45:
      case TracerCommandsParser::T__47:
      case TracerCommandsParser::T__48:
      case TracerCommandsParser::T__49:
      case TracerCommandsParser::T__64:
      case TracerCommandsParser::T__65:
      case TracerCommandsParser::T__67:
      case TracerCommandsParser::T__69:
      case TracerCommandsParser::T__70:
      case TracerCommandsParser::T__71:
      case TracerCommandsParser::T__76:
      case TracerCommandsParser::T__77:
      case TracerCommandsParser::T__78:
      case TracerCommandsParser::T__79:
      case TracerCommandsParser::T__80:
      case TracerCommandsParser::T__81:
      case TracerCommandsParser::T__82:
      case TracerCommandsParser::T__88:
      case TracerCommandsParser::T__89:
      case TracerCommandsParser::T__93:
      case TracerCommandsParser::T__94:
      case TracerCommandsParser::T__95:
      case TracerCommandsParser::T__96:
      case TracerCommandsParser::T__97:
      case TracerCommandsParser::T__98:
      case TracerCommandsParser::T__99:
      case TracerCommandsParser::T__100:
      case TracerCommandsParser::T__101:
      case TracerCommandsParser::T__102:
      case TracerCommandsParser::T__103:
      case TracerCommandsParser::T__104:
      case TracerCommandsParser::T__105:
      case TracerCommandsParser::T__106:
      case TracerCommandsParser::T__107:
      case TracerCommandsParser::T__108:
      case TracerCommandsParser::T__113:
      case TracerCommandsParser::T__114:
      case TracerCommandsParser::T__115:
      case TracerCommandsParser::T__116:
      case TracerCommandsParser::T__117:
      case TracerCommandsParser::T__118:
      case TracerCommandsParser::T__119:
      case TracerCommandsParser::UINT:
      case TracerCommandsParser::STRING:
      case TracerCommandsParser::STACK:
      case TracerCommandsParser::WORD: {
        setState(582);
        value();
        setState(583);
        match(TracerCommandsParser::T__25);
        setState(584);
        value();
        setState(587);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__24) {
          setState(585);
          match(TracerCommandsParser::T__24);
          setState(586);
          value();
        }
        break;
      }

      case TracerCommandsParser::T__24: {
        setState(589);
        match(TracerCommandsParser::T__24);
        setState(590);
        value();
        setState(591);
        match(TracerCommandsParser::T__161);
        setState(592);
        value();
        setState(593);
        match(TracerCommandsParser::T__162);
        setState(594);
        value();
        setState(595);
        match(TracerCommandsParser::T__25);
        setState(596);
        value();
        break;
      }

    default:
      throw NoViableAltException(this);
    }
    setState(601);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::CWD) {
      setState(600);
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

tree::TerminalNode* TracerCommandsParser::ProbeReloadContext::UINT() {
  return getToken(TracerCommandsParser::UINT, 0);
}

TracerCommandsParser::ValueContext* TracerCommandsParser::ProbeReloadContext::value() {
  return getRuleContext<TracerCommandsParser::ValueContext>(0);
}

TracerCommandsParser::CwdOptionContext* TracerCommandsParser::ProbeReloadContext::cwdOption() {
  return getRuleContext<TracerCommandsParser::CwdOptionContext>(0);
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
    setState(603);
    match(TracerCommandsParser::T__42);
    setState(604);
    match(TracerCommandsParser::T__118);
    setState(605);
    match(TracerCommandsParser::UINT);
    setState(606);
    match(TracerCommandsParser::T__46);
    setState(607);
    value();
    setState(609);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::CWD) {
      setState(608);
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
  enterRule(_localctx, 86, TracerCommandsParser::RuleStackFilter);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(611);
    match(TracerCommandsParser::T__163);
    setState(612);
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
  enterRule(_localctx, 88, TracerCommandsParser::RuleContextEventBytes);
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
    setState(614);
    _la = _input->LA(1);
    if (!(_la == TracerCommandsParser::T__164

    || _la == TracerCommandsParser::T__165)) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(615);
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
  enterRule(_localctx, 90, TracerCommandsParser::RuleExtraCompileOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(626);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__166: {
        enterOuterAlt(_localctx, 1);
        setState(617);
        match(TracerCommandsParser::T__166);
        setState(618);
        value();
        break;
      }

      case TracerCommandsParser::T__167: {
        enterOuterAlt(_localctx, 2);
        setState(619);
        match(TracerCommandsParser::T__167);
        setState(622);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(622);
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
            case TracerCommandsParser::T__17:
            case TracerCommandsParser::T__18:
            case TracerCommandsParser::T__19:
            case TracerCommandsParser::T__20:
            case TracerCommandsParser::T__21:
            case TracerCommandsParser::T__22:
            case TracerCommandsParser::T__34:
            case TracerCommandsParser::T__39:
            case TracerCommandsParser::T__42:
            case TracerCommandsParser::T__43:
            case TracerCommandsParser::T__44:
            case TracerCommandsParser::T__45:
            case TracerCommandsParser::T__47:
            case TracerCommandsParser::T__48:
            case TracerCommandsParser::T__49:
            case TracerCommandsParser::T__64:
            case TracerCommandsParser::T__65:
            case TracerCommandsParser::T__67:
            case TracerCommandsParser::T__69:
            case TracerCommandsParser::T__70:
            case TracerCommandsParser::T__71:
            case TracerCommandsParser::T__76:
            case TracerCommandsParser::T__77:
            case TracerCommandsParser::T__78:
            case TracerCommandsParser::T__79:
            case TracerCommandsParser::T__80:
            case TracerCommandsParser::T__81:
            case TracerCommandsParser::T__82:
            case TracerCommandsParser::T__88:
            case TracerCommandsParser::T__89:
            case TracerCommandsParser::T__93:
            case TracerCommandsParser::T__94:
            case TracerCommandsParser::T__95:
            case TracerCommandsParser::T__96:
            case TracerCommandsParser::T__97:
            case TracerCommandsParser::T__98:
            case TracerCommandsParser::T__99:
            case TracerCommandsParser::T__100:
            case TracerCommandsParser::T__101:
            case TracerCommandsParser::T__102:
            case TracerCommandsParser::T__103:
            case TracerCommandsParser::T__104:
            case TracerCommandsParser::T__105:
            case TracerCommandsParser::T__106:
            case TracerCommandsParser::T__107:
            case TracerCommandsParser::T__108:
            case TracerCommandsParser::T__113:
            case TracerCommandsParser::T__114:
            case TracerCommandsParser::T__115:
            case TracerCommandsParser::T__116:
            case TracerCommandsParser::T__117:
            case TracerCommandsParser::T__118:
            case TracerCommandsParser::T__119:
            case TracerCommandsParser::UINT:
            case TracerCommandsParser::STRING:
            case TracerCommandsParser::STACK:
            case TracerCommandsParser::WORD: {
              setState(620);
              value();
              break;
            }

            case TracerCommandsParser::SHELL: {
              setState(621);
              match(TracerCommandsParser::SHELL);
              break;
            }

          default:
            throw NoViableAltException(this);
          }
          setState(624);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 2103400120314492) != 0) || ((((_la - 65) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 65)) & 71529827970576619) != 0) || ((((_la - 169) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 169)) & 1063) != 0));
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
  enterRule(_localctx, 92, TracerCommandsParser::RuleCwdOption);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(628);
    match(TracerCommandsParser::CWD);
    setState(629);
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
