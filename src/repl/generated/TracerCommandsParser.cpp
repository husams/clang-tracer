
// Generated from TracerCommands.g4 by ANTLR 4.13.2


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
      "value", "keyword", "liveUpdate", "liveScanSelection", "liveOption",
      "objectWatch", "objectOption", "memory", "probeEntry", "probeReload",
      "stackFilter", "contextEventBytes", "extraCompileOption", "cwdOption"
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
      "'--from-scan'", "'--build'", "'--function'", "'--fixture'", "'--bind'",
      "'--recursion'", "'--discover-file'", "'--discover-function'", "'--after-file'",
      "'--after-function'", "'--for-each'", "'--managed'", "'--retention-limit'",
      "'--base'", "'--owner'", "'--field'", "'--collection'", "'--owner-add'",
      "'--owner-get'", "'--owner-remove'", "'--owner-snapshot'", "'--snapshot-field'",
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
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
      "", "", "", "", "", "UINT", "STRING", "SHELL", "METADATA_ONLY", "SQLITE",
      "STACK", "CWD", "SIGNATURE", "MEMBER", "SNAPSHOT_RETURN_POINTEE",
      "WORD", "WS"
    }
  );
  static const int32_t serializedATNSegment[] = {
    4,1,203,688,2,0,7,0,2,1,7,1,2,2,7,2,2,3,7,3,2,4,7,4,2,5,7,5,2,6,7,6,2,
    7,7,7,2,8,7,8,2,9,7,9,2,10,7,10,2,11,7,11,2,12,7,12,2,13,7,13,2,14,7,
    14,2,15,7,15,2,16,7,16,2,17,7,17,2,18,7,18,2,19,7,19,2,20,7,20,2,21,7,
    21,2,22,7,22,2,23,7,23,2,24,7,24,2,25,7,25,2,26,7,26,2,27,7,27,2,28,7,
    28,2,29,7,29,2,30,7,30,2,31,7,31,2,32,7,32,2,33,7,33,2,34,7,34,2,35,7,
    35,2,36,7,36,2,37,7,37,2,38,7,38,2,39,7,39,2,40,7,40,2,41,7,41,2,42,7,
    42,2,43,7,43,2,44,7,44,2,45,7,45,2,46,7,46,2,47,7,47,2,48,7,48,2,49,7,
    49,1,0,1,0,1,0,1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,3,1,133,
    8,1,1,2,1,2,1,2,5,2,138,8,2,10,2,12,2,141,9,2,1,2,1,2,1,2,1,2,3,2,147,
    8,2,1,3,1,3,1,3,3,3,152,8,3,1,4,1,4,1,4,1,4,3,4,158,8,4,3,4,160,8,4,1,
    5,1,5,1,5,3,5,165,8,5,1,6,1,6,1,7,1,7,1,7,1,8,1,8,1,9,1,9,3,9,176,8,9,
    1,10,1,10,3,10,180,8,10,1,11,1,11,1,11,4,11,185,8,11,11,11,12,11,186,
    1,11,1,11,1,11,1,11,5,11,193,8,11,10,11,12,11,196,9,11,1,11,1,11,4,11,
    200,8,11,11,11,12,11,201,1,11,1,11,4,11,206,8,11,11,11,12,11,207,3,11,
    210,8,11,1,12,1,12,1,12,1,12,3,12,216,8,12,1,13,1,13,1,13,1,13,1,13,1,
    13,1,13,3,13,225,8,13,1,14,1,14,1,14,1,15,1,15,1,15,3,15,233,8,15,1,16,
    1,16,1,17,1,17,5,17,239,8,17,10,17,12,17,242,9,17,1,18,1,18,1,18,1,18,
    1,18,3,18,249,8,18,1,19,1,19,1,19,1,19,4,19,255,8,19,11,19,12,19,256,
    1,19,1,19,1,19,1,19,5,19,263,8,19,10,19,12,19,266,9,19,1,19,1,19,1,19,
    1,19,3,19,272,8,19,1,19,1,19,5,19,276,8,19,10,19,12,19,279,9,19,1,19,
    1,19,1,19,1,19,1,19,1,19,5,19,287,8,19,10,19,12,19,290,9,19,1,19,1,19,
    1,19,3,19,295,8,19,3,19,297,8,19,1,19,1,19,3,19,301,8,19,3,19,303,8,19,
    1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,3,20,313,8,20,1,21,1,21,1,21,
    3,21,318,8,21,1,22,1,22,1,22,1,22,1,22,1,22,1,22,1,22,1,22,1,22,3,22,
    330,8,22,1,23,1,23,1,23,1,23,1,23,1,23,1,23,1,23,1,23,1,23,1,23,1,23,
    1,23,1,23,1,23,5,23,347,8,23,10,23,12,23,350,9,23,1,23,1,23,1,23,1,23,
    1,23,5,23,357,8,23,10,23,12,23,360,9,23,3,23,362,8,23,1,24,1,24,5,24,
    366,8,24,10,24,12,24,369,9,24,1,24,1,24,1,24,1,24,3,24,375,8,24,1,24,
    1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,5,24,390,
    8,24,10,24,12,24,393,9,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,
    1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,
    3,24,418,8,24,1,25,1,25,5,25,422,8,25,10,25,12,25,425,9,25,1,26,1,26,
    1,26,1,26,5,26,431,8,26,10,26,12,26,434,9,26,1,27,1,27,1,27,1,27,1,27,
    1,27,1,27,3,27,443,8,27,1,28,1,28,1,28,1,28,1,28,1,28,1,28,1,28,5,28,
    453,8,28,10,28,12,28,456,9,28,1,28,1,28,1,28,1,28,4,28,462,8,28,11,28,
    12,28,463,1,28,1,28,1,28,1,28,1,28,1,28,3,28,472,8,28,3,28,474,8,28,1,
    29,1,29,1,29,1,30,1,30,1,30,1,31,1,31,1,31,1,32,1,32,3,32,487,8,32,1,
    32,4,32,490,8,32,11,32,12,32,491,3,32,494,8,32,1,33,1,33,1,34,1,34,1,
    34,4,34,501,8,34,11,34,12,34,502,1,34,1,34,1,34,1,34,1,34,3,34,510,8,
    34,1,34,1,34,3,34,514,8,34,1,35,1,35,1,35,1,36,1,36,1,36,1,36,3,36,523,
    8,36,1,37,1,37,1,38,1,38,1,38,1,38,4,38,531,8,38,11,38,12,38,532,1,38,
    1,38,1,38,3,38,538,8,38,1,38,1,38,1,38,4,38,543,8,38,11,38,12,38,544,
    1,38,1,38,1,38,1,38,5,38,551,8,38,10,38,12,38,554,9,38,1,38,1,38,4,38,
    558,8,38,11,38,12,38,559,3,38,562,8,38,1,39,1,39,4,39,566,8,39,11,39,
    12,39,567,1,40,1,40,1,40,1,40,3,40,574,8,40,1,41,1,41,1,41,3,41,579,8,
    41,1,41,4,41,582,8,41,11,41,12,41,583,1,41,1,41,4,41,588,8,41,11,41,12,
    41,589,1,41,1,41,3,41,594,8,41,3,41,596,8,41,1,42,1,42,1,42,1,42,1,42,
    1,42,1,42,1,42,1,42,1,42,1,42,3,42,609,8,42,1,43,1,43,1,43,1,43,1,43,
    1,43,1,43,1,43,1,43,1,43,3,43,621,8,43,1,43,1,43,1,43,1,43,1,43,1,43,
    1,43,1,43,1,43,1,43,3,43,633,8,43,3,43,635,8,43,1,44,1,44,1,44,1,44,1,
    44,1,44,1,44,3,44,644,8,44,1,44,1,44,1,44,1,44,1,44,1,44,1,44,1,44,1,
    44,3,44,655,8,44,1,44,3,44,658,8,44,1,45,1,45,1,45,1,45,1,45,1,45,3,45,
    666,8,45,1,46,1,46,1,46,1,47,1,47,1,47,1,48,1,48,1,48,1,48,1,48,4,48,
    679,8,48,11,48,12,48,680,3,48,683,8,48,1,49,1,49,1,49,1,49,0,0,50,0,2,
    4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,34,36,38,40,42,44,46,48,50,
    52,54,56,58,60,62,64,66,68,70,72,74,76,78,80,82,84,86,88,90,92,94,96,
    98,0,26,1,0,7,8,1,0,20,21,1,0,24,28,1,0,29,30,2,0,12,12,31,32,2,0,31,
    31,37,39,4,0,12,12,25,26,32,32,41,41,4,0,25,25,27,27,39,39,51,55,3,0,
    25,25,51,53,56,58,2,0,192,192,202,202,1,0,63,64,1,0,84,87,1,0,89,90,2,
    0,67,67,91,92,1,0,96,97,2,0,27,27,37,37,1,0,105,106,3,0,25,25,51,51,110,
    113,17,0,2,6,9,9,11,11,13,16,18,23,35,35,40,40,43,46,48,50,65,66,68,68,
    70,72,77,83,89,90,94,109,114,126,197,197,2,0,108,108,122,122,5,0,24,25,
    37,37,41,41,52,52,128,138,2,0,3,3,108,108,1,0,5,6,8,0,7,7,25,25,27,27,
    37,37,51,53,63,64,76,76,140,166,4,0,30,30,59,59,85,85,167,176,1,0,188,
    189,800,0,100,1,0,0,0,2,132,1,0,0,0,4,134,1,0,0,0,6,151,1,0,0,0,8,153,
    1,0,0,0,10,161,1,0,0,0,12,166,1,0,0,0,14,168,1,0,0,0,16,171,1,0,0,0,18,
    173,1,0,0,0,20,177,1,0,0,0,22,181,1,0,0,0,24,215,1,0,0,0,26,224,1,0,0,
    0,28,226,1,0,0,0,30,232,1,0,0,0,32,234,1,0,0,0,34,236,1,0,0,0,36,248,
    1,0,0,0,38,302,1,0,0,0,40,312,1,0,0,0,42,317,1,0,0,0,44,329,1,0,0,0,46,
    331,1,0,0,0,48,363,1,0,0,0,50,419,1,0,0,0,52,426,1,0,0,0,54,442,1,0,0,
    0,56,444,1,0,0,0,58,475,1,0,0,0,60,478,1,0,0,0,62,481,1,0,0,0,64,484,
    1,0,0,0,66,495,1,0,0,0,68,497,1,0,0,0,70,515,1,0,0,0,72,522,1,0,0,0,74,
    524,1,0,0,0,76,526,1,0,0,0,78,563,1,0,0,0,80,573,1,0,0,0,82,575,1,0,0,
    0,84,608,1,0,0,0,86,610,1,0,0,0,88,636,1,0,0,0,90,659,1,0,0,0,92,667,
    1,0,0,0,94,670,1,0,0,0,96,682,1,0,0,0,98,684,1,0,0,0,100,101,5,1,0,0,
    101,102,3,2,1,0,102,103,5,0,0,1,103,1,1,0,0,0,104,133,3,4,2,0,105,133,
    3,8,4,0,106,133,3,10,5,0,107,133,3,12,6,0,108,133,3,14,7,0,109,133,3,
    16,8,0,110,133,3,18,9,0,111,133,3,20,10,0,112,133,3,22,11,0,113,133,3,
    32,16,0,114,133,3,34,17,0,115,133,3,38,19,0,116,133,3,46,23,0,117,133,
    3,48,24,0,118,133,3,50,25,0,119,133,3,52,26,0,120,133,3,56,28,0,121,133,
    3,58,29,0,122,133,3,60,30,0,123,133,3,62,31,0,124,133,3,64,32,0,125,133,
    3,66,33,0,126,133,3,68,34,0,127,133,3,86,43,0,128,133,3,88,44,0,129,133,
    3,90,45,0,130,133,3,82,41,0,131,133,3,76,38,0,132,104,1,0,0,0,132,105,
    1,0,0,0,132,106,1,0,0,0,132,107,1,0,0,0,132,108,1,0,0,0,132,109,1,0,0,
    0,132,110,1,0,0,0,132,111,1,0,0,0,132,112,1,0,0,0,132,113,1,0,0,0,132,
    114,1,0,0,0,132,115,1,0,0,0,132,116,1,0,0,0,132,117,1,0,0,0,132,118,1,
    0,0,0,132,119,1,0,0,0,132,120,1,0,0,0,132,121,1,0,0,0,132,122,1,0,0,0,
    132,123,1,0,0,0,132,124,1,0,0,0,132,125,1,0,0,0,132,126,1,0,0,0,132,127,
    1,0,0,0,132,128,1,0,0,0,132,129,1,0,0,0,132,130,1,0,0,0,132,131,1,0,0,
    0,133,3,1,0,0,0,134,146,5,2,0,0,135,139,5,3,0,0,136,138,3,6,3,0,137,136,
    1,0,0,0,138,141,1,0,0,0,139,137,1,0,0,0,139,140,1,0,0,0,140,147,1,0,0,
    0,141,139,1,0,0,0,142,143,5,4,0,0,143,147,3,72,36,0,144,147,5,5,0,0,145,
    147,5,6,0,0,146,135,1,0,0,0,146,142,1,0,0,0,146,144,1,0,0,0,146,145,1,
    0,0,0,147,5,1,0,0,0,148,149,7,0,0,0,149,152,3,72,36,0,150,152,3,94,47,
    0,151,148,1,0,0,0,151,150,1,0,0,0,152,7,1,0,0,0,153,159,5,9,0,0,154,157,
    5,192,0,0,155,156,5,10,0,0,156,158,5,192,0,0,157,155,1,0,0,0,157,158,
    1,0,0,0,158,160,1,0,0,0,159,154,1,0,0,0,159,160,1,0,0,0,160,9,1,0,0,0,
    161,164,5,11,0,0,162,163,5,12,0,0,163,165,3,72,36,0,164,162,1,0,0,0,164,
    165,1,0,0,0,165,11,1,0,0,0,166,167,5,13,0,0,167,13,1,0,0,0,168,169,5,
    14,0,0,169,170,3,72,36,0,170,15,1,0,0,0,171,172,5,5,0,0,172,17,1,0,0,
    0,173,175,5,15,0,0,174,176,3,72,36,0,175,174,1,0,0,0,175,176,1,0,0,0,
    176,19,1,0,0,0,177,179,5,16,0,0,178,180,5,17,0,0,179,178,1,0,0,0,179,
    180,1,0,0,0,180,21,1,0,0,0,181,209,5,18,0,0,182,184,5,19,0,0,183,185,
    3,24,12,0,184,183,1,0,0,0,185,186,1,0,0,0,186,184,1,0,0,0,186,187,1,0,
    0,0,187,210,1,0,0,0,188,189,5,14,0,0,189,210,3,72,36,0,190,194,7,1,0,
    0,191,193,3,26,13,0,192,191,1,0,0,0,193,196,1,0,0,0,194,192,1,0,0,0,194,
    195,1,0,0,0,195,210,1,0,0,0,196,194,1,0,0,0,197,199,5,22,0,0,198,200,
    3,28,14,0,199,198,1,0,0,0,200,201,1,0,0,0,201,199,1,0,0,0,201,202,1,0,
    0,0,202,210,1,0,0,0,203,205,5,23,0,0,204,206,3,30,15,0,205,204,1,0,0,
    0,206,207,1,0,0,0,207,205,1,0,0,0,207,208,1,0,0,0,208,210,1,0,0,0,209,
    182,1,0,0,0,209,188,1,0,0,0,209,190,1,0,0,0,209,197,1,0,0,0,209,203,1,
    0,0,0,210,23,1,0,0,0,211,212,7,2,0,0,212,216,3,72,36,0,213,214,7,3,0,
    0,214,216,5,192,0,0,215,211,1,0,0,0,215,213,1,0,0,0,216,25,1,0,0,0,217,
    218,7,4,0,0,218,225,3,72,36,0,219,220,5,33,0,0,220,225,5,192,0,0,221,
    222,5,34,0,0,222,225,5,35,0,0,223,225,5,36,0,0,224,217,1,0,0,0,224,219,
    1,0,0,0,224,221,1,0,0,0,224,223,1,0,0,0,225,27,1,0,0,0,226,227,7,5,0,
    0,227,228,3,72,36,0,228,29,1,0,0,0,229,233,3,28,14,0,230,231,5,27,0,0,
    231,233,3,72,36,0,232,229,1,0,0,0,232,230,1,0,0,0,233,31,1,0,0,0,234,
    235,5,40,0,0,235,33,1,0,0,0,236,240,5,21,0,0,237,239,3,36,18,0,238,237,
    1,0,0,0,239,242,1,0,0,0,240,238,1,0,0,0,240,241,1,0,0,0,241,35,1,0,0,
    0,242,240,1,0,0,0,243,244,7,6,0,0,244,249,3,72,36,0,245,246,5,33,0,0,
    246,249,5,192,0,0,247,249,5,42,0,0,248,243,1,0,0,0,248,245,1,0,0,0,248,
    247,1,0,0,0,249,37,1,0,0,0,250,296,5,43,0,0,251,252,5,44,0,0,252,254,
    3,72,36,0,253,255,3,40,20,0,254,253,1,0,0,0,255,256,1,0,0,0,256,254,1,
    0,0,0,256,257,1,0,0,0,257,297,1,0,0,0,258,259,5,45,0,0,259,264,3,72,36,
    0,260,263,3,42,21,0,261,263,3,98,49,0,262,260,1,0,0,0,262,261,1,0,0,0,
    263,266,1,0,0,0,264,262,1,0,0,0,264,265,1,0,0,0,265,297,1,0,0,0,266,264,
    1,0,0,0,267,268,5,46,0,0,268,271,3,72,36,0,269,270,5,47,0,0,270,272,3,
    72,36,0,271,269,1,0,0,0,271,272,1,0,0,0,272,277,1,0,0,0,273,276,3,44,
    22,0,274,276,3,98,49,0,275,273,1,0,0,0,275,274,1,0,0,0,276,279,1,0,0,
    0,277,275,1,0,0,0,277,278,1,0,0,0,278,297,1,0,0,0,279,277,1,0,0,0,280,
    281,5,48,0,0,281,282,3,72,36,0,282,283,5,47,0,0,283,288,3,72,36,0,284,
    287,3,42,21,0,285,287,3,98,49,0,286,284,1,0,0,0,286,285,1,0,0,0,287,290,
    1,0,0,0,288,286,1,0,0,0,288,289,1,0,0,0,289,297,1,0,0,0,290,288,1,0,0,
    0,291,292,5,49,0,0,292,294,5,192,0,0,293,295,3,98,49,0,294,293,1,0,0,
    0,294,295,1,0,0,0,295,297,1,0,0,0,296,251,1,0,0,0,296,258,1,0,0,0,296,
    267,1,0,0,0,296,280,1,0,0,0,296,291,1,0,0,0,297,303,1,0,0,0,298,300,5,
    50,0,0,299,301,3,98,49,0,300,299,1,0,0,0,300,301,1,0,0,0,301,303,1,0,
    0,0,302,250,1,0,0,0,302,298,1,0,0,0,303,39,1,0,0,0,304,305,7,7,0,0,305,
    313,3,72,36,0,306,307,5,199,0,0,307,313,3,72,36,0,308,313,5,200,0,0,309,
    313,5,195,0,0,310,313,3,96,48,0,311,313,3,98,49,0,312,304,1,0,0,0,312,
    306,1,0,0,0,312,308,1,0,0,0,312,309,1,0,0,0,312,310,1,0,0,0,312,311,1,
    0,0,0,313,41,1,0,0,0,314,315,7,8,0,0,315,318,3,72,36,0,316,318,3,96,48,
    0,317,314,1,0,0,0,317,316,1,0,0,0,318,43,1,0,0,0,319,330,3,42,21,0,320,
    321,5,59,0,0,321,330,7,9,0,0,322,330,5,60,0,0,323,330,5,61,0,0,324,330,
    5,201,0,0,325,330,5,62,0,0,326,330,3,92,46,0,327,328,7,10,0,0,328,330,
    3,72,36,0,329,319,1,0,0,0,329,320,1,0,0,0,329,322,1,0,0,0,329,323,1,0,
    0,0,329,324,1,0,0,0,329,325,1,0,0,0,329,326,1,0,0,0,329,327,1,0,0,0,330,
    45,1,0,0,0,331,361,5,65,0,0,332,333,5,66,0,0,333,334,3,72,36,0,334,335,
    5,67,0,0,335,336,3,72,36,0,336,362,1,0,0,0,337,338,5,68,0,0,338,362,3,
    72,36,0,339,340,5,22,0,0,340,362,3,72,36,0,341,342,5,48,0,0,342,343,3,
    72,36,0,343,344,5,69,0,0,344,348,3,72,36,0,345,347,3,42,21,0,346,345,
    1,0,0,0,347,350,1,0,0,0,348,346,1,0,0,0,348,349,1,0,0,0,349,362,1,0,0,
    0,350,348,1,0,0,0,351,352,5,70,0,0,352,353,3,72,36,0,353,354,5,69,0,0,
    354,358,3,72,36,0,355,357,3,44,22,0,356,355,1,0,0,0,357,360,1,0,0,0,358,
    356,1,0,0,0,358,359,1,0,0,0,359,362,1,0,0,0,360,358,1,0,0,0,361,332,1,
    0,0,0,361,337,1,0,0,0,361,339,1,0,0,0,361,341,1,0,0,0,361,351,1,0,0,0,
    362,47,1,0,0,0,363,417,5,71,0,0,364,366,3,54,27,0,365,364,1,0,0,0,366,
    369,1,0,0,0,367,365,1,0,0,0,367,368,1,0,0,0,368,418,1,0,0,0,369,367,1,
    0,0,0,370,371,5,72,0,0,371,374,3,72,36,0,372,373,5,33,0,0,373,375,5,192,
    0,0,374,372,1,0,0,0,374,375,1,0,0,0,375,418,1,0,0,0,376,377,5,22,0,0,
    377,378,3,72,36,0,378,379,5,31,0,0,379,380,5,192,0,0,380,418,1,0,0,0,
    381,382,5,73,0,0,382,383,3,72,36,0,383,384,5,74,0,0,384,385,5,192,0,0,
    385,386,5,75,0,0,386,391,5,192,0,0,387,388,5,76,0,0,388,390,3,72,36,0,
    389,387,1,0,0,0,390,393,1,0,0,0,391,389,1,0,0,0,391,392,1,0,0,0,392,418,
    1,0,0,0,393,391,1,0,0,0,394,395,5,197,0,0,395,396,3,72,36,0,396,397,5,
    31,0,0,397,398,5,192,0,0,398,418,1,0,0,0,399,400,5,77,0,0,400,418,3,72,
    36,0,401,402,5,78,0,0,402,418,3,72,36,0,403,404,5,79,0,0,404,418,3,72,
    36,0,405,406,5,80,0,0,406,407,3,72,36,0,407,408,5,67,0,0,408,409,3,72,
    36,0,409,418,1,0,0,0,410,411,5,81,0,0,411,412,3,72,36,0,412,413,5,34,
    0,0,413,414,5,35,0,0,414,415,5,27,0,0,415,416,3,72,36,0,416,418,1,0,0,
    0,417,367,1,0,0,0,417,370,1,0,0,0,417,376,1,0,0,0,417,381,1,0,0,0,417,
    394,1,0,0,0,417,399,1,0,0,0,417,401,1,0,0,0,417,403,1,0,0,0,417,405,1,
    0,0,0,417,410,1,0,0,0,418,49,1,0,0,0,419,423,5,82,0,0,420,422,3,54,27,
    0,421,420,1,0,0,0,422,425,1,0,0,0,423,421,1,0,0,0,423,424,1,0,0,0,424,
    51,1,0,0,0,425,423,1,0,0,0,426,427,5,83,0,0,427,432,3,72,36,0,428,431,
    3,54,27,0,429,431,5,196,0,0,430,428,1,0,0,0,430,429,1,0,0,0,431,434,1,
    0,0,0,432,430,1,0,0,0,432,433,1,0,0,0,433,53,1,0,0,0,434,432,1,0,0,0,
    435,436,7,11,0,0,436,443,5,192,0,0,437,438,5,88,0,0,438,443,7,12,0,0,
    439,440,7,13,0,0,440,443,3,72,36,0,441,443,5,93,0,0,442,435,1,0,0,0,442,
    437,1,0,0,0,442,439,1,0,0,0,442,441,1,0,0,0,443,55,1,0,0,0,444,473,5,
    94,0,0,445,446,5,66,0,0,446,474,3,72,36,0,447,474,5,95,0,0,448,449,7,
    14,0,0,449,454,3,72,36,0,450,451,7,15,0,0,451,453,3,72,36,0,452,450,1,
    0,0,0,453,456,1,0,0,0,454,452,1,0,0,0,454,455,1,0,0,0,455,474,1,0,0,0,
    456,454,1,0,0,0,457,471,5,98,0,0,458,461,5,99,0,0,459,462,3,72,36,0,460,
    462,5,194,0,0,461,459,1,0,0,0,461,460,1,0,0,0,462,463,1,0,0,0,463,461,
    1,0,0,0,463,464,1,0,0,0,464,472,1,0,0,0,465,472,5,79,0,0,466,467,5,100,
    0,0,467,468,3,72,36,0,468,469,5,24,0,0,469,470,3,72,36,0,470,472,1,0,
    0,0,471,458,1,0,0,0,471,465,1,0,0,0,471,466,1,0,0,0,471,472,1,0,0,0,472,
    474,1,0,0,0,473,445,1,0,0,0,473,447,1,0,0,0,473,448,1,0,0,0,473,457,1,
    0,0,0,474,57,1,0,0,0,475,476,5,101,0,0,476,477,3,72,36,0,477,59,1,0,0,
    0,478,479,5,102,0,0,479,480,3,72,36,0,480,61,1,0,0,0,481,482,5,103,0,
    0,482,483,3,72,36,0,483,63,1,0,0,0,484,493,5,104,0,0,485,487,5,1,0,0,
    486,485,1,0,0,0,486,487,1,0,0,0,487,489,1,0,0,0,488,490,3,72,36,0,489,
    488,1,0,0,0,490,491,1,0,0,0,491,489,1,0,0,0,491,492,1,0,0,0,492,494,1,
    0,0,0,493,486,1,0,0,0,493,494,1,0,0,0,494,65,1,0,0,0,495,496,7,16,0,0,
    496,67,1,0,0,0,497,513,5,107,0,0,498,500,5,108,0,0,499,501,3,70,35,0,
    500,499,1,0,0,0,501,502,1,0,0,0,502,500,1,0,0,0,502,503,1,0,0,0,503,514,
    1,0,0,0,504,505,5,109,0,0,505,506,5,110,0,0,506,509,3,72,36,0,507,508,
    5,47,0,0,508,510,3,72,36,0,509,507,1,0,0,0,509,510,1,0,0,0,510,514,1,
    0,0,0,511,512,5,49,0,0,512,514,3,72,36,0,513,498,1,0,0,0,513,504,1,0,
    0,0,513,511,1,0,0,0,514,69,1,0,0,0,515,516,7,17,0,0,516,517,3,72,36,0,
    517,71,1,0,0,0,518,523,5,193,0,0,519,523,5,202,0,0,520,523,5,192,0,0,
    521,523,3,74,37,0,522,518,1,0,0,0,522,519,1,0,0,0,522,520,1,0,0,0,522,
    521,1,0,0,0,523,73,1,0,0,0,524,525,7,18,0,0,525,75,1,0,0,0,526,561,5,
    121,0,0,527,530,7,19,0,0,528,531,3,72,36,0,529,531,3,80,40,0,530,528,
    1,0,0,0,530,529,1,0,0,0,531,532,1,0,0,0,532,530,1,0,0,0,532,533,1,0,0,
    0,533,562,1,0,0,0,534,562,5,5,0,0,535,537,5,123,0,0,536,538,5,192,0,0,
    537,536,1,0,0,0,537,538,1,0,0,0,538,562,1,0,0,0,539,540,5,124,0,0,540,
    542,3,72,36,0,541,543,3,80,40,0,542,541,1,0,0,0,543,544,1,0,0,0,544,542,
    1,0,0,0,544,545,1,0,0,0,545,562,1,0,0,0,546,547,5,125,0,0,547,552,3,72,
    36,0,548,551,3,80,40,0,549,551,3,78,39,0,550,548,1,0,0,0,550,549,1,0,
    0,0,551,554,1,0,0,0,552,550,1,0,0,0,552,553,1,0,0,0,553,562,1,0,0,0,554,
    552,1,0,0,0,555,557,5,126,0,0,556,558,3,80,40,0,557,556,1,0,0,0,558,559,
    1,0,0,0,559,557,1,0,0,0,559,560,1,0,0,0,560,562,1,0,0,0,561,527,1,0,0,
    0,561,534,1,0,0,0,561,535,1,0,0,0,561,539,1,0,0,0,561,546,1,0,0,0,561,
    555,1,0,0,0,562,77,1,0,0,0,563,565,5,127,0,0,564,566,3,72,36,0,565,564,
    1,0,0,0,566,567,1,0,0,0,567,565,1,0,0,0,567,568,1,0,0,0,568,79,1,0,0,
    0,569,570,7,20,0,0,570,574,3,72,36,0,571,572,5,139,0,0,572,574,5,192,
    0,0,573,569,1,0,0,0,573,571,1,0,0,0,574,81,1,0,0,0,575,595,5,120,0,0,
    576,578,5,44,0,0,577,579,3,72,36,0,578,577,1,0,0,0,578,579,1,0,0,0,579,
    581,1,0,0,0,580,582,3,84,42,0,581,580,1,0,0,0,582,583,1,0,0,0,583,581,
    1,0,0,0,583,584,1,0,0,0,584,596,1,0,0,0,585,587,7,21,0,0,586,588,3,84,
    42,0,587,586,1,0,0,0,588,589,1,0,0,0,589,587,1,0,0,0,589,590,1,0,0,0,
    590,596,1,0,0,0,591,593,7,22,0,0,592,594,5,192,0,0,593,592,1,0,0,0,593,
    594,1,0,0,0,594,596,1,0,0,0,595,576,1,0,0,0,595,585,1,0,0,0,595,591,1,
    0,0,0,596,83,1,0,0,0,597,598,7,23,0,0,598,609,3,72,36,0,599,600,7,24,
    0,0,600,609,5,192,0,0,601,609,5,177,0,0,602,609,5,178,0,0,603,609,5,179,
    0,0,604,609,5,180,0,0,605,609,5,181,0,0,606,609,5,182,0,0,607,609,5,183,
    0,0,608,597,1,0,0,0,608,599,1,0,0,0,608,601,1,0,0,0,608,602,1,0,0,0,608,
    603,1,0,0,0,608,604,1,0,0,0,608,605,1,0,0,0,608,606,1,0,0,0,608,607,1,
    0,0,0,609,85,1,0,0,0,610,634,5,114,0,0,611,635,5,115,0,0,612,613,5,116,
    0,0,613,614,3,72,36,0,614,615,5,26,0,0,615,620,3,72,36,0,616,617,5,84,
    0,0,617,621,5,192,0,0,618,619,5,37,0,0,619,621,3,72,36,0,620,616,1,0,
    0,0,620,618,1,0,0,0,621,635,1,0,0,0,622,623,5,117,0,0,623,624,3,72,36,
    0,624,625,5,26,0,0,625,626,3,72,36,0,626,627,5,84,0,0,627,628,5,192,0,
    0,628,629,5,184,0,0,629,632,3,72,36,0,630,631,5,33,0,0,631,633,5,192,
    0,0,632,630,1,0,0,0,632,633,1,0,0,0,633,635,1,0,0,0,634,611,1,0,0,0,634,
    612,1,0,0,0,634,622,1,0,0,0,635,87,1,0,0,0,636,637,5,43,0,0,637,654,5,
    118,0,0,638,639,3,72,36,0,639,640,5,26,0,0,640,643,3,72,36,0,641,642,
    5,25,0,0,642,644,3,72,36,0,643,641,1,0,0,0,643,644,1,0,0,0,644,655,1,
    0,0,0,645,646,5,25,0,0,646,647,3,72,36,0,647,648,5,185,0,0,648,649,3,
    72,36,0,649,650,5,186,0,0,650,651,3,72,36,0,651,652,5,26,0,0,652,653,
    3,72,36,0,653,655,1,0,0,0,654,638,1,0,0,0,654,645,1,0,0,0,655,657,1,0,
    0,0,656,658,3,98,49,0,657,656,1,0,0,0,657,658,1,0,0,0,658,89,1,0,0,0,
    659,660,5,43,0,0,660,661,5,119,0,0,661,662,5,192,0,0,662,663,5,47,0,0,
    663,665,3,72,36,0,664,666,3,98,49,0,665,664,1,0,0,0,665,666,1,0,0,0,666,
    91,1,0,0,0,667,668,5,187,0,0,668,669,3,72,36,0,669,93,1,0,0,0,670,671,
    7,25,0,0,671,672,5,192,0,0,672,95,1,0,0,0,673,674,5,190,0,0,674,683,3,
    72,36,0,675,678,5,191,0,0,676,679,3,72,36,0,677,679,5,194,0,0,678,676,
    1,0,0,0,678,677,1,0,0,0,679,680,1,0,0,0,680,678,1,0,0,0,680,681,1,0,0,
    0,681,683,1,0,0,0,682,673,1,0,0,0,682,675,1,0,0,0,683,97,1,0,0,0,684,
    685,5,198,0,0,685,686,3,72,36,0,686,99,1,0,0,0,83,132,139,146,151,157,
    159,164,175,179,186,194,201,207,209,215,224,232,240,248,256,262,264,271,
    275,277,286,288,294,296,300,302,312,317,329,348,358,361,367,374,391,417,
    423,430,432,442,454,461,463,471,473,486,491,493,502,509,513,522,530,532,
    537,544,550,552,559,561,567,573,578,583,589,593,595,608,620,632,634,643,
    654,657,665,678,680,682
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
    setState(132);
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
      moduleIndex();
      break;
    }

    case 10: {
      enterOuterAlt(_localctx, 10);
      setState(113);
      threads();
      break;
    }

    case 11: {
      enterOuterAlt(_localctx, 11);
      setState(114);
      symbols();
      break;
    }

    case 12: {
      enterOuterAlt(_localctx, 12);
      setState(115);
      probe();
      break;
    }

    case 13: {
      enterOuterAlt(_localctx, 13);
      setState(116);
      interceptor();
      break;
    }

    case 14: {
      enterOuterAlt(_localctx, 14);
      setState(117);
      events();
      break;
    }

    case 15: {
      enterOuterAlt(_localctx, 15);
      setState(118);
      collect();
      break;
    }

    case 16: {
      enterOuterAlt(_localctx, 16);
      setState(119);
      record();
      break;
    }

    case 17: {
      enterOuterAlt(_localctx, 17);
      setState(120);
      cpp();
      break;
    }

    case 18: {
      enterOuterAlt(_localctx, 18);
      setState(121);
      shell();
      break;
    }

    case 19: {
      enterOuterAlt(_localctx, 19);
      setState(122);
      complete();
      break;
    }

    case 20: {
      enterOuterAlt(_localctx, 20);
      setState(123);
      type();
      break;
    }

    case 21: {
      enterOuterAlt(_localctx, 21);
      setState(124);
      help();
      break;
    }

    case 22: {
      enterOuterAlt(_localctx, 22);
      setState(125);
      quit();
      break;
    }

    case 23: {
      enterOuterAlt(_localctx, 23);
      setState(126);
      bulkTrace();
      break;
    }

    case 24: {
      enterOuterAlt(_localctx, 24);
      setState(127);
      memory();
      break;
    }

    case 25: {
      enterOuterAlt(_localctx, 25);
      setState(128);
      probeEntry();
      break;
    }

    case 26: {
      enterOuterAlt(_localctx, 26);
      setState(129);
      probeReload();
      break;
    }

    case 27: {
      enterOuterAlt(_localctx, 27);
      setState(130);
      objectWatch();
      break;
    }

    case 28: {
      enterOuterAlt(_localctx, 28);
      setState(131);
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
    setState(134);
    match(TracerCommandsParser::T__1);
    setState(146);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__2: {
        setState(135);
        match(TracerCommandsParser::T__2);
        setState(139);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__6

        || _la == TracerCommandsParser::T__7 || _la == TracerCommandsParser::T__187

        || _la == TracerCommandsParser::T__188) {
          setState(136);
          serverOption();
          setState(141);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__3: {
        setState(142);
        match(TracerCommandsParser::T__3);
        setState(143);
        value();
        break;
      }

      case TracerCommandsParser::T__4: {
        setState(144);
        match(TracerCommandsParser::T__4);
        break;
      }

      case TracerCommandsParser::T__5: {
        setState(145);
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
    setState(151);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__6:
      case TracerCommandsParser::T__7: {
        enterOuterAlt(_localctx, 1);
        setState(148);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__6

        || _la == TracerCommandsParser::T__7)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(149);
        value();
        break;
      }

      case TracerCommandsParser::T__187:
      case TracerCommandsParser::T__188: {
        enterOuterAlt(_localctx, 2);
        setState(150);
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
    setState(153);
    match(TracerCommandsParser::T__8);
    setState(159);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::UINT) {
      setState(154);
      match(TracerCommandsParser::UINT);
      setState(157);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == TracerCommandsParser::T__9) {
        setState(155);
        match(TracerCommandsParser::T__9);
        setState(156);
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
    setState(161);
    match(TracerCommandsParser::T__10);
    setState(164);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::T__11) {
      setState(162);
      match(TracerCommandsParser::T__11);
      setState(163);
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
    setState(166);
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
    setState(168);
    match(TracerCommandsParser::T__13);
    setState(169);
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
    setState(171);
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
    setState(173);
    match(TracerCommandsParser::T__14);
    setState(175);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 2103400120314492) != 0) || ((((_la - 65) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 65)) & 4611158252360036587) != 0) || ((((_la - 192) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 192)) & 1059) != 0)) {
      setState(174);
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
    setState(177);
    match(TracerCommandsParser::T__15);
    setState(179);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::T__16) {
      setState(178);
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
    setState(181);
    match(TracerCommandsParser::T__17);
    setState(209);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__18: {
        setState(182);
        match(TracerCommandsParser::T__18);
        setState(184);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(183);
          moduleBuildOption();
          setState(186);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 2130706432) != 0));
        break;
      }

      case TracerCommandsParser::T__13: {
        setState(188);
        match(TracerCommandsParser::T__13);
        setState(189);
        value();
        break;
      }

      case TracerCommandsParser::T__19:
      case TracerCommandsParser::T__20: {
        setState(190);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__19

        || _la == TracerCommandsParser::T__20)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(194);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 100931735552) != 0)) {
          setState(191);
          moduleQueryOption();
          setState(196);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__21: {
        setState(197);
        match(TracerCommandsParser::T__21);
        setState(199);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(198);
          moduleShowOption();
          setState(201);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 964220157952) != 0));
        break;
      }

      case TracerCommandsParser::T__22: {
        setState(203);
        match(TracerCommandsParser::T__22);
        setState(205);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(204);
          moduleEmitOption();
          setState(207);
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
    setState(215);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__23:
      case TracerCommandsParser::T__24:
      case TracerCommandsParser::T__25:
      case TracerCommandsParser::T__26:
      case TracerCommandsParser::T__27: {
        enterOuterAlt(_localctx, 1);
        setState(211);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 520093696) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(212);
        value();
        break;
      }

      case TracerCommandsParser::T__28:
      case TracerCommandsParser::T__29: {
        enterOuterAlt(_localctx, 2);
        setState(213);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__28

        || _la == TracerCommandsParser::T__29)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(214);
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
    setState(224);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__11:
      case TracerCommandsParser::T__30:
      case TracerCommandsParser::T__31: {
        enterOuterAlt(_localctx, 1);
        setState(217);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 6442455040) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(218);
        value();
        break;
      }

      case TracerCommandsParser::T__32: {
        enterOuterAlt(_localctx, 2);
        setState(219);
        match(TracerCommandsParser::T__32);
        setState(220);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__33: {
        enterOuterAlt(_localctx, 3);
        setState(221);
        match(TracerCommandsParser::T__33);
        setState(222);
        match(TracerCommandsParser::T__34);
        break;
      }

      case TracerCommandsParser::T__35: {
        enterOuterAlt(_localctx, 4);
        setState(223);
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
    setState(226);
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 964220157952) != 0))) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(227);
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
    setState(232);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__30:
      case TracerCommandsParser::T__36:
      case TracerCommandsParser::T__37:
      case TracerCommandsParser::T__38: {
        enterOuterAlt(_localctx, 1);
        setState(229);
        moduleShowOption();
        break;
      }

      case TracerCommandsParser::T__26: {
        enterOuterAlt(_localctx, 2);
        setState(230);
        match(TracerCommandsParser::T__26);
        setState(231);
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
    setState(234);
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
    setState(236);
    match(TracerCommandsParser::T__20);
    setState(240);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 6610055335936) != 0)) {
      setState(237);
      symbolOption();
      setState(242);
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
    setState(248);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__11:
      case TracerCommandsParser::T__24:
      case TracerCommandsParser::T__25:
      case TracerCommandsParser::T__31:
      case TracerCommandsParser::T__40: {
        enterOuterAlt(_localctx, 1);
        setState(243);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 2203418890240) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(244);
        value();
        break;
      }

      case TracerCommandsParser::T__32: {
        enterOuterAlt(_localctx, 2);
        setState(245);
        match(TracerCommandsParser::T__32);
        setState(246);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__41: {
        enterOuterAlt(_localctx, 3);
        setState(247);
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
    setState(302);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__42: {
        enterOuterAlt(_localctx, 1);
        setState(250);
        match(TracerCommandsParser::T__42);
        setState(296);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__43: {
            setState(251);
            match(TracerCommandsParser::T__43);
            setState(252);
            value();
            setState(254);
            _errHandler->sync(this);
            _la = _input->LA(1);
            do {
              setState(253);
              prepareOption();
              setState(256);
              _errHandler->sync(this);
              _la = _input->LA(1);
            } while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 69806344147828736) != 0) || ((((_la - 190) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 190)) & 1827) != 0));
            break;
          }

          case TracerCommandsParser::T__44: {
            setState(258);
            match(TracerCommandsParser::T__44);
            setState(259);
            value();
            setState(264);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 520165756994846720) != 0) || ((((_la - 190) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 190)) & 259) != 0)) {
              setState(262);
              _errHandler->sync(this);
              switch (_input->LA(1)) {
                case TracerCommandsParser::T__24:
                case TracerCommandsParser::T__50:
                case TracerCommandsParser::T__51:
                case TracerCommandsParser::T__52:
                case TracerCommandsParser::T__55:
                case TracerCommandsParser::T__56:
                case TracerCommandsParser::T__57:
                case TracerCommandsParser::T__189:
                case TracerCommandsParser::T__190: {
                  setState(260);
                  resolveOption();
                  break;
                }

                case TracerCommandsParser::CWD: {
                  setState(261);
                  cwdOption();
                  break;
                }

              default:
                throw NoViableAltException(this);
              }
              setState(266);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__45: {
            setState(267);
            match(TracerCommandsParser::T__45);
            setState(268);
            value();
            setState(271);
            _errHandler->sync(this);

            _la = _input->LA(1);
            if (_la == TracerCommandsParser::T__46) {
              setState(269);
              match(TracerCommandsParser::T__46);
              setState(270);
              value();
            }
            setState(277);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while (((((_la - 25) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 25)) & 1097833906177) != 0) || ((((_la - 187) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 187)) & 18457) != 0)) {
              setState(275);
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
                case TracerCommandsParser::T__186:
                case TracerCommandsParser::T__189:
                case TracerCommandsParser::T__190:
                case TracerCommandsParser::SNAPSHOT_RETURN_POINTEE: {
                  setState(273);
                  deployOption();
                  break;
                }

                case TracerCommandsParser::CWD: {
                  setState(274);
                  cwdOption();
                  break;
                }

              default:
                throw NoViableAltException(this);
              }
              setState(279);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__47: {
            setState(280);
            match(TracerCommandsParser::T__47);
            setState(281);
            value();
            setState(282);
            match(TracerCommandsParser::T__46);
            setState(283);
            value();
            setState(288);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 520165756994846720) != 0) || ((((_la - 190) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 190)) & 259) != 0)) {
              setState(286);
              _errHandler->sync(this);
              switch (_input->LA(1)) {
                case TracerCommandsParser::T__24:
                case TracerCommandsParser::T__50:
                case TracerCommandsParser::T__51:
                case TracerCommandsParser::T__52:
                case TracerCommandsParser::T__55:
                case TracerCommandsParser::T__56:
                case TracerCommandsParser::T__57:
                case TracerCommandsParser::T__189:
                case TracerCommandsParser::T__190: {
                  setState(284);
                  resolveOption();
                  break;
                }

                case TracerCommandsParser::CWD: {
                  setState(285);
                  cwdOption();
                  break;
                }

              default:
                throw NoViableAltException(this);
              }
              setState(290);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__48: {
            setState(291);
            match(TracerCommandsParser::T__48);
            setState(292);
            match(TracerCommandsParser::UINT);
            setState(294);
            _errHandler->sync(this);

            _la = _input->LA(1);
            if (_la == TracerCommandsParser::CWD) {
              setState(293);
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
        setState(298);
        match(TracerCommandsParser::T__49);
        setState(300);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::CWD) {
          setState(299);
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
    setState(312);
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
        setState(304);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 69806344147828736) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(305);
        value();
        break;
      }

      case TracerCommandsParser::SIGNATURE: {
        enterOuterAlt(_localctx, 2);
        setState(306);
        match(TracerCommandsParser::SIGNATURE);
        setState(307);
        value();
        break;
      }

      case TracerCommandsParser::MEMBER: {
        enterOuterAlt(_localctx, 3);
        setState(308);
        match(TracerCommandsParser::MEMBER);
        break;
      }

      case TracerCommandsParser::METADATA_ONLY: {
        enterOuterAlt(_localctx, 4);
        setState(309);
        match(TracerCommandsParser::METADATA_ONLY);
        break;
      }

      case TracerCommandsParser::T__189:
      case TracerCommandsParser::T__190: {
        enterOuterAlt(_localctx, 5);
        setState(310);
        extraCompileOption();
        break;
      }

      case TracerCommandsParser::CWD: {
        enterOuterAlt(_localctx, 6);
        setState(311);
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
    setState(317);
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
        setState(314);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 520165756994846720) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(315);
        value();
        break;
      }

      case TracerCommandsParser::T__189:
      case TracerCommandsParser::T__190: {
        enterOuterAlt(_localctx, 2);
        setState(316);
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
    setState(329);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__24:
      case TracerCommandsParser::T__50:
      case TracerCommandsParser::T__51:
      case TracerCommandsParser::T__52:
      case TracerCommandsParser::T__55:
      case TracerCommandsParser::T__56:
      case TracerCommandsParser::T__57:
      case TracerCommandsParser::T__189:
      case TracerCommandsParser::T__190: {
        enterOuterAlt(_localctx, 1);
        setState(319);
        resolveOption();
        break;
      }

      case TracerCommandsParser::T__58: {
        enterOuterAlt(_localctx, 2);
        setState(320);
        match(TracerCommandsParser::T__58);
        setState(321);
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
        setState(322);
        match(TracerCommandsParser::T__59);
        break;
      }

      case TracerCommandsParser::T__60: {
        enterOuterAlt(_localctx, 4);
        setState(323);
        match(TracerCommandsParser::T__60);
        break;
      }

      case TracerCommandsParser::SNAPSHOT_RETURN_POINTEE: {
        enterOuterAlt(_localctx, 5);
        setState(324);
        match(TracerCommandsParser::SNAPSHOT_RETURN_POINTEE);
        break;
      }

      case TracerCommandsParser::T__61: {
        enterOuterAlt(_localctx, 6);
        setState(325);
        match(TracerCommandsParser::T__61);
        break;
      }

      case TracerCommandsParser::T__186: {
        enterOuterAlt(_localctx, 7);
        setState(326);
        stackFilter();
        break;
      }

      case TracerCommandsParser::T__62:
      case TracerCommandsParser::T__63: {
        enterOuterAlt(_localctx, 8);
        setState(327);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__62

        || _la == TracerCommandsParser::T__63)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(328);
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
    setState(331);
    match(TracerCommandsParser::T__64);
    setState(361);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__65: {
        setState(332);
        match(TracerCommandsParser::T__65);
        setState(333);
        value();
        setState(334);
        match(TracerCommandsParser::T__66);
        setState(335);
        value();
        break;
      }

      case TracerCommandsParser::T__67: {
        setState(337);
        match(TracerCommandsParser::T__67);
        setState(338);
        value();
        break;
      }

      case TracerCommandsParser::T__21: {
        setState(339);
        match(TracerCommandsParser::T__21);
        setState(340);
        value();
        break;
      }

      case TracerCommandsParser::T__47: {
        setState(341);
        match(TracerCommandsParser::T__47);
        setState(342);
        value();
        setState(343);
        match(TracerCommandsParser::T__68);
        setState(344);
        value();
        setState(348);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 520165756994846720) != 0) || _la == TracerCommandsParser::T__189

        || _la == TracerCommandsParser::T__190) {
          setState(345);
          resolveOption();
          setState(350);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__69: {
        setState(351);
        match(TracerCommandsParser::T__69);
        setState(352);
        value();
        setState(353);
        match(TracerCommandsParser::T__68);
        setState(354);
        value();
        setState(358);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (((((_la - 25) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 25)) & 1097833906177) != 0) || ((((_la - 187) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 187)) & 16409) != 0)) {
          setState(355);
          deployOption();
          setState(360);
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
    setState(363);
    match(TracerCommandsParser::T__70);
    setState(417);
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
        setState(367);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (((((_la - 67) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 67)) & 121503745) != 0)) {
          setState(364);
          streamOption();
          setState(369);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__71: {
        setState(370);
        match(TracerCommandsParser::T__71);
        setState(371);
        value();
        setState(374);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__32) {
          setState(372);
          match(TracerCommandsParser::T__32);
          setState(373);
          match(TracerCommandsParser::UINT);
        }
        break;
      }

      case TracerCommandsParser::T__21: {
        setState(376);
        match(TracerCommandsParser::T__21);
        setState(377);
        value();
        setState(378);
        match(TracerCommandsParser::T__30);
        setState(379);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__72: {
        setState(381);
        match(TracerCommandsParser::T__72);
        setState(382);
        value();
        setState(383);
        match(TracerCommandsParser::T__73);
        setState(384);
        match(TracerCommandsParser::UINT);
        setState(385);
        match(TracerCommandsParser::T__74);
        setState(386);
        match(TracerCommandsParser::UINT);
        setState(391);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__75) {
          setState(387);
          match(TracerCommandsParser::T__75);
          setState(388);
          value();
          setState(393);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::STACK: {
        setState(394);
        match(TracerCommandsParser::STACK);
        setState(395);
        value();
        setState(396);
        match(TracerCommandsParser::T__30);
        setState(397);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__76: {
        setState(399);
        match(TracerCommandsParser::T__76);
        setState(400);
        value();
        break;
      }

      case TracerCommandsParser::T__77: {
        setState(401);
        match(TracerCommandsParser::T__77);
        setState(402);
        value();
        break;
      }

      case TracerCommandsParser::T__78: {
        setState(403);
        match(TracerCommandsParser::T__78);
        setState(404);
        value();
        break;
      }

      case TracerCommandsParser::T__79: {
        setState(405);
        match(TracerCommandsParser::T__79);
        setState(406);
        value();
        setState(407);
        match(TracerCommandsParser::T__66);
        setState(408);
        value();
        break;
      }

      case TracerCommandsParser::T__80: {
        setState(410);
        match(TracerCommandsParser::T__80);
        setState(411);
        value();
        setState(412);
        match(TracerCommandsParser::T__33);
        setState(413);
        match(TracerCommandsParser::T__34);
        setState(414);
        match(TracerCommandsParser::T__26);
        setState(415);
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
    setState(419);
    match(TracerCommandsParser::T__81);
    setState(423);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (((((_la - 67) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 67)) & 121503745) != 0)) {
      setState(420);
      streamOption();
      setState(425);
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
    setState(426);
    match(TracerCommandsParser::T__82);
    setState(427);
    value();
    setState(432);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (((((_la - 67) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 67)) & 121503745) != 0) || _la == TracerCommandsParser::SQLITE) {
      setState(430);
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
          setState(428);
          streamOption();
          break;
        }

        case TracerCommandsParser::SQLITE: {
          setState(429);
          match(TracerCommandsParser::SQLITE);
          break;
        }

      default:
        throw NoViableAltException(this);
      }
      setState(434);
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
    setState(442);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__83:
      case TracerCommandsParser::T__84:
      case TracerCommandsParser::T__85:
      case TracerCommandsParser::T__86: {
        enterOuterAlt(_localctx, 1);
        setState(435);
        _la = _input->LA(1);
        if (!(((((_la - 84) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 84)) & 15) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(436);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__87: {
        enterOuterAlt(_localctx, 2);
        setState(437);
        match(TracerCommandsParser::T__87);
        setState(438);
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
        setState(439);
        _la = _input->LA(1);
        if (!(((((_la - 67) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 67)) & 50331649) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(440);
        value();
        break;
      }

      case TracerCommandsParser::T__92: {
        enterOuterAlt(_localctx, 4);
        setState(441);
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
    setState(444);
    match(TracerCommandsParser::T__93);
    setState(473);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__65: {
        setState(445);
        match(TracerCommandsParser::T__65);
        setState(446);
        value();
        break;
      }

      case TracerCommandsParser::T__94: {
        setState(447);
        match(TracerCommandsParser::T__94);
        break;
      }

      case TracerCommandsParser::T__95:
      case TracerCommandsParser::T__96: {
        setState(448);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__95

        || _la == TracerCommandsParser::T__96)) {
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
        while (_la == TracerCommandsParser::T__26

        || _la == TracerCommandsParser::T__36) {
          setState(450);
          _la = _input->LA(1);
          if (!(_la == TracerCommandsParser::T__26

          || _la == TracerCommandsParser::T__36)) {
          _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(451);
          value();
          setState(456);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__97: {
        setState(457);
        match(TracerCommandsParser::T__97);
        setState(471);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__98: {
            setState(458);
            match(TracerCommandsParser::T__98);
            setState(461);
            _errHandler->sync(this);
            _la = _input->LA(1);
            do {
              setState(461);
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
                  setState(459);
                  value();
                  break;
                }

                case TracerCommandsParser::SHELL: {
                  setState(460);
                  match(TracerCommandsParser::SHELL);
                  break;
                }

              default:
                throw NoViableAltException(this);
              }
              setState(463);
              _errHandler->sync(this);
              _la = _input->LA(1);
            } while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 2103400120314492) != 0) || ((((_la - 65) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 65)) & 4611158252360036587) != 0) || ((((_la - 192) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 192)) & 1063) != 0));
            break;
          }

          case TracerCommandsParser::T__78: {
            setState(465);
            match(TracerCommandsParser::T__78);
            break;
          }

          case TracerCommandsParser::T__99: {
            setState(466);
            match(TracerCommandsParser::T__99);
            setState(467);
            value();
            setState(468);
            match(TracerCommandsParser::T__23);
            setState(469);
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
    setState(475);
    match(TracerCommandsParser::T__100);
    setState(476);
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
    setState(478);
    match(TracerCommandsParser::T__101);
    setState(479);
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
    setState(481);
    match(TracerCommandsParser::T__102);
    setState(482);
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
    setState(484);
    match(TracerCommandsParser::T__103);
    setState(493);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 2103400120314494) != 0) || ((((_la - 65) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 65)) & 4611158252360036587) != 0) || ((((_la - 192) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 192)) & 1059) != 0)) {
      setState(486);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == TracerCommandsParser::T__0) {
        setState(485);
        match(TracerCommandsParser::T__0);
      }
      setState(489);
      _errHandler->sync(this);
      _la = _input->LA(1);
      do {
        setState(488);
        value();
        setState(491);
        _errHandler->sync(this);
        _la = _input->LA(1);
      } while ((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & 2103400120314492) != 0) || ((((_la - 65) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 65)) & 4611158252360036587) != 0) || ((((_la - 192) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 192)) & 1059) != 0));
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
    setState(495);
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
    setState(497);
    match(TracerCommandsParser::T__106);
    setState(513);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__107: {
        setState(498);
        match(TracerCommandsParser::T__107);
        setState(500);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(499);
          bulkTraceOption();
          setState(502);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while (_la == TracerCommandsParser::T__24

        || _la == TracerCommandsParser::T__50 || ((((_la - 110) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 110)) & 15) != 0));
        break;
      }

      case TracerCommandsParser::T__108: {
        setState(504);
        match(TracerCommandsParser::T__108);
        setState(505);
        match(TracerCommandsParser::T__109);
        setState(506);
        value();
        setState(509);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__46) {
          setState(507);
          match(TracerCommandsParser::T__46);
          setState(508);
          value();
        }
        break;
      }

      case TracerCommandsParser::T__48: {
        setState(511);
        match(TracerCommandsParser::T__48);
        setState(512);
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
    setState(515);
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
    setState(516);
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
    setState(522);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::STRING: {
        enterOuterAlt(_localctx, 1);
        setState(518);
        match(TracerCommandsParser::STRING);
        break;
      }

      case TracerCommandsParser::WORD: {
        enterOuterAlt(_localctx, 2);
        setState(519);
        match(TracerCommandsParser::WORD);
        break;
      }

      case TracerCommandsParser::UINT: {
        enterOuterAlt(_localctx, 3);
        setState(520);
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
        setState(521);
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
    setState(524);
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

std::vector<TracerCommandsParser::LiveScanSelectionContext *> TracerCommandsParser::LiveUpdateContext::liveScanSelection() {
  return getRuleContexts<TracerCommandsParser::LiveScanSelectionContext>();
}

TracerCommandsParser::LiveScanSelectionContext* TracerCommandsParser::LiveUpdateContext::liveScanSelection(size_t i) {
  return getRuleContext<TracerCommandsParser::LiveScanSelectionContext>(i);
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
    setState(526);
    match(TracerCommandsParser::T__120);
    setState(561);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__107:
      case TracerCommandsParser::T__121: {
        setState(527);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__107

        || _la == TracerCommandsParser::T__121)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(530);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(530);
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
              setState(528);
              value();
              break;
            }

            case TracerCommandsParser::T__23:
            case TracerCommandsParser::T__24:
            case TracerCommandsParser::T__36:
            case TracerCommandsParser::T__40:
            case TracerCommandsParser::T__51:
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
            case TracerCommandsParser::T__138: {
              setState(529);
              liveOption();
              break;
            }

          default:
            throw NoViableAltException(this);
          }
          setState(532);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 6609336260225660) != 0) || ((((_la - 65) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 65)) & -4612213784494739221) != 0) || ((((_la - 129) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 129)) & -9223372036854773761) != 0) || ((((_la - 193) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 193)) & 529) != 0));
        break;
      }

      case TracerCommandsParser::T__4: {
        setState(534);
        match(TracerCommandsParser::T__4);
        break;
      }

      case TracerCommandsParser::T__122: {
        setState(535);
        match(TracerCommandsParser::T__122);
        setState(537);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::UINT) {
          setState(536);
          match(TracerCommandsParser::UINT);
        }
        break;
      }

      case TracerCommandsParser::T__123: {
        setState(539);
        match(TracerCommandsParser::T__123);
        setState(540);
        value();
        setState(542);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(541);
          liveOption();
          setState(544);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 4505936139911168) != 0) || ((((_la - 128) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 128)) & 4095) != 0));
        break;
      }

      case TracerCommandsParser::T__124: {
        setState(546);
        match(TracerCommandsParser::T__124);
        setState(547);
        value();
        setState(552);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 4505936139911168) != 0) || ((((_la - 127) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 127)) & 8191) != 0)) {
          setState(550);
          _errHandler->sync(this);
          switch (_input->LA(1)) {
            case TracerCommandsParser::T__23:
            case TracerCommandsParser::T__24:
            case TracerCommandsParser::T__36:
            case TracerCommandsParser::T__40:
            case TracerCommandsParser::T__51:
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
            case TracerCommandsParser::T__138: {
              setState(548);
              liveOption();
              break;
            }

            case TracerCommandsParser::T__126: {
              setState(549);
              liveScanSelection();
              break;
            }

          default:
            throw NoViableAltException(this);
          }
          setState(554);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__125: {
        setState(555);
        match(TracerCommandsParser::T__125);
        setState(557);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(556);
          liveOption();
          setState(559);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 4505936139911168) != 0) || ((((_la - 128) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 128)) & 4095) != 0));
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

//----------------- LiveScanSelectionContext ------------------------------------------------------------------

TracerCommandsParser::LiveScanSelectionContext::LiveScanSelectionContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

std::vector<TracerCommandsParser::ValueContext *> TracerCommandsParser::LiveScanSelectionContext::value() {
  return getRuleContexts<TracerCommandsParser::ValueContext>();
}

TracerCommandsParser::ValueContext* TracerCommandsParser::LiveScanSelectionContext::value(size_t i) {
  return getRuleContext<TracerCommandsParser::ValueContext>(i);
}


size_t TracerCommandsParser::LiveScanSelectionContext::getRuleIndex() const {
  return TracerCommandsParser::RuleLiveScanSelection;
}


std::any TracerCommandsParser::LiveScanSelectionContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitLiveScanSelection(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::LiveScanSelectionContext* TracerCommandsParser::liveScanSelection() {
  LiveScanSelectionContext *_localctx = _tracker.createInstance<LiveScanSelectionContext>(_ctx, getState());
  enterRule(_localctx, 78, TracerCommandsParser::RuleLiveScanSelection);
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
    setState(563);
    match(TracerCommandsParser::T__126);
    setState(565);
    _errHandler->sync(this);
    _la = _input->LA(1);
    do {
      setState(564);
      value();
      setState(567);
      _errHandler->sync(this);
      _la = _input->LA(1);
    } while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 2103400120314492) != 0) || ((((_la - 65) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 65)) & 4611158252360036587) != 0) || ((((_la - 192) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 192)) & 1059) != 0));

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
  enterRule(_localctx, 80, TracerCommandsParser::RuleLiveOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(573);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__23:
      case TracerCommandsParser::T__24:
      case TracerCommandsParser::T__36:
      case TracerCommandsParser::T__40:
      case TracerCommandsParser::T__51:
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
        enterOuterAlt(_localctx, 1);
        setState(569);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 4505936139911168) != 0) || ((((_la - 128) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 128)) & 2047) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(570);
        value();
        break;
      }

      case TracerCommandsParser::T__138: {
        enterOuterAlt(_localctx, 2);
        setState(571);
        match(TracerCommandsParser::T__138);
        setState(572);
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
  enterRule(_localctx, 82, TracerCommandsParser::RuleObjectWatch);
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
    setState(575);
    match(TracerCommandsParser::T__119);
    setState(595);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__43: {
        setState(576);
        match(TracerCommandsParser::T__43);
        setState(578);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 2103400120314492) != 0) || ((((_la - 65) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 65)) & 4611158252360036587) != 0) || ((((_la - 192) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 192)) & 1059) != 0)) {
          setState(577);
          value();
        }
        setState(581);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(580);
          objectOption();
          setState(583);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & -8631148547175088000) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 64)) & 2101249) != 0) || ((((_la - 140) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 140)) & 17592186044415) != 0));
        break;
      }

      case TracerCommandsParser::T__2:
      case TracerCommandsParser::T__107: {
        setState(585);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__2 || _la == TracerCommandsParser::T__107)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(587);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(586);
          objectOption();
          setState(589);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & -8631148547175088000) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 64)) & 2101249) != 0) || ((((_la - 140) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 140)) & 17592186044415) != 0));
        break;
      }

      case TracerCommandsParser::T__4:
      case TracerCommandsParser::T__5: {
        setState(591);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__4

        || _la == TracerCommandsParser::T__5)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(593);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::UINT) {
          setState(592);
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
  enterRule(_localctx, 84, TracerCommandsParser::RuleObjectOption);
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
      case TracerCommandsParser::T__164:
      case TracerCommandsParser::T__165: {
        enterOuterAlt(_localctx, 1);
        setState(597);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & -9207609300552253312) != 0) || _la == TracerCommandsParser::T__63

        || _la == TracerCommandsParser::T__75 || ((((_la - 140) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 140)) & 134217727) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(598);
        value();
        break;
      }

      case TracerCommandsParser::T__29:
      case TracerCommandsParser::T__58:
      case TracerCommandsParser::T__84:
      case TracerCommandsParser::T__166:
      case TracerCommandsParser::T__167:
      case TracerCommandsParser::T__168:
      case TracerCommandsParser::T__169:
      case TracerCommandsParser::T__170:
      case TracerCommandsParser::T__171:
      case TracerCommandsParser::T__172:
      case TracerCommandsParser::T__173:
      case TracerCommandsParser::T__174:
      case TracerCommandsParser::T__175: {
        enterOuterAlt(_localctx, 2);
        setState(599);
        _la = _input->LA(1);
        if (!(((((_la - 30) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 30)) & 36028797555834881) != 0) || ((((_la - 167) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 167)) & 1023) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(600);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__176: {
        enterOuterAlt(_localctx, 3);
        setState(601);
        match(TracerCommandsParser::T__176);
        break;
      }

      case TracerCommandsParser::T__177: {
        enterOuterAlt(_localctx, 4);
        setState(602);
        match(TracerCommandsParser::T__177);
        break;
      }

      case TracerCommandsParser::T__178: {
        enterOuterAlt(_localctx, 5);
        setState(603);
        match(TracerCommandsParser::T__178);
        break;
      }

      case TracerCommandsParser::T__179: {
        enterOuterAlt(_localctx, 6);
        setState(604);
        match(TracerCommandsParser::T__179);
        break;
      }

      case TracerCommandsParser::T__180: {
        enterOuterAlt(_localctx, 7);
        setState(605);
        match(TracerCommandsParser::T__180);
        break;
      }

      case TracerCommandsParser::T__181: {
        enterOuterAlt(_localctx, 8);
        setState(606);
        match(TracerCommandsParser::T__181);
        break;
      }

      case TracerCommandsParser::T__182: {
        enterOuterAlt(_localctx, 9);
        setState(607);
        match(TracerCommandsParser::T__182);
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
  enterRule(_localctx, 86, TracerCommandsParser::RuleMemory);
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
    setState(610);
    match(TracerCommandsParser::T__113);
    setState(634);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__114: {
        setState(611);
        match(TracerCommandsParser::T__114);
        break;
      }

      case TracerCommandsParser::T__115: {
        setState(612);
        match(TracerCommandsParser::T__115);
        setState(613);
        value();
        setState(614);
        match(TracerCommandsParser::T__25);
        setState(615);
        value();
        setState(620);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__83: {
            setState(616);
            match(TracerCommandsParser::T__83);
            setState(617);
            match(TracerCommandsParser::UINT);
            break;
          }

          case TracerCommandsParser::T__36: {
            setState(618);
            match(TracerCommandsParser::T__36);
            setState(619);
            value();
            break;
          }

        default:
          throw NoViableAltException(this);
        }
        break;
      }

      case TracerCommandsParser::T__116: {
        setState(622);
        match(TracerCommandsParser::T__116);
        setState(623);
        value();
        setState(624);
        match(TracerCommandsParser::T__25);
        setState(625);
        value();
        setState(626);
        match(TracerCommandsParser::T__83);
        setState(627);
        match(TracerCommandsParser::UINT);
        setState(628);
        match(TracerCommandsParser::T__183);
        setState(629);
        value();
        setState(632);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__32) {
          setState(630);
          match(TracerCommandsParser::T__32);
          setState(631);
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
  enterRule(_localctx, 88, TracerCommandsParser::RuleProbeEntry);
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
    match(TracerCommandsParser::T__42);
    setState(637);
    match(TracerCommandsParser::T__117);
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
        setState(638);
        value();
        setState(639);
        match(TracerCommandsParser::T__25);
        setState(640);
        value();
        setState(643);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__24) {
          setState(641);
          match(TracerCommandsParser::T__24);
          setState(642);
          value();
        }
        break;
      }

      case TracerCommandsParser::T__24: {
        setState(645);
        match(TracerCommandsParser::T__24);
        setState(646);
        value();
        setState(647);
        match(TracerCommandsParser::T__184);
        setState(648);
        value();
        setState(649);
        match(TracerCommandsParser::T__185);
        setState(650);
        value();
        setState(651);
        match(TracerCommandsParser::T__25);
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
  enterRule(_localctx, 90, TracerCommandsParser::RuleProbeReload);
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
    match(TracerCommandsParser::T__42);
    setState(660);
    match(TracerCommandsParser::T__118);
    setState(661);
    match(TracerCommandsParser::UINT);
    setState(662);
    match(TracerCommandsParser::T__46);
    setState(663);
    value();
    setState(665);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::CWD) {
      setState(664);
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
    setState(667);
    match(TracerCommandsParser::T__186);
    setState(668);
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
    setState(670);
    _la = _input->LA(1);
    if (!(_la == TracerCommandsParser::T__187

    || _la == TracerCommandsParser::T__188)) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(671);
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
    setState(682);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__189: {
        enterOuterAlt(_localctx, 1);
        setState(673);
        match(TracerCommandsParser::T__189);
        setState(674);
        value();
        break;
      }

      case TracerCommandsParser::T__190: {
        enterOuterAlt(_localctx, 2);
        setState(675);
        match(TracerCommandsParser::T__190);
        setState(678);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(678);
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
              setState(676);
              value();
              break;
            }

            case TracerCommandsParser::SHELL: {
              setState(677);
              match(TracerCommandsParser::SHELL);
              break;
            }

          default:
            throw NoViableAltException(this);
          }
          setState(680);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 2103400120314492) != 0) || ((((_la - 65) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 65)) & 4611158252360036587) != 0) || ((((_la - 192) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 192)) & 1063) != 0));
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
    setState(684);
    match(TracerCommandsParser::CWD);
    setState(685);
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
