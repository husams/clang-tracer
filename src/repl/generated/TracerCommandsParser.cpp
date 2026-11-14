
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
      "value", "keyword", "liveUpdate", "liveOption", "objectWatch", "objectOption",
      "memory", "probeEntry", "probeReload", "stackFilter", "contextEventBytes",
      "extraCompileOption", "cwdOption"
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
      "'maps'", "'read'", "'scan'", "'entry'", "'reload'", "'object'", "'live'",
      "'update'", "'revert'", "'mock'", "'update-data'", "'scan-heap'",
      "'--build'", "'--function'", "'--fixture'", "'--bind'", "'--recursion'",
      "'--discover-file'", "'--discover-function'", "'--from-scan'", "'--after-file'",
      "'--after-function'", "'--for-each'", "'--retention-limit'", "'--base'",
      "'--owner'", "'--field'", "'--collection'", "'--owner-add'", "'--owner-get'",
      "'--owner-remove'", "'--owner-snapshot'", "'--snapshot-field'", "'--code'",
      "'--plan'", "'--mode'", "'--policy'", "'--on'", "'--emit-source'",
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
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
      "", "", "", "", "UINT", "STRING", "SHELL", "METADATA_ONLY", "SQLITE",
      "STACK", "CWD", "SIGNATURE", "MEMBER", "SNAPSHOT_RETURN_POINTEE",
      "WORD", "WS"
    }
  );
  static const int32_t serializedATNSegment[] = {
    4,1,202,679,2,0,7,0,2,1,7,1,2,2,7,2,2,3,7,3,2,4,7,4,2,5,7,5,2,6,7,6,2,
    7,7,7,2,8,7,8,2,9,7,9,2,10,7,10,2,11,7,11,2,12,7,12,2,13,7,13,2,14,7,
    14,2,15,7,15,2,16,7,16,2,17,7,17,2,18,7,18,2,19,7,19,2,20,7,20,2,21,7,
    21,2,22,7,22,2,23,7,23,2,24,7,24,2,25,7,25,2,26,7,26,2,27,7,27,2,28,7,
    28,2,29,7,29,2,30,7,30,2,31,7,31,2,32,7,32,2,33,7,33,2,34,7,34,2,35,7,
    35,2,36,7,36,2,37,7,37,2,38,7,38,2,39,7,39,2,40,7,40,2,41,7,41,2,42,7,
    42,2,43,7,43,2,44,7,44,2,45,7,45,2,46,7,46,2,47,7,47,2,48,7,48,1,0,1,
    0,1,0,1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,3,1,131,8,1,1,2,1,
    2,1,2,5,2,136,8,2,10,2,12,2,139,9,2,1,2,1,2,1,2,1,2,3,2,145,8,2,1,3,1,
    3,1,3,3,3,150,8,3,1,4,1,4,1,4,1,4,3,4,156,8,4,3,4,158,8,4,1,5,1,5,1,5,
    3,5,163,8,5,1,6,1,6,1,7,1,7,1,7,1,8,1,8,1,9,1,9,3,9,174,8,9,1,10,1,10,
    3,10,178,8,10,1,11,1,11,1,11,4,11,183,8,11,11,11,12,11,184,1,11,1,11,
    1,11,1,11,5,11,191,8,11,10,11,12,11,194,9,11,1,11,1,11,4,11,198,8,11,
    11,11,12,11,199,1,11,1,11,4,11,204,8,11,11,11,12,11,205,3,11,208,8,11,
    1,12,1,12,1,12,1,12,3,12,214,8,12,1,13,1,13,1,13,1,13,1,13,1,13,1,13,
    3,13,223,8,13,1,14,1,14,1,14,1,15,1,15,1,15,3,15,231,8,15,1,16,1,16,1,
    17,1,17,5,17,237,8,17,10,17,12,17,240,9,17,1,18,1,18,1,18,1,18,1,18,3,
    18,247,8,18,1,19,1,19,1,19,1,19,4,19,253,8,19,11,19,12,19,254,1,19,1,
    19,1,19,1,19,5,19,261,8,19,10,19,12,19,264,9,19,1,19,1,19,1,19,1,19,3,
    19,270,8,19,1,19,1,19,5,19,274,8,19,10,19,12,19,277,9,19,1,19,1,19,1,
    19,1,19,1,19,1,19,5,19,285,8,19,10,19,12,19,288,9,19,1,19,1,19,1,19,3,
    19,293,8,19,3,19,295,8,19,1,19,1,19,3,19,299,8,19,3,19,301,8,19,1,20,
    1,20,1,20,1,20,1,20,1,20,1,20,1,20,3,20,311,8,20,1,21,1,21,1,21,3,21,
    316,8,21,1,22,1,22,1,22,1,22,1,22,1,22,1,22,1,22,1,22,1,22,3,22,328,8,
    22,1,23,1,23,1,23,1,23,1,23,1,23,1,23,1,23,1,23,1,23,1,23,1,23,1,23,1,
    23,1,23,5,23,345,8,23,10,23,12,23,348,9,23,1,23,1,23,1,23,1,23,1,23,5,
    23,355,8,23,10,23,12,23,358,9,23,3,23,360,8,23,1,24,1,24,5,24,364,8,24,
    10,24,12,24,367,9,24,1,24,1,24,1,24,1,24,3,24,373,8,24,1,24,1,24,1,24,
    1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,5,24,388,8,24,10,24,
    12,24,391,9,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,
    1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,3,24,416,
    8,24,1,25,1,25,5,25,420,8,25,10,25,12,25,423,9,25,1,26,1,26,1,26,1,26,
    5,26,429,8,26,10,26,12,26,432,9,26,1,27,1,27,1,27,1,27,1,27,1,27,1,27,
    3,27,441,8,27,1,28,1,28,1,28,1,28,1,28,1,28,1,28,1,28,5,28,451,8,28,10,
    28,12,28,454,9,28,1,28,1,28,1,28,1,28,4,28,460,8,28,11,28,12,28,461,1,
    28,1,28,1,28,1,28,1,28,1,28,3,28,470,8,28,3,28,472,8,28,1,29,1,29,1,29,
    1,30,1,30,1,30,1,31,1,31,1,31,1,32,1,32,3,32,485,8,32,1,32,4,32,488,8,
    32,11,32,12,32,489,3,32,492,8,32,1,33,1,33,1,34,1,34,1,34,4,34,499,8,
    34,11,34,12,34,500,1,34,1,34,1,34,1,34,1,34,3,34,508,8,34,1,34,1,34,3,
    34,512,8,34,1,35,1,35,1,35,1,36,1,36,1,36,1,36,3,36,521,8,36,1,37,1,37,
    1,38,1,38,1,38,1,38,4,38,529,8,38,11,38,12,38,530,1,38,1,38,1,38,3,38,
    536,8,38,1,38,1,38,1,38,4,38,541,8,38,11,38,12,38,542,1,38,1,38,1,38,
    5,38,548,8,38,10,38,12,38,551,9,38,1,38,1,38,4,38,555,8,38,11,38,12,38,
    556,3,38,559,8,38,1,39,1,39,1,39,1,39,3,39,565,8,39,1,40,1,40,1,40,3,
    40,570,8,40,1,40,4,40,573,8,40,11,40,12,40,574,1,40,1,40,4,40,579,8,40,
    11,40,12,40,580,1,40,1,40,3,40,585,8,40,3,40,587,8,40,1,41,1,41,1,41,
    1,41,1,41,1,41,1,41,1,41,1,41,1,41,1,41,3,41,600,8,41,1,42,1,42,1,42,
    1,42,1,42,1,42,1,42,1,42,1,42,1,42,3,42,612,8,42,1,42,1,42,1,42,1,42,
    1,42,1,42,1,42,1,42,1,42,1,42,3,42,624,8,42,3,42,626,8,42,1,43,1,43,1,
    43,1,43,1,43,1,43,1,43,3,43,635,8,43,1,43,1,43,1,43,1,43,1,43,1,43,1,
    43,1,43,1,43,3,43,646,8,43,1,43,3,43,649,8,43,1,44,1,44,1,44,1,44,1,44,
    1,44,3,44,657,8,44,1,45,1,45,1,45,1,46,1,46,1,46,1,47,1,47,1,47,1,47,
    1,47,4,47,670,8,47,11,47,12,47,671,3,47,674,8,47,1,48,1,48,1,48,1,48,
    0,0,49,0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,34,36,38,40,42,44,
    46,48,50,52,54,56,58,60,62,64,66,68,70,72,74,76,78,80,82,84,86,88,90,
    92,94,96,0,26,1,0,7,8,1,0,20,21,1,0,24,28,1,0,29,30,2,0,12,12,31,32,2,
    0,31,31,37,39,4,0,12,12,25,26,32,32,41,41,4,0,25,25,27,27,39,39,51,55,
    3,0,25,25,51,53,56,58,2,0,191,191,201,201,1,0,63,64,1,0,84,87,1,0,89,
    90,2,0,67,67,91,92,1,0,96,97,2,0,27,27,37,37,1,0,105,106,3,0,25,25,51,
    51,110,113,17,0,2,6,9,9,11,11,13,16,18,23,35,35,40,40,43,46,48,50,65,
    66,68,68,70,72,77,83,89,90,94,109,114,126,196,196,2,0,108,108,122,122,
    5,0,24,25,37,37,41,41,52,52,127,137,2,0,3,3,108,108,1,0,5,6,8,0,7,7,25,
    25,27,27,37,37,51,53,63,64,76,76,139,165,4,0,30,30,59,59,85,85,166,175,
    1,0,187,188,790,0,98,1,0,0,0,2,130,1,0,0,0,4,132,1,0,0,0,6,149,1,0,0,
    0,8,151,1,0,0,0,10,159,1,0,0,0,12,164,1,0,0,0,14,166,1,0,0,0,16,169,1,
    0,0,0,18,171,1,0,0,0,20,175,1,0,0,0,22,179,1,0,0,0,24,213,1,0,0,0,26,
    222,1,0,0,0,28,224,1,0,0,0,30,230,1,0,0,0,32,232,1,0,0,0,34,234,1,0,0,
    0,36,246,1,0,0,0,38,300,1,0,0,0,40,310,1,0,0,0,42,315,1,0,0,0,44,327,
    1,0,0,0,46,329,1,0,0,0,48,361,1,0,0,0,50,417,1,0,0,0,52,424,1,0,0,0,54,
    440,1,0,0,0,56,442,1,0,0,0,58,473,1,0,0,0,60,476,1,0,0,0,62,479,1,0,0,
    0,64,482,1,0,0,0,66,493,1,0,0,0,68,495,1,0,0,0,70,513,1,0,0,0,72,520,
    1,0,0,0,74,522,1,0,0,0,76,524,1,0,0,0,78,564,1,0,0,0,80,566,1,0,0,0,82,
    599,1,0,0,0,84,601,1,0,0,0,86,627,1,0,0,0,88,650,1,0,0,0,90,658,1,0,0,
    0,92,661,1,0,0,0,94,673,1,0,0,0,96,675,1,0,0,0,98,99,5,1,0,0,99,100,3,
    2,1,0,100,101,5,0,0,1,101,1,1,0,0,0,102,131,3,4,2,0,103,131,3,8,4,0,104,
    131,3,10,5,0,105,131,3,12,6,0,106,131,3,14,7,0,107,131,3,16,8,0,108,131,
    3,18,9,0,109,131,3,20,10,0,110,131,3,22,11,0,111,131,3,32,16,0,112,131,
    3,34,17,0,113,131,3,38,19,0,114,131,3,46,23,0,115,131,3,48,24,0,116,131,
    3,50,25,0,117,131,3,52,26,0,118,131,3,56,28,0,119,131,3,58,29,0,120,131,
    3,60,30,0,121,131,3,62,31,0,122,131,3,64,32,0,123,131,3,66,33,0,124,131,
    3,68,34,0,125,131,3,84,42,0,126,131,3,86,43,0,127,131,3,88,44,0,128,131,
    3,80,40,0,129,131,3,76,38,0,130,102,1,0,0,0,130,103,1,0,0,0,130,104,1,
    0,0,0,130,105,1,0,0,0,130,106,1,0,0,0,130,107,1,0,0,0,130,108,1,0,0,0,
    130,109,1,0,0,0,130,110,1,0,0,0,130,111,1,0,0,0,130,112,1,0,0,0,130,113,
    1,0,0,0,130,114,1,0,0,0,130,115,1,0,0,0,130,116,1,0,0,0,130,117,1,0,0,
    0,130,118,1,0,0,0,130,119,1,0,0,0,130,120,1,0,0,0,130,121,1,0,0,0,130,
    122,1,0,0,0,130,123,1,0,0,0,130,124,1,0,0,0,130,125,1,0,0,0,130,126,1,
    0,0,0,130,127,1,0,0,0,130,128,1,0,0,0,130,129,1,0,0,0,131,3,1,0,0,0,132,
    144,5,2,0,0,133,137,5,3,0,0,134,136,3,6,3,0,135,134,1,0,0,0,136,139,1,
    0,0,0,137,135,1,0,0,0,137,138,1,0,0,0,138,145,1,0,0,0,139,137,1,0,0,0,
    140,141,5,4,0,0,141,145,3,72,36,0,142,145,5,5,0,0,143,145,5,6,0,0,144,
    133,1,0,0,0,144,140,1,0,0,0,144,142,1,0,0,0,144,143,1,0,0,0,145,5,1,0,
    0,0,146,147,7,0,0,0,147,150,3,72,36,0,148,150,3,92,46,0,149,146,1,0,0,
    0,149,148,1,0,0,0,150,7,1,0,0,0,151,157,5,9,0,0,152,155,5,191,0,0,153,
    154,5,10,0,0,154,156,5,191,0,0,155,153,1,0,0,0,155,156,1,0,0,0,156,158,
    1,0,0,0,157,152,1,0,0,0,157,158,1,0,0,0,158,9,1,0,0,0,159,162,5,11,0,
    0,160,161,5,12,0,0,161,163,3,72,36,0,162,160,1,0,0,0,162,163,1,0,0,0,
    163,11,1,0,0,0,164,165,5,13,0,0,165,13,1,0,0,0,166,167,5,14,0,0,167,168,
    3,72,36,0,168,15,1,0,0,0,169,170,5,5,0,0,170,17,1,0,0,0,171,173,5,15,
    0,0,172,174,3,72,36,0,173,172,1,0,0,0,173,174,1,0,0,0,174,19,1,0,0,0,
    175,177,5,16,0,0,176,178,5,17,0,0,177,176,1,0,0,0,177,178,1,0,0,0,178,
    21,1,0,0,0,179,207,5,18,0,0,180,182,5,19,0,0,181,183,3,24,12,0,182,181,
    1,0,0,0,183,184,1,0,0,0,184,182,1,0,0,0,184,185,1,0,0,0,185,208,1,0,0,
    0,186,187,5,14,0,0,187,208,3,72,36,0,188,192,7,1,0,0,189,191,3,26,13,
    0,190,189,1,0,0,0,191,194,1,0,0,0,192,190,1,0,0,0,192,193,1,0,0,0,193,
    208,1,0,0,0,194,192,1,0,0,0,195,197,5,22,0,0,196,198,3,28,14,0,197,196,
    1,0,0,0,198,199,1,0,0,0,199,197,1,0,0,0,199,200,1,0,0,0,200,208,1,0,0,
    0,201,203,5,23,0,0,202,204,3,30,15,0,203,202,1,0,0,0,204,205,1,0,0,0,
    205,203,1,0,0,0,205,206,1,0,0,0,206,208,1,0,0,0,207,180,1,0,0,0,207,186,
    1,0,0,0,207,188,1,0,0,0,207,195,1,0,0,0,207,201,1,0,0,0,208,23,1,0,0,
    0,209,210,7,2,0,0,210,214,3,72,36,0,211,212,7,3,0,0,212,214,5,191,0,0,
    213,209,1,0,0,0,213,211,1,0,0,0,214,25,1,0,0,0,215,216,7,4,0,0,216,223,
    3,72,36,0,217,218,5,33,0,0,218,223,5,191,0,0,219,220,5,34,0,0,220,223,
    5,35,0,0,221,223,5,36,0,0,222,215,1,0,0,0,222,217,1,0,0,0,222,219,1,0,
    0,0,222,221,1,0,0,0,223,27,1,0,0,0,224,225,7,5,0,0,225,226,3,72,36,0,
    226,29,1,0,0,0,227,231,3,28,14,0,228,229,5,27,0,0,229,231,3,72,36,0,230,
    227,1,0,0,0,230,228,1,0,0,0,231,31,1,0,0,0,232,233,5,40,0,0,233,33,1,
    0,0,0,234,238,5,21,0,0,235,237,3,36,18,0,236,235,1,0,0,0,237,240,1,0,
    0,0,238,236,1,0,0,0,238,239,1,0,0,0,239,35,1,0,0,0,240,238,1,0,0,0,241,
    242,7,6,0,0,242,247,3,72,36,0,243,244,5,33,0,0,244,247,5,191,0,0,245,
    247,5,42,0,0,246,241,1,0,0,0,246,243,1,0,0,0,246,245,1,0,0,0,247,37,1,
    0,0,0,248,294,5,43,0,0,249,250,5,44,0,0,250,252,3,72,36,0,251,253,3,40,
    20,0,252,251,1,0,0,0,253,254,1,0,0,0,254,252,1,0,0,0,254,255,1,0,0,0,
    255,295,1,0,0,0,256,257,5,45,0,0,257,262,3,72,36,0,258,261,3,42,21,0,
    259,261,3,96,48,0,260,258,1,0,0,0,260,259,1,0,0,0,261,264,1,0,0,0,262,
    260,1,0,0,0,262,263,1,0,0,0,263,295,1,0,0,0,264,262,1,0,0,0,265,266,5,
    46,0,0,266,269,3,72,36,0,267,268,5,47,0,0,268,270,3,72,36,0,269,267,1,
    0,0,0,269,270,1,0,0,0,270,275,1,0,0,0,271,274,3,44,22,0,272,274,3,96,
    48,0,273,271,1,0,0,0,273,272,1,0,0,0,274,277,1,0,0,0,275,273,1,0,0,0,
    275,276,1,0,0,0,276,295,1,0,0,0,277,275,1,0,0,0,278,279,5,48,0,0,279,
    280,3,72,36,0,280,281,5,47,0,0,281,286,3,72,36,0,282,285,3,42,21,0,283,
    285,3,96,48,0,284,282,1,0,0,0,284,283,1,0,0,0,285,288,1,0,0,0,286,284,
    1,0,0,0,286,287,1,0,0,0,287,295,1,0,0,0,288,286,1,0,0,0,289,290,5,49,
    0,0,290,292,5,191,0,0,291,293,3,96,48,0,292,291,1,0,0,0,292,293,1,0,0,
    0,293,295,1,0,0,0,294,249,1,0,0,0,294,256,1,0,0,0,294,265,1,0,0,0,294,
    278,1,0,0,0,294,289,1,0,0,0,295,301,1,0,0,0,296,298,5,50,0,0,297,299,
    3,96,48,0,298,297,1,0,0,0,298,299,1,0,0,0,299,301,1,0,0,0,300,248,1,0,
    0,0,300,296,1,0,0,0,301,39,1,0,0,0,302,303,7,7,0,0,303,311,3,72,36,0,
    304,305,5,198,0,0,305,311,3,72,36,0,306,311,5,199,0,0,307,311,5,194,0,
    0,308,311,3,94,47,0,309,311,3,96,48,0,310,302,1,0,0,0,310,304,1,0,0,0,
    310,306,1,0,0,0,310,307,1,0,0,0,310,308,1,0,0,0,310,309,1,0,0,0,311,41,
    1,0,0,0,312,313,7,8,0,0,313,316,3,72,36,0,314,316,3,94,47,0,315,312,1,
    0,0,0,315,314,1,0,0,0,316,43,1,0,0,0,317,328,3,42,21,0,318,319,5,59,0,
    0,319,328,7,9,0,0,320,328,5,60,0,0,321,328,5,61,0,0,322,328,5,200,0,0,
    323,328,5,62,0,0,324,328,3,90,45,0,325,326,7,10,0,0,326,328,3,72,36,0,
    327,317,1,0,0,0,327,318,1,0,0,0,327,320,1,0,0,0,327,321,1,0,0,0,327,322,
    1,0,0,0,327,323,1,0,0,0,327,324,1,0,0,0,327,325,1,0,0,0,328,45,1,0,0,
    0,329,359,5,65,0,0,330,331,5,66,0,0,331,332,3,72,36,0,332,333,5,67,0,
    0,333,334,3,72,36,0,334,360,1,0,0,0,335,336,5,68,0,0,336,360,3,72,36,
    0,337,338,5,22,0,0,338,360,3,72,36,0,339,340,5,48,0,0,340,341,3,72,36,
    0,341,342,5,69,0,0,342,346,3,72,36,0,343,345,3,42,21,0,344,343,1,0,0,
    0,345,348,1,0,0,0,346,344,1,0,0,0,346,347,1,0,0,0,347,360,1,0,0,0,348,
    346,1,0,0,0,349,350,5,70,0,0,350,351,3,72,36,0,351,352,5,69,0,0,352,356,
    3,72,36,0,353,355,3,44,22,0,354,353,1,0,0,0,355,358,1,0,0,0,356,354,1,
    0,0,0,356,357,1,0,0,0,357,360,1,0,0,0,358,356,1,0,0,0,359,330,1,0,0,0,
    359,335,1,0,0,0,359,337,1,0,0,0,359,339,1,0,0,0,359,349,1,0,0,0,360,47,
    1,0,0,0,361,415,5,71,0,0,362,364,3,54,27,0,363,362,1,0,0,0,364,367,1,
    0,0,0,365,363,1,0,0,0,365,366,1,0,0,0,366,416,1,0,0,0,367,365,1,0,0,0,
    368,369,5,72,0,0,369,372,3,72,36,0,370,371,5,33,0,0,371,373,5,191,0,0,
    372,370,1,0,0,0,372,373,1,0,0,0,373,416,1,0,0,0,374,375,5,22,0,0,375,
    376,3,72,36,0,376,377,5,31,0,0,377,378,5,191,0,0,378,416,1,0,0,0,379,
    380,5,73,0,0,380,381,3,72,36,0,381,382,5,74,0,0,382,383,5,191,0,0,383,
    384,5,75,0,0,384,389,5,191,0,0,385,386,5,76,0,0,386,388,3,72,36,0,387,
    385,1,0,0,0,388,391,1,0,0,0,389,387,1,0,0,0,389,390,1,0,0,0,390,416,1,
    0,0,0,391,389,1,0,0,0,392,393,5,196,0,0,393,394,3,72,36,0,394,395,5,31,
    0,0,395,396,5,191,0,0,396,416,1,0,0,0,397,398,5,77,0,0,398,416,3,72,36,
    0,399,400,5,78,0,0,400,416,3,72,36,0,401,402,5,79,0,0,402,416,3,72,36,
    0,403,404,5,80,0,0,404,405,3,72,36,0,405,406,5,67,0,0,406,407,3,72,36,
    0,407,416,1,0,0,0,408,409,5,81,0,0,409,410,3,72,36,0,410,411,5,34,0,0,
    411,412,5,35,0,0,412,413,5,27,0,0,413,414,3,72,36,0,414,416,1,0,0,0,415,
    365,1,0,0,0,415,368,1,0,0,0,415,374,1,0,0,0,415,379,1,0,0,0,415,392,1,
    0,0,0,415,397,1,0,0,0,415,399,1,0,0,0,415,401,1,0,0,0,415,403,1,0,0,0,
    415,408,1,0,0,0,416,49,1,0,0,0,417,421,5,82,0,0,418,420,3,54,27,0,419,
    418,1,0,0,0,420,423,1,0,0,0,421,419,1,0,0,0,421,422,1,0,0,0,422,51,1,
    0,0,0,423,421,1,0,0,0,424,425,5,83,0,0,425,430,3,72,36,0,426,429,3,54,
    27,0,427,429,5,195,0,0,428,426,1,0,0,0,428,427,1,0,0,0,429,432,1,0,0,
    0,430,428,1,0,0,0,430,431,1,0,0,0,431,53,1,0,0,0,432,430,1,0,0,0,433,
    434,7,11,0,0,434,441,5,191,0,0,435,436,5,88,0,0,436,441,7,12,0,0,437,
    438,7,13,0,0,438,441,3,72,36,0,439,441,5,93,0,0,440,433,1,0,0,0,440,435,
    1,0,0,0,440,437,1,0,0,0,440,439,1,0,0,0,441,55,1,0,0,0,442,471,5,94,0,
    0,443,444,5,66,0,0,444,472,3,72,36,0,445,472,5,95,0,0,446,447,7,14,0,
    0,447,452,3,72,36,0,448,449,7,15,0,0,449,451,3,72,36,0,450,448,1,0,0,
    0,451,454,1,0,0,0,452,450,1,0,0,0,452,453,1,0,0,0,453,472,1,0,0,0,454,
    452,1,0,0,0,455,469,5,98,0,0,456,459,5,99,0,0,457,460,3,72,36,0,458,460,
    5,193,0,0,459,457,1,0,0,0,459,458,1,0,0,0,460,461,1,0,0,0,461,459,1,0,
    0,0,461,462,1,0,0,0,462,470,1,0,0,0,463,470,5,79,0,0,464,465,5,100,0,
    0,465,466,3,72,36,0,466,467,5,24,0,0,467,468,3,72,36,0,468,470,1,0,0,
    0,469,456,1,0,0,0,469,463,1,0,0,0,469,464,1,0,0,0,469,470,1,0,0,0,470,
    472,1,0,0,0,471,443,1,0,0,0,471,445,1,0,0,0,471,446,1,0,0,0,471,455,1,
    0,0,0,472,57,1,0,0,0,473,474,5,101,0,0,474,475,3,72,36,0,475,59,1,0,0,
    0,476,477,5,102,0,0,477,478,3,72,36,0,478,61,1,0,0,0,479,480,5,103,0,
    0,480,481,3,72,36,0,481,63,1,0,0,0,482,491,5,104,0,0,483,485,5,1,0,0,
    484,483,1,0,0,0,484,485,1,0,0,0,485,487,1,0,0,0,486,488,3,72,36,0,487,
    486,1,0,0,0,488,489,1,0,0,0,489,487,1,0,0,0,489,490,1,0,0,0,490,492,1,
    0,0,0,491,484,1,0,0,0,491,492,1,0,0,0,492,65,1,0,0,0,493,494,7,16,0,0,
    494,67,1,0,0,0,495,511,5,107,0,0,496,498,5,108,0,0,497,499,3,70,35,0,
    498,497,1,0,0,0,499,500,1,0,0,0,500,498,1,0,0,0,500,501,1,0,0,0,501,512,
    1,0,0,0,502,503,5,109,0,0,503,504,5,110,0,0,504,507,3,72,36,0,505,506,
    5,47,0,0,506,508,3,72,36,0,507,505,1,0,0,0,507,508,1,0,0,0,508,512,1,
    0,0,0,509,510,5,49,0,0,510,512,3,72,36,0,511,496,1,0,0,0,511,502,1,0,
    0,0,511,509,1,0,0,0,512,69,1,0,0,0,513,514,7,17,0,0,514,515,3,72,36,0,
    515,71,1,0,0,0,516,521,5,192,0,0,517,521,5,201,0,0,518,521,5,191,0,0,
    519,521,3,74,37,0,520,516,1,0,0,0,520,517,1,0,0,0,520,518,1,0,0,0,520,
    519,1,0,0,0,521,73,1,0,0,0,522,523,7,18,0,0,523,75,1,0,0,0,524,558,5,
    121,0,0,525,528,7,19,0,0,526,529,3,72,36,0,527,529,3,78,39,0,528,526,
    1,0,0,0,528,527,1,0,0,0,529,530,1,0,0,0,530,528,1,0,0,0,530,531,1,0,0,
    0,531,559,1,0,0,0,532,559,5,5,0,0,533,535,5,123,0,0,534,536,5,191,0,0,
    535,534,1,0,0,0,535,536,1,0,0,0,536,559,1,0,0,0,537,538,5,124,0,0,538,
    540,3,72,36,0,539,541,3,78,39,0,540,539,1,0,0,0,541,542,1,0,0,0,542,540,
    1,0,0,0,542,543,1,0,0,0,543,559,1,0,0,0,544,545,5,125,0,0,545,549,3,72,
    36,0,546,548,3,78,39,0,547,546,1,0,0,0,548,551,1,0,0,0,549,547,1,0,0,
    0,549,550,1,0,0,0,550,559,1,0,0,0,551,549,1,0,0,0,552,554,5,126,0,0,553,
    555,3,78,39,0,554,553,1,0,0,0,555,556,1,0,0,0,556,554,1,0,0,0,556,557,
    1,0,0,0,557,559,1,0,0,0,558,525,1,0,0,0,558,532,1,0,0,0,558,533,1,0,0,
    0,558,537,1,0,0,0,558,544,1,0,0,0,558,552,1,0,0,0,559,77,1,0,0,0,560,
    561,7,20,0,0,561,565,3,72,36,0,562,563,5,138,0,0,563,565,5,191,0,0,564,
    560,1,0,0,0,564,562,1,0,0,0,565,79,1,0,0,0,566,586,5,120,0,0,567,569,
    5,44,0,0,568,570,3,72,36,0,569,568,1,0,0,0,569,570,1,0,0,0,570,572,1,
    0,0,0,571,573,3,82,41,0,572,571,1,0,0,0,573,574,1,0,0,0,574,572,1,0,0,
    0,574,575,1,0,0,0,575,587,1,0,0,0,576,578,7,21,0,0,577,579,3,82,41,0,
    578,577,1,0,0,0,579,580,1,0,0,0,580,578,1,0,0,0,580,581,1,0,0,0,581,587,
    1,0,0,0,582,584,7,22,0,0,583,585,5,191,0,0,584,583,1,0,0,0,584,585,1,
    0,0,0,585,587,1,0,0,0,586,567,1,0,0,0,586,576,1,0,0,0,586,582,1,0,0,0,
    587,81,1,0,0,0,588,589,7,23,0,0,589,600,3,72,36,0,590,591,7,24,0,0,591,
    600,5,191,0,0,592,600,5,176,0,0,593,600,5,177,0,0,594,600,5,178,0,0,595,
    600,5,179,0,0,596,600,5,180,0,0,597,600,5,181,0,0,598,600,5,182,0,0,599,
    588,1,0,0,0,599,590,1,0,0,0,599,592,1,0,0,0,599,593,1,0,0,0,599,594,1,
    0,0,0,599,595,1,0,0,0,599,596,1,0,0,0,599,597,1,0,0,0,599,598,1,0,0,0,
    600,83,1,0,0,0,601,625,5,114,0,0,602,626,5,115,0,0,603,604,5,116,0,0,
    604,605,3,72,36,0,605,606,5,26,0,0,606,611,3,72,36,0,607,608,5,84,0,0,
    608,612,5,191,0,0,609,610,5,37,0,0,610,612,3,72,36,0,611,607,1,0,0,0,
    611,609,1,0,0,0,612,626,1,0,0,0,613,614,5,117,0,0,614,615,3,72,36,0,615,
    616,5,26,0,0,616,617,3,72,36,0,617,618,5,84,0,0,618,619,5,191,0,0,619,
    620,5,183,0,0,620,623,3,72,36,0,621,622,5,33,0,0,622,624,5,191,0,0,623,
    621,1,0,0,0,623,624,1,0,0,0,624,626,1,0,0,0,625,602,1,0,0,0,625,603,1,
    0,0,0,625,613,1,0,0,0,626,85,1,0,0,0,627,628,5,43,0,0,628,645,5,118,0,
    0,629,630,3,72,36,0,630,631,5,26,0,0,631,634,3,72,36,0,632,633,5,25,0,
    0,633,635,3,72,36,0,634,632,1,0,0,0,634,635,1,0,0,0,635,646,1,0,0,0,636,
    637,5,25,0,0,637,638,3,72,36,0,638,639,5,184,0,0,639,640,3,72,36,0,640,
    641,5,185,0,0,641,642,3,72,36,0,642,643,5,26,0,0,643,644,3,72,36,0,644,
    646,1,0,0,0,645,629,1,0,0,0,645,636,1,0,0,0,646,648,1,0,0,0,647,649,3,
    96,48,0,648,647,1,0,0,0,648,649,1,0,0,0,649,87,1,0,0,0,650,651,5,43,0,
    0,651,652,5,119,0,0,652,653,5,191,0,0,653,654,5,47,0,0,654,656,3,72,36,
    0,655,657,3,96,48,0,656,655,1,0,0,0,656,657,1,0,0,0,657,89,1,0,0,0,658,
    659,5,186,0,0,659,660,3,72,36,0,660,91,1,0,0,0,661,662,7,25,0,0,662,663,
    5,191,0,0,663,93,1,0,0,0,664,665,5,189,0,0,665,674,3,72,36,0,666,669,
    5,190,0,0,667,670,3,72,36,0,668,670,5,193,0,0,669,667,1,0,0,0,669,668,
    1,0,0,0,670,671,1,0,0,0,671,669,1,0,0,0,671,672,1,0,0,0,672,674,1,0,0,
    0,673,664,1,0,0,0,673,666,1,0,0,0,674,95,1,0,0,0,675,676,5,197,0,0,676,
    677,3,72,36,0,677,97,1,0,0,0,81,130,137,144,149,155,157,162,173,177,184,
    192,199,205,207,213,222,230,238,246,254,260,262,269,273,275,284,286,292,
    294,298,300,310,315,327,346,356,359,365,372,389,415,421,428,430,440,452,
    459,461,469,471,484,489,491,500,507,511,520,528,530,535,542,549,556,558,
    564,569,574,580,584,586,599,611,623,625,634,645,648,656,669,671,673
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

TracerCommandsParser::LiveUpdateContext* TracerCommandsParser::CommandContext::liveUpdate() {
  return getRuleContext<TracerCommandsParser::LiveUpdateContext>(0);
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
    setState(130);
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
      moduleIndex();
      break;
    }

    case 10: {
      enterOuterAlt(_localctx, 10);
      setState(111);
      threads();
      break;
    }

    case 11: {
      enterOuterAlt(_localctx, 11);
      setState(112);
      symbols();
      break;
    }

    case 12: {
      enterOuterAlt(_localctx, 12);
      setState(113);
      probe();
      break;
    }

    case 13: {
      enterOuterAlt(_localctx, 13);
      setState(114);
      interceptor();
      break;
    }

    case 14: {
      enterOuterAlt(_localctx, 14);
      setState(115);
      events();
      break;
    }

    case 15: {
      enterOuterAlt(_localctx, 15);
      setState(116);
      collect();
      break;
    }

    case 16: {
      enterOuterAlt(_localctx, 16);
      setState(117);
      record();
      break;
    }

    case 17: {
      enterOuterAlt(_localctx, 17);
      setState(118);
      cpp();
      break;
    }

    case 18: {
      enterOuterAlt(_localctx, 18);
      setState(119);
      shell();
      break;
    }

    case 19: {
      enterOuterAlt(_localctx, 19);
      setState(120);
      complete();
      break;
    }

    case 20: {
      enterOuterAlt(_localctx, 20);
      setState(121);
      type();
      break;
    }

    case 21: {
      enterOuterAlt(_localctx, 21);
      setState(122);
      help();
      break;
    }

    case 22: {
      enterOuterAlt(_localctx, 22);
      setState(123);
      quit();
      break;
    }

    case 23: {
      enterOuterAlt(_localctx, 23);
      setState(124);
      bulkTrace();
      break;
    }

    case 24: {
      enterOuterAlt(_localctx, 24);
      setState(125);
      memory();
      break;
    }

    case 25: {
      enterOuterAlt(_localctx, 25);
      setState(126);
      probeEntry();
      break;
    }

    case 26: {
      enterOuterAlt(_localctx, 26);
      setState(127);
      probeReload();
      break;
    }

    case 27: {
      enterOuterAlt(_localctx, 27);
      setState(128);
      objectWatch();
      break;
    }

    case 28: {
      enterOuterAlt(_localctx, 28);
      setState(129);
      liveUpdate();
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
    setState(132);
    match(TracerCommandsParser::T__1);
    setState(144);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__2: {
        setState(133);
        match(TracerCommandsParser::T__2);
        setState(137);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__6

        || _la == TracerCommandsParser::T__7 || _la == TracerCommandsParser::T__186

        || _la == TracerCommandsParser::T__187) {
          setState(134);
          serverOption();
          setState(139);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__3: {
        setState(140);
        match(TracerCommandsParser::T__3);
        setState(141);
        value();
        break;
      }

      case TracerCommandsParser::T__4: {
        setState(142);
        match(TracerCommandsParser::T__4);
        break;
      }

      case TracerCommandsParser::T__5: {
        setState(143);
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
    setState(149);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__6:
      case TracerCommandsParser::T__7: {
        enterOuterAlt(_localctx, 1);
        setState(146);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__6

        || _la == TracerCommandsParser::T__7)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(147);
        value();
        break;
      }

      case TracerCommandsParser::T__186:
      case TracerCommandsParser::T__187: {
        enterOuterAlt(_localctx, 2);
        setState(148);
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
    setState(151);
    match(TracerCommandsParser::T__8);
    setState(157);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::UINT) {
      setState(152);
      match(TracerCommandsParser::UINT);
      setState(155);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == TracerCommandsParser::T__9) {
        setState(153);
        match(TracerCommandsParser::T__9);
        setState(154);
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
    setState(159);
    match(TracerCommandsParser::T__10);
    setState(162);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::T__11) {
      setState(160);
      match(TracerCommandsParser::T__11);
      setState(161);
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
    setState(164);
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
    setState(166);
    match(TracerCommandsParser::T__13);
    setState(167);
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
    setState(169);
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
    setState(171);
    match(TracerCommandsParser::T__14);
    setState(173);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 2103400120314492) != 0) || ((((_la - 65) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 65)) & 4611158252360036587) != 0) || ((((_la - 191) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 191)) & 1059) != 0)) {
      setState(172);
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
    setState(175);
    match(TracerCommandsParser::T__15);
    setState(177);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::T__16) {
      setState(176);
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
    setState(179);
    match(TracerCommandsParser::T__17);
    setState(207);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__18: {
        setState(180);
        match(TracerCommandsParser::T__18);
        setState(182);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(181);
          moduleBuildOption();
          setState(184);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 2130706432) != 0));
        break;
      }

      case TracerCommandsParser::T__13: {
        setState(186);
        match(TracerCommandsParser::T__13);
        setState(187);
        value();
        break;
      }

      case TracerCommandsParser::T__19:
      case TracerCommandsParser::T__20: {
        setState(188);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__19

        || _la == TracerCommandsParser::T__20)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(192);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 100931735552) != 0)) {
          setState(189);
          moduleQueryOption();
          setState(194);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__21: {
        setState(195);
        match(TracerCommandsParser::T__21);
        setState(197);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(196);
          moduleShowOption();
          setState(199);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 964220157952) != 0));
        break;
      }

      case TracerCommandsParser::T__22: {
        setState(201);
        match(TracerCommandsParser::T__22);
        setState(203);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(202);
          moduleEmitOption();
          setState(205);
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
    setState(213);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__23:
      case TracerCommandsParser::T__24:
      case TracerCommandsParser::T__25:
      case TracerCommandsParser::T__26:
      case TracerCommandsParser::T__27: {
        enterOuterAlt(_localctx, 1);
        setState(209);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 520093696) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(210);
        value();
        break;
      }

      case TracerCommandsParser::T__28:
      case TracerCommandsParser::T__29: {
        enterOuterAlt(_localctx, 2);
        setState(211);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__28

        || _la == TracerCommandsParser::T__29)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(212);
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
    setState(222);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__11:
      case TracerCommandsParser::T__30:
      case TracerCommandsParser::T__31: {
        enterOuterAlt(_localctx, 1);
        setState(215);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 6442455040) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(216);
        value();
        break;
      }

      case TracerCommandsParser::T__32: {
        enterOuterAlt(_localctx, 2);
        setState(217);
        match(TracerCommandsParser::T__32);
        setState(218);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__33: {
        enterOuterAlt(_localctx, 3);
        setState(219);
        match(TracerCommandsParser::T__33);
        setState(220);
        match(TracerCommandsParser::T__34);
        break;
      }

      case TracerCommandsParser::T__35: {
        enterOuterAlt(_localctx, 4);
        setState(221);
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
    setState(224);
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 964220157952) != 0))) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(225);
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
    setState(230);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__30:
      case TracerCommandsParser::T__36:
      case TracerCommandsParser::T__37:
      case TracerCommandsParser::T__38: {
        enterOuterAlt(_localctx, 1);
        setState(227);
        moduleShowOption();
        break;
      }

      case TracerCommandsParser::T__26: {
        enterOuterAlt(_localctx, 2);
        setState(228);
        match(TracerCommandsParser::T__26);
        setState(229);
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
    setState(232);
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
    setState(234);
    match(TracerCommandsParser::T__20);
    setState(238);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 6610055335936) != 0)) {
      setState(235);
      symbolOption();
      setState(240);
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
    setState(246);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__11:
      case TracerCommandsParser::T__24:
      case TracerCommandsParser::T__25:
      case TracerCommandsParser::T__31:
      case TracerCommandsParser::T__40: {
        enterOuterAlt(_localctx, 1);
        setState(241);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 2203418890240) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(242);
        value();
        break;
      }

      case TracerCommandsParser::T__32: {
        enterOuterAlt(_localctx, 2);
        setState(243);
        match(TracerCommandsParser::T__32);
        setState(244);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__41: {
        enterOuterAlt(_localctx, 3);
        setState(245);
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
    setState(300);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__42: {
        enterOuterAlt(_localctx, 1);
        setState(248);
        match(TracerCommandsParser::T__42);
        setState(294);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__43: {
            setState(249);
            match(TracerCommandsParser::T__43);
            setState(250);
            value();
            setState(252);
            _errHandler->sync(this);
            _la = _input->LA(1);
            do {
              setState(251);
              prepareOption();
              setState(254);
              _errHandler->sync(this);
              _la = _input->LA(1);
            } while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 69806344147828736) != 0) || ((((_la - 189) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 189)) & 1827) != 0));
            break;
          }

          case TracerCommandsParser::T__44: {
            setState(256);
            match(TracerCommandsParser::T__44);
            setState(257);
            value();
            setState(262);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 520165756994846720) != 0) || ((((_la - 189) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 189)) & 259) != 0)) {
              setState(260);
              _errHandler->sync(this);
              switch (_input->LA(1)) {
                case TracerCommandsParser::T__24:
                case TracerCommandsParser::T__50:
                case TracerCommandsParser::T__51:
                case TracerCommandsParser::T__52:
                case TracerCommandsParser::T__55:
                case TracerCommandsParser::T__56:
                case TracerCommandsParser::T__57:
                case TracerCommandsParser::T__188:
                case TracerCommandsParser::T__189: {
                  setState(258);
                  resolveOption();
                  break;
                }

                case TracerCommandsParser::CWD: {
                  setState(259);
                  cwdOption();
                  break;
                }

              default:
                throw NoViableAltException(this);
              }
              setState(264);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__45: {
            setState(265);
            match(TracerCommandsParser::T__45);
            setState(266);
            value();
            setState(269);
            _errHandler->sync(this);

            _la = _input->LA(1);
            if (_la == TracerCommandsParser::T__46) {
              setState(267);
              match(TracerCommandsParser::T__46);
              setState(268);
              value();
            }
            setState(275);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while (((((_la - 25) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 25)) & 1097833906177) != 0) || ((((_la - 186) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 186)) & 18457) != 0)) {
              setState(273);
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
                case TracerCommandsParser::T__185:
                case TracerCommandsParser::T__188:
                case TracerCommandsParser::T__189:
                case TracerCommandsParser::SNAPSHOT_RETURN_POINTEE: {
                  setState(271);
                  deployOption();
                  break;
                }

                case TracerCommandsParser::CWD: {
                  setState(272);
                  cwdOption();
                  break;
                }

              default:
                throw NoViableAltException(this);
              }
              setState(277);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__47: {
            setState(278);
            match(TracerCommandsParser::T__47);
            setState(279);
            value();
            setState(280);
            match(TracerCommandsParser::T__46);
            setState(281);
            value();
            setState(286);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 520165756994846720) != 0) || ((((_la - 189) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 189)) & 259) != 0)) {
              setState(284);
              _errHandler->sync(this);
              switch (_input->LA(1)) {
                case TracerCommandsParser::T__24:
                case TracerCommandsParser::T__50:
                case TracerCommandsParser::T__51:
                case TracerCommandsParser::T__52:
                case TracerCommandsParser::T__55:
                case TracerCommandsParser::T__56:
                case TracerCommandsParser::T__57:
                case TracerCommandsParser::T__188:
                case TracerCommandsParser::T__189: {
                  setState(282);
                  resolveOption();
                  break;
                }

                case TracerCommandsParser::CWD: {
                  setState(283);
                  cwdOption();
                  break;
                }

              default:
                throw NoViableAltException(this);
              }
              setState(288);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__48: {
            setState(289);
            match(TracerCommandsParser::T__48);
            setState(290);
            match(TracerCommandsParser::UINT);
            setState(292);
            _errHandler->sync(this);

            _la = _input->LA(1);
            if (_la == TracerCommandsParser::CWD) {
              setState(291);
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
        setState(296);
        match(TracerCommandsParser::T__49);
        setState(298);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::CWD) {
          setState(297);
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
    setState(310);
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
        setState(302);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 69806344147828736) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(303);
        value();
        break;
      }

      case TracerCommandsParser::SIGNATURE: {
        enterOuterAlt(_localctx, 2);
        setState(304);
        match(TracerCommandsParser::SIGNATURE);
        setState(305);
        value();
        break;
      }

      case TracerCommandsParser::MEMBER: {
        enterOuterAlt(_localctx, 3);
        setState(306);
        match(TracerCommandsParser::MEMBER);
        break;
      }

      case TracerCommandsParser::METADATA_ONLY: {
        enterOuterAlt(_localctx, 4);
        setState(307);
        match(TracerCommandsParser::METADATA_ONLY);
        break;
      }

      case TracerCommandsParser::T__188:
      case TracerCommandsParser::T__189: {
        enterOuterAlt(_localctx, 5);
        setState(308);
        extraCompileOption();
        break;
      }

      case TracerCommandsParser::CWD: {
        enterOuterAlt(_localctx, 6);
        setState(309);
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
    setState(315);
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
        setState(312);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 520165756994846720) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(313);
        value();
        break;
      }

      case TracerCommandsParser::T__188:
      case TracerCommandsParser::T__189: {
        enterOuterAlt(_localctx, 2);
        setState(314);
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
    setState(327);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__24:
      case TracerCommandsParser::T__50:
      case TracerCommandsParser::T__51:
      case TracerCommandsParser::T__52:
      case TracerCommandsParser::T__55:
      case TracerCommandsParser::T__56:
      case TracerCommandsParser::T__57:
      case TracerCommandsParser::T__188:
      case TracerCommandsParser::T__189: {
        enterOuterAlt(_localctx, 1);
        setState(317);
        resolveOption();
        break;
      }

      case TracerCommandsParser::T__58: {
        enterOuterAlt(_localctx, 2);
        setState(318);
        match(TracerCommandsParser::T__58);
        setState(319);
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
        setState(320);
        match(TracerCommandsParser::T__59);
        break;
      }

      case TracerCommandsParser::T__60: {
        enterOuterAlt(_localctx, 4);
        setState(321);
        match(TracerCommandsParser::T__60);
        break;
      }

      case TracerCommandsParser::SNAPSHOT_RETURN_POINTEE: {
        enterOuterAlt(_localctx, 5);
        setState(322);
        match(TracerCommandsParser::SNAPSHOT_RETURN_POINTEE);
        break;
      }

      case TracerCommandsParser::T__61: {
        enterOuterAlt(_localctx, 6);
        setState(323);
        match(TracerCommandsParser::T__61);
        break;
      }

      case TracerCommandsParser::T__185: {
        enterOuterAlt(_localctx, 7);
        setState(324);
        stackFilter();
        break;
      }

      case TracerCommandsParser::T__62:
      case TracerCommandsParser::T__63: {
        enterOuterAlt(_localctx, 8);
        setState(325);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__62

        || _la == TracerCommandsParser::T__63)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(326);
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
    setState(329);
    match(TracerCommandsParser::T__64);
    setState(359);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__65: {
        setState(330);
        match(TracerCommandsParser::T__65);
        setState(331);
        value();
        setState(332);
        match(TracerCommandsParser::T__66);
        setState(333);
        value();
        break;
      }

      case TracerCommandsParser::T__67: {
        setState(335);
        match(TracerCommandsParser::T__67);
        setState(336);
        value();
        break;
      }

      case TracerCommandsParser::T__21: {
        setState(337);
        match(TracerCommandsParser::T__21);
        setState(338);
        value();
        break;
      }

      case TracerCommandsParser::T__47: {
        setState(339);
        match(TracerCommandsParser::T__47);
        setState(340);
        value();
        setState(341);
        match(TracerCommandsParser::T__68);
        setState(342);
        value();
        setState(346);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 520165756994846720) != 0) || _la == TracerCommandsParser::T__188

        || _la == TracerCommandsParser::T__189) {
          setState(343);
          resolveOption();
          setState(348);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__69: {
        setState(349);
        match(TracerCommandsParser::T__69);
        setState(350);
        value();
        setState(351);
        match(TracerCommandsParser::T__68);
        setState(352);
        value();
        setState(356);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (((((_la - 25) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 25)) & 1097833906177) != 0) || ((((_la - 186) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 186)) & 16409) != 0)) {
          setState(353);
          deployOption();
          setState(358);
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
    setState(361);
    match(TracerCommandsParser::T__70);
    setState(415);
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
        setState(365);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (((((_la - 67) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 67)) & 121503745) != 0)) {
          setState(362);
          streamOption();
          setState(367);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__71: {
        setState(368);
        match(TracerCommandsParser::T__71);
        setState(369);
        value();
        setState(372);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__32) {
          setState(370);
          match(TracerCommandsParser::T__32);
          setState(371);
          match(TracerCommandsParser::UINT);
        }
        break;
      }

      case TracerCommandsParser::T__21: {
        setState(374);
        match(TracerCommandsParser::T__21);
        setState(375);
        value();
        setState(376);
        match(TracerCommandsParser::T__30);
        setState(377);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__72: {
        setState(379);
        match(TracerCommandsParser::T__72);
        setState(380);
        value();
        setState(381);
        match(TracerCommandsParser::T__73);
        setState(382);
        match(TracerCommandsParser::UINT);
        setState(383);
        match(TracerCommandsParser::T__74);
        setState(384);
        match(TracerCommandsParser::UINT);
        setState(389);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__75) {
          setState(385);
          match(TracerCommandsParser::T__75);
          setState(386);
          value();
          setState(391);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::STACK: {
        setState(392);
        match(TracerCommandsParser::STACK);
        setState(393);
        value();
        setState(394);
        match(TracerCommandsParser::T__30);
        setState(395);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__76: {
        setState(397);
        match(TracerCommandsParser::T__76);
        setState(398);
        value();
        break;
      }

      case TracerCommandsParser::T__77: {
        setState(399);
        match(TracerCommandsParser::T__77);
        setState(400);
        value();
        break;
      }

      case TracerCommandsParser::T__78: {
        setState(401);
        match(TracerCommandsParser::T__78);
        setState(402);
        value();
        break;
      }

      case TracerCommandsParser::T__79: {
        setState(403);
        match(TracerCommandsParser::T__79);
        setState(404);
        value();
        setState(405);
        match(TracerCommandsParser::T__66);
        setState(406);
        value();
        break;
      }

      case TracerCommandsParser::T__80: {
        setState(408);
        match(TracerCommandsParser::T__80);
        setState(409);
        value();
        setState(410);
        match(TracerCommandsParser::T__33);
        setState(411);
        match(TracerCommandsParser::T__34);
        setState(412);
        match(TracerCommandsParser::T__26);
        setState(413);
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
    setState(417);
    match(TracerCommandsParser::T__81);
    setState(421);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (((((_la - 67) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 67)) & 121503745) != 0)) {
      setState(418);
      streamOption();
      setState(423);
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
    setState(424);
    match(TracerCommandsParser::T__82);
    setState(425);
    value();
    setState(430);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (((((_la - 67) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 67)) & 121503745) != 0) || _la == TracerCommandsParser::SQLITE) {
      setState(428);
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
          setState(426);
          streamOption();
          break;
        }

        case TracerCommandsParser::SQLITE: {
          setState(427);
          match(TracerCommandsParser::SQLITE);
          break;
        }

      default:
        throw NoViableAltException(this);
      }
      setState(432);
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
    setState(440);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__83:
      case TracerCommandsParser::T__84:
      case TracerCommandsParser::T__85:
      case TracerCommandsParser::T__86: {
        enterOuterAlt(_localctx, 1);
        setState(433);
        _la = _input->LA(1);
        if (!(((((_la - 84) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 84)) & 15) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(434);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__87: {
        enterOuterAlt(_localctx, 2);
        setState(435);
        match(TracerCommandsParser::T__87);
        setState(436);
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
        setState(437);
        _la = _input->LA(1);
        if (!(((((_la - 67) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 67)) & 50331649) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(438);
        value();
        break;
      }

      case TracerCommandsParser::T__92: {
        enterOuterAlt(_localctx, 4);
        setState(439);
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
    setState(442);
    match(TracerCommandsParser::T__93);
    setState(471);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__65: {
        setState(443);
        match(TracerCommandsParser::T__65);
        setState(444);
        value();
        break;
      }

      case TracerCommandsParser::T__94: {
        setState(445);
        match(TracerCommandsParser::T__94);
        break;
      }

      case TracerCommandsParser::T__95:
      case TracerCommandsParser::T__96: {
        setState(446);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__95

        || _la == TracerCommandsParser::T__96)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(447);
        value();
        setState(452);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__26

        || _la == TracerCommandsParser::T__36) {
          setState(448);
          _la = _input->LA(1);
          if (!(_la == TracerCommandsParser::T__26

          || _la == TracerCommandsParser::T__36)) {
          _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(449);
          value();
          setState(454);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__97: {
        setState(455);
        match(TracerCommandsParser::T__97);
        setState(469);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__98: {
            setState(456);
            match(TracerCommandsParser::T__98);
            setState(459);
            _errHandler->sync(this);
            _la = _input->LA(1);
            do {
              setState(459);
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
                case TracerCommandsParser::T__120:
                case TracerCommandsParser::T__121:
                case TracerCommandsParser::T__122:
                case TracerCommandsParser::T__123:
                case TracerCommandsParser::T__124:
                case TracerCommandsParser::T__125:
                case TracerCommandsParser::UINT:
                case TracerCommandsParser::STRING:
                case TracerCommandsParser::STACK:
                case TracerCommandsParser::WORD: {
                  setState(457);
                  value();
                  break;
                }

                case TracerCommandsParser::SHELL: {
                  setState(458);
                  match(TracerCommandsParser::SHELL);
                  break;
                }

              default:
                throw NoViableAltException(this);
              }
              setState(461);
              _errHandler->sync(this);
              _la = _input->LA(1);
            } while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 2103400120314492) != 0) || ((((_la - 65) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 65)) & 4611158252360036587) != 0) || ((((_la - 191) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 191)) & 1063) != 0));
            break;
          }

          case TracerCommandsParser::T__78: {
            setState(463);
            match(TracerCommandsParser::T__78);
            break;
          }

          case TracerCommandsParser::T__99: {
            setState(464);
            match(TracerCommandsParser::T__99);
            setState(465);
            value();
            setState(466);
            match(TracerCommandsParser::T__23);
            setState(467);
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
    setState(473);
    match(TracerCommandsParser::T__100);
    setState(474);
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
    setState(476);
    match(TracerCommandsParser::T__101);
    setState(477);
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
    setState(479);
    match(TracerCommandsParser::T__102);
    setState(480);
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
    setState(482);
    match(TracerCommandsParser::T__103);
    setState(491);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 2103400120314494) != 0) || ((((_la - 65) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 65)) & 4611158252360036587) != 0) || ((((_la - 191) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 191)) & 1059) != 0)) {
      setState(484);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == TracerCommandsParser::T__0) {
        setState(483);
        match(TracerCommandsParser::T__0);
      }
      setState(487);
      _errHandler->sync(this);
      _la = _input->LA(1);
      do {
        setState(486);
        value();
        setState(489);
        _errHandler->sync(this);
        _la = _input->LA(1);
      } while ((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & 2103400120314492) != 0) || ((((_la - 65) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 65)) & 4611158252360036587) != 0) || ((((_la - 191) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 191)) & 1059) != 0));
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
    setState(493);
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
    setState(495);
    match(TracerCommandsParser::T__106);
    setState(511);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__107: {
        setState(496);
        match(TracerCommandsParser::T__107);
        setState(498);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(497);
          bulkTraceOption();
          setState(500);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while (_la == TracerCommandsParser::T__24

        || _la == TracerCommandsParser::T__50 || ((((_la - 110) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 110)) & 15) != 0));
        break;
      }

      case TracerCommandsParser::T__108: {
        setState(502);
        match(TracerCommandsParser::T__108);
        setState(503);
        match(TracerCommandsParser::T__109);
        setState(504);
        value();
        setState(507);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__46) {
          setState(505);
          match(TracerCommandsParser::T__46);
          setState(506);
          value();
        }
        break;
      }

      case TracerCommandsParser::T__48: {
        setState(509);
        match(TracerCommandsParser::T__48);
        setState(510);
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
    setState(513);
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
    setState(514);
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
    setState(520);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::STRING: {
        enterOuterAlt(_localctx, 1);
        setState(516);
        match(TracerCommandsParser::STRING);
        break;
      }

      case TracerCommandsParser::WORD: {
        enterOuterAlt(_localctx, 2);
        setState(517);
        match(TracerCommandsParser::WORD);
        break;
      }

      case TracerCommandsParser::UINT: {
        enterOuterAlt(_localctx, 3);
        setState(518);
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
      case TracerCommandsParser::T__120:
      case TracerCommandsParser::T__121:
      case TracerCommandsParser::T__122:
      case TracerCommandsParser::T__123:
      case TracerCommandsParser::T__124:
      case TracerCommandsParser::T__125:
      case TracerCommandsParser::STACK: {
        enterOuterAlt(_localctx, 4);
        setState(519);
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
    setState(522);
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 2103400120314492) != 0) || ((((_la - 65) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 65)) & 4611158252360036587) != 0) || _la == TracerCommandsParser::STACK)) {
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

//----------------- LiveUpdateContext ------------------------------------------------------------------

TracerCommandsParser::LiveUpdateContext::LiveUpdateContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<TracerCommandsParser::ValueContext *> TracerCommandsParser::LiveUpdateContext::value() {
  return getRuleContexts<TracerCommandsParser::ValueContext>();
}

TracerCommandsParser::ValueContext* TracerCommandsParser::LiveUpdateContext::value(size_t i) {
  return getRuleContext<TracerCommandsParser::ValueContext>(i);
}

std::vector<TracerCommandsParser::LiveOptionContext *> TracerCommandsParser::LiveUpdateContext::liveOption() {
  return getRuleContexts<TracerCommandsParser::LiveOptionContext>();
}

TracerCommandsParser::LiveOptionContext* TracerCommandsParser::LiveUpdateContext::liveOption(size_t i) {
  return getRuleContext<TracerCommandsParser::LiveOptionContext>(i);
}

tree::TerminalNode* TracerCommandsParser::LiveUpdateContext::UINT() {
  return getToken(TracerCommandsParser::UINT, 0);
}


size_t TracerCommandsParser::LiveUpdateContext::getRuleIndex() const {
  return TracerCommandsParser::RuleLiveUpdate;
}


std::any TracerCommandsParser::LiveUpdateContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitLiveUpdate(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::LiveUpdateContext* TracerCommandsParser::liveUpdate() {
  LiveUpdateContext *_localctx = _tracker.createInstance<LiveUpdateContext>(_ctx, getState());
  enterRule(_localctx, 76, TracerCommandsParser::RuleLiveUpdate);
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
    setState(524);
    match(TracerCommandsParser::T__120);
    setState(558);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__107:
      case TracerCommandsParser::T__121: {
        setState(525);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__107

        || _la == TracerCommandsParser::T__121)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(528);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(528);
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
            case TracerCommandsParser::T__120:
            case TracerCommandsParser::T__121:
            case TracerCommandsParser::T__122:
            case TracerCommandsParser::T__123:
            case TracerCommandsParser::T__124:
            case TracerCommandsParser::T__125:
            case TracerCommandsParser::UINT:
            case TracerCommandsParser::STRING:
            case TracerCommandsParser::STACK:
            case TracerCommandsParser::WORD: {
              setState(526);
              value();
              break;
            }

            case TracerCommandsParser::T__23:
            case TracerCommandsParser::T__24:
            case TracerCommandsParser::T__36:
            case TracerCommandsParser::T__40:
            case TracerCommandsParser::T__51:
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
            case TracerCommandsParser::T__137: {
              setState(527);
              liveOption();
              break;
            }

          default:
            throw NoViableAltException(this);
          }
          setState(530);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 6609336260225660) != 0) || ((((_la - 65) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 65)) & -527766067351317) != 0) || ((((_la - 129) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 129)) & -4611686018427386881) != 0) || _la == TracerCommandsParser::STACK

        || _la == TracerCommandsParser::WORD);
        break;
      }

      case TracerCommandsParser::T__4: {
        setState(532);
        match(TracerCommandsParser::T__4);
        break;
      }

      case TracerCommandsParser::T__122: {
        setState(533);
        match(TracerCommandsParser::T__122);
        setState(535);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::UINT) {
          setState(534);
          match(TracerCommandsParser::UINT);
        }
        break;
      }

      case TracerCommandsParser::T__123: {
        setState(537);
        match(TracerCommandsParser::T__123);
        setState(538);
        value();
        setState(540);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(539);
          liveOption();
          setState(542);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 4505936139911168) != 0) || ((((_la - 127) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 127)) & 4095) != 0));
        break;
      }

      case TracerCommandsParser::T__124: {
        setState(544);
        match(TracerCommandsParser::T__124);
        setState(545);
        value();
        setState(549);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 4505936139911168) != 0) || ((((_la - 127) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 127)) & 4095) != 0)) {
          setState(546);
          liveOption();
          setState(551);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__125: {
        setState(552);
        match(TracerCommandsParser::T__125);
        setState(554);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(553);
          liveOption();
          setState(556);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 4505936139911168) != 0) || ((((_la - 127) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 127)) & 4095) != 0));
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

//----------------- LiveOptionContext ------------------------------------------------------------------

TracerCommandsParser::LiveOptionContext::LiveOptionContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

TracerCommandsParser::ValueContext* TracerCommandsParser::LiveOptionContext::value() {
  return getRuleContext<TracerCommandsParser::ValueContext>(0);
}

tree::TerminalNode* TracerCommandsParser::LiveOptionContext::UINT() {
  return getToken(TracerCommandsParser::UINT, 0);
}


size_t TracerCommandsParser::LiveOptionContext::getRuleIndex() const {
  return TracerCommandsParser::RuleLiveOption;
}


std::any TracerCommandsParser::LiveOptionContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitLiveOption(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::LiveOptionContext* TracerCommandsParser::liveOption() {
  LiveOptionContext *_localctx = _tracker.createInstance<LiveOptionContext>(_ctx, getState());
  enterRule(_localctx, 78, TracerCommandsParser::RuleLiveOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(564);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__23:
      case TracerCommandsParser::T__24:
      case TracerCommandsParser::T__36:
      case TracerCommandsParser::T__40:
      case TracerCommandsParser::T__51:
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
      case TracerCommandsParser::T__136: {
        enterOuterAlt(_localctx, 1);
        setState(560);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 4505936139911168) != 0) || ((((_la - 127) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 127)) & 2047) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(561);
        value();
        break;
      }

      case TracerCommandsParser::T__137: {
        enterOuterAlt(_localctx, 2);
        setState(562);
        match(TracerCommandsParser::T__137);
        setState(563);
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
  enterRule(_localctx, 80, TracerCommandsParser::RuleObjectWatch);
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
    setState(566);
    match(TracerCommandsParser::T__119);
    setState(586);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__43: {
        setState(567);
        match(TracerCommandsParser::T__43);
        setState(569);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 2103400120314492) != 0) || ((((_la - 65) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 65)) & 4611158252360036587) != 0) || ((((_la - 191) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 191)) & 1059) != 0)) {
          setState(568);
          value();
        }
        setState(572);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(571);
          objectOption();
          setState(574);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & -8631148547175088000) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 64)) & 2101249) != 0) || ((((_la - 139) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 139)) & 17592186044415) != 0));
        break;
      }

      case TracerCommandsParser::T__2:
      case TracerCommandsParser::T__107: {
        setState(576);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__2 || _la == TracerCommandsParser::T__107)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(578);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(577);
          objectOption();
          setState(580);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & -8631148547175088000) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 64)) & 2101249) != 0) || ((((_la - 139) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 139)) & 17592186044415) != 0));
        break;
      }

      case TracerCommandsParser::T__4:
      case TracerCommandsParser::T__5: {
        setState(582);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__4

        || _la == TracerCommandsParser::T__5)) {
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
  enterRule(_localctx, 82, TracerCommandsParser::RuleObjectOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(599);
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
      case TracerCommandsParser::T__155:
      case TracerCommandsParser::T__156:
      case TracerCommandsParser::T__157:
      case TracerCommandsParser::T__158:
      case TracerCommandsParser::T__159:
      case TracerCommandsParser::T__160:
      case TracerCommandsParser::T__161:
      case TracerCommandsParser::T__162:
      case TracerCommandsParser::T__163:
      case TracerCommandsParser::T__164: {
        enterOuterAlt(_localctx, 1);
        setState(588);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & -9207609300552253312) != 0) || _la == TracerCommandsParser::T__63

        || _la == TracerCommandsParser::T__75 || ((((_la - 139) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 139)) & 134217727) != 0))) {
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

      case TracerCommandsParser::T__29:
      case TracerCommandsParser::T__58:
      case TracerCommandsParser::T__84:
      case TracerCommandsParser::T__165:
      case TracerCommandsParser::T__166:
      case TracerCommandsParser::T__167:
      case TracerCommandsParser::T__168:
      case TracerCommandsParser::T__169:
      case TracerCommandsParser::T__170:
      case TracerCommandsParser::T__171:
      case TracerCommandsParser::T__172:
      case TracerCommandsParser::T__173:
      case TracerCommandsParser::T__174: {
        enterOuterAlt(_localctx, 2);
        setState(590);
        _la = _input->LA(1);
        if (!(((((_la - 30) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 30)) & 36028797555834881) != 0) || ((((_la - 166) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 166)) & 1023) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(591);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__175: {
        enterOuterAlt(_localctx, 3);
        setState(592);
        match(TracerCommandsParser::T__175);
        break;
      }

      case TracerCommandsParser::T__176: {
        enterOuterAlt(_localctx, 4);
        setState(593);
        match(TracerCommandsParser::T__176);
        break;
      }

      case TracerCommandsParser::T__177: {
        enterOuterAlt(_localctx, 5);
        setState(594);
        match(TracerCommandsParser::T__177);
        break;
      }

      case TracerCommandsParser::T__178: {
        enterOuterAlt(_localctx, 6);
        setState(595);
        match(TracerCommandsParser::T__178);
        break;
      }

      case TracerCommandsParser::T__179: {
        enterOuterAlt(_localctx, 7);
        setState(596);
        match(TracerCommandsParser::T__179);
        break;
      }

      case TracerCommandsParser::T__180: {
        enterOuterAlt(_localctx, 8);
        setState(597);
        match(TracerCommandsParser::T__180);
        break;
      }

      case TracerCommandsParser::T__181: {
        enterOuterAlt(_localctx, 9);
        setState(598);
        match(TracerCommandsParser::T__181);
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
  enterRule(_localctx, 84, TracerCommandsParser::RuleMemory);
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
    setState(601);
    match(TracerCommandsParser::T__113);
    setState(625);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__114: {
        setState(602);
        match(TracerCommandsParser::T__114);
        break;
      }

      case TracerCommandsParser::T__115: {
        setState(603);
        match(TracerCommandsParser::T__115);
        setState(604);
        value();
        setState(605);
        match(TracerCommandsParser::T__25);
        setState(606);
        value();
        setState(611);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__83: {
            setState(607);
            match(TracerCommandsParser::T__83);
            setState(608);
            match(TracerCommandsParser::UINT);
            break;
          }

          case TracerCommandsParser::T__36: {
            setState(609);
            match(TracerCommandsParser::T__36);
            setState(610);
            value();
            break;
          }

        default:
          throw NoViableAltException(this);
        }
        break;
      }

      case TracerCommandsParser::T__116: {
        setState(613);
        match(TracerCommandsParser::T__116);
        setState(614);
        value();
        setState(615);
        match(TracerCommandsParser::T__25);
        setState(616);
        value();
        setState(617);
        match(TracerCommandsParser::T__83);
        setState(618);
        match(TracerCommandsParser::UINT);
        setState(619);
        match(TracerCommandsParser::T__182);
        setState(620);
        value();
        setState(623);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__32) {
          setState(621);
          match(TracerCommandsParser::T__32);
          setState(622);
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
  enterRule(_localctx, 86, TracerCommandsParser::RuleProbeEntry);
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
    setState(627);
    match(TracerCommandsParser::T__42);
    setState(628);
    match(TracerCommandsParser::T__117);
    setState(645);
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
      case TracerCommandsParser::T__120:
      case TracerCommandsParser::T__121:
      case TracerCommandsParser::T__122:
      case TracerCommandsParser::T__123:
      case TracerCommandsParser::T__124:
      case TracerCommandsParser::T__125:
      case TracerCommandsParser::UINT:
      case TracerCommandsParser::STRING:
      case TracerCommandsParser::STACK:
      case TracerCommandsParser::WORD: {
        setState(629);
        value();
        setState(630);
        match(TracerCommandsParser::T__25);
        setState(631);
        value();
        setState(634);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__24) {
          setState(632);
          match(TracerCommandsParser::T__24);
          setState(633);
          value();
        }
        break;
      }

      case TracerCommandsParser::T__24: {
        setState(636);
        match(TracerCommandsParser::T__24);
        setState(637);
        value();
        setState(638);
        match(TracerCommandsParser::T__183);
        setState(639);
        value();
        setState(640);
        match(TracerCommandsParser::T__184);
        setState(641);
        value();
        setState(642);
        match(TracerCommandsParser::T__25);
        setState(643);
        value();
        break;
      }

    default:
      throw NoViableAltException(this);
    }
    setState(648);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::CWD) {
      setState(647);
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
  enterRule(_localctx, 88, TracerCommandsParser::RuleProbeReload);
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
    setState(650);
    match(TracerCommandsParser::T__42);
    setState(651);
    match(TracerCommandsParser::T__118);
    setState(652);
    match(TracerCommandsParser::UINT);
    setState(653);
    match(TracerCommandsParser::T__46);
    setState(654);
    value();
    setState(656);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::CWD) {
      setState(655);
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
  enterRule(_localctx, 90, TracerCommandsParser::RuleStackFilter);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(658);
    match(TracerCommandsParser::T__185);
    setState(659);
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
  enterRule(_localctx, 92, TracerCommandsParser::RuleContextEventBytes);
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
    setState(661);
    _la = _input->LA(1);
    if (!(_la == TracerCommandsParser::T__186

    || _la == TracerCommandsParser::T__187)) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(662);
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
  enterRule(_localctx, 94, TracerCommandsParser::RuleExtraCompileOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(673);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__188: {
        enterOuterAlt(_localctx, 1);
        setState(664);
        match(TracerCommandsParser::T__188);
        setState(665);
        value();
        break;
      }

      case TracerCommandsParser::T__189: {
        enterOuterAlt(_localctx, 2);
        setState(666);
        match(TracerCommandsParser::T__189);
        setState(669);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(669);
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
            case TracerCommandsParser::T__120:
            case TracerCommandsParser::T__121:
            case TracerCommandsParser::T__122:
            case TracerCommandsParser::T__123:
            case TracerCommandsParser::T__124:
            case TracerCommandsParser::T__125:
            case TracerCommandsParser::UINT:
            case TracerCommandsParser::STRING:
            case TracerCommandsParser::STACK:
            case TracerCommandsParser::WORD: {
              setState(667);
              value();
              break;
            }

            case TracerCommandsParser::SHELL: {
              setState(668);
              match(TracerCommandsParser::SHELL);
              break;
            }

          default:
            throw NoViableAltException(this);
          }
          setState(671);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 2103400120314492) != 0) || ((((_la - 65) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 65)) & 4611158252360036587) != 0) || ((((_la - 191) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 191)) & 1063) != 0));
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
  enterRule(_localctx, 96, TracerCommandsParser::RuleCwdOption);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(675);
    match(TracerCommandsParser::CWD);
    setState(676);
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
