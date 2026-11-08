
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
      "launchOption", "targetCall", "localPerf", "instructions", "instructionOption",
      "watchpoint"
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
      "'--allow-replacement'", "'--reclaimable'", "'--stack-depth'", "'--print-arguments'",
      "'--print-result'", "'interceptor'", "'load'", "'--as'", "'edit'",
      "'show'", "'--target'", "'upload'", "'events'", "'list'", "'--index'",
      "'calls'", "'stats'", "'clear'", "'open'", "'export'", "'--format'",
      "'json'", "'collect'", "'record'", "'--after'", "'--probe'", "'--phase'",
      "'PRE'", "'POST'", "'--name'", "'--session'", "'--all'", "'cpp'",
      "'reset'", "'import'", "'generate'", "'--type'", "'options'", "'add'",
      "'compdb'", "'--file'", "'shell'", "'complete'", "'type'", "'help'",
      "'quit'", "'exit'", "'trace'", "'preview'", "'apply'", "'--manifest'",
      "'--exclude'", "'--exact'", "'--save'", "'memory'", "'maps'", "'read'",
      "'scan'", "'patch'", "'restore'", "'import-hook'", "'monitor-start'",
      "'monitor-stop'", "'entry'", "'reload'", "'launch'", "'resume'", "'terminate'",
      "'call'", "'call-result'", "'syscalls'", "'sample'", "'cpu'", "'instructions'",
      "'watchpoint'", "'--pattern'", "'--elf-address'", "'--expect'", "'--replace'",
      "'--tid'", "'--size'", "'--symbol'", "'--original'", "'--abi'", "'--arg'",
      "'--cwd'", "'--env'", "'--stdin'", "'--stdout'", "'--stderr'", "'--preload'",
      "'--supervise'", "'--memory-size'", "'--registers'", "'--callback-library'",
      "'--memory-address'", "'--access'", "", "", "", "'--metadata-only'",
      "'--sqlite'"
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
      "", "", "", "", "", "", "", "", "UINT", "STRING", "SHELL", "METADATA_ONLY",
      "SQLITE", "WORD", "WS"
    }
  );
  static const int32_t serializedATNSegment[] = {
    4,1,150,622,2,0,7,0,2,1,7,1,2,2,7,2,2,3,7,3,2,4,7,4,2,5,7,5,2,6,7,6,2,
    7,7,7,2,8,7,8,2,9,7,9,2,10,7,10,2,11,7,11,2,12,7,12,2,13,7,13,2,14,7,
    14,2,15,7,15,2,16,7,16,2,17,7,17,2,18,7,18,2,19,7,19,2,20,7,20,2,21,7,
    21,2,22,7,22,2,23,7,23,2,24,7,24,2,25,7,25,2,26,7,26,2,27,7,27,2,28,7,
    28,2,29,7,29,2,30,7,30,2,31,7,31,2,32,7,32,2,33,7,33,2,34,7,34,2,35,7,
    35,2,36,7,36,2,37,7,37,2,38,7,38,2,39,7,39,2,40,7,40,2,41,7,41,2,42,7,
    42,2,43,7,43,1,0,1,0,1,0,1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,3,1,123,8,1,1,2,1,2,1,2,5,2,128,8,2,10,2,12,2,131,9,2,1,2,1,
    2,1,2,1,2,3,2,137,8,2,1,3,1,3,1,3,1,4,1,4,1,4,1,4,3,4,146,8,4,3,4,148,
    8,4,1,5,1,5,1,5,3,5,153,8,5,1,6,1,6,1,7,1,7,1,7,1,8,1,8,1,9,1,9,3,9,164,
    8,9,1,10,1,10,1,10,1,10,1,10,1,10,1,10,5,10,173,8,10,10,10,12,10,176,
    9,10,3,10,178,8,10,1,11,1,11,1,12,1,12,5,12,184,8,12,10,12,12,12,187,
    9,12,1,13,1,13,1,13,1,13,1,13,3,13,194,8,13,1,14,1,14,1,14,1,14,4,14,
    200,8,14,11,14,12,14,201,1,14,1,14,1,14,5,14,207,8,14,10,14,12,14,210,
    9,14,1,14,1,14,1,14,1,14,1,14,5,14,217,8,14,10,14,12,14,220,9,14,1,14,
    1,14,1,14,1,14,1,14,5,14,227,8,14,10,14,12,14,230,9,14,1,14,1,14,3,14,
    234,8,14,1,14,3,14,237,8,14,1,15,1,15,1,15,3,15,242,8,15,1,16,1,16,1,
    16,1,17,1,17,1,17,1,17,1,17,3,17,252,8,17,1,18,1,18,1,18,1,18,1,18,3,
    18,259,8,18,1,19,1,19,1,19,1,19,1,19,1,19,1,19,1,19,1,19,1,19,1,19,1,
    19,1,19,1,19,1,19,5,19,276,8,19,10,19,12,19,279,9,19,1,19,1,19,1,19,1,
    19,1,19,5,19,286,8,19,10,19,12,19,289,9,19,3,19,291,8,19,1,20,1,20,5,
    20,295,8,20,10,20,12,20,298,9,20,1,20,1,20,1,20,1,20,3,20,304,8,20,1,
    20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,
    20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,3,20,329,8,20,1,21,1,21,5,
    21,333,8,21,10,21,12,21,336,9,21,1,22,1,22,1,22,1,22,5,22,342,8,22,10,
    22,12,22,345,9,22,1,23,1,23,1,23,1,23,1,23,1,23,1,23,3,23,354,8,23,1,
    24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,5,24,364,8,24,10,24,12,24,367,9,
    24,1,24,1,24,1,24,1,24,4,24,373,8,24,11,24,12,24,374,1,24,1,24,1,24,1,
    24,1,24,1,24,3,24,383,8,24,3,24,385,8,24,1,25,1,25,1,25,1,26,1,26,1,26,
    1,27,1,27,1,27,1,28,1,28,3,28,398,8,28,1,28,4,28,401,8,28,11,28,12,28,
    402,3,28,405,8,28,1,29,1,29,1,30,1,30,1,30,4,30,412,8,30,11,30,12,30,
    413,1,30,1,30,1,30,1,30,1,30,3,30,421,8,30,1,30,1,30,3,30,425,8,30,1,
    31,1,31,1,31,1,32,1,32,1,32,1,32,3,32,434,8,32,1,33,1,33,1,34,1,34,1,
    34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,3,34,448,8,34,1,34,1,34,1,34,1,
    34,1,34,1,34,1,34,1,34,1,34,1,34,3,34,460,8,34,1,34,1,34,1,34,1,34,1,
    34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,
    34,1,34,1,34,1,34,1,34,1,34,1,34,3,34,487,8,34,1,34,1,34,1,34,1,34,1,
    34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,3,34,504,8,34,1,
    35,1,35,1,35,1,35,1,35,1,35,1,35,3,35,513,8,35,1,35,1,35,1,35,1,35,1,
    35,1,35,1,35,1,35,1,35,3,35,524,8,35,1,36,1,36,1,36,1,36,1,36,1,36,1,
    36,1,36,5,36,534,8,36,10,36,12,36,537,9,36,3,36,539,8,36,1,37,1,37,1,
    37,5,37,544,8,37,10,37,12,37,547,9,37,1,37,1,37,3,37,551,8,37,3,37,553,
    8,37,1,38,1,38,1,38,1,38,1,38,3,38,560,8,38,1,39,1,39,1,39,1,39,1,39,
    1,39,5,39,568,8,39,10,39,12,39,571,9,39,1,39,1,39,1,39,3,39,576,8,39,
    3,39,578,8,39,1,40,1,40,1,40,3,40,583,8,40,1,40,1,40,5,40,587,8,40,10,
    40,12,40,590,9,40,1,41,1,41,1,41,5,41,595,8,41,10,41,12,41,598,9,41,1,
    41,3,41,601,8,41,1,42,1,42,1,42,1,42,1,42,1,42,1,42,3,42,610,8,42,1,43,
    1,43,1,43,1,43,1,43,5,43,617,8,43,10,43,12,43,620,9,43,1,43,0,0,44,0,
    2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,34,36,38,40,42,44,46,48,50,
    52,54,56,58,60,62,64,66,68,70,72,74,76,78,80,82,84,86,0,18,1,0,7,8,1,
    0,20,21,2,0,12,12,24,27,2,0,24,24,38,41,3,0,24,24,38,39,42,44,2,0,20,
    21,71,72,1,0,74,75,2,0,54,54,76,77,1,0,81,82,2,0,41,41,83,83,1,0,92,93,
    3,0,24,24,38,38,97,100,17,0,2,6,9,9,11,11,13,16,22,23,30,33,35,37,52,
    53,55,56,58,60,62,66,68,70,74,75,79,82,84,86,88,96,101,121,1,0,113,114,
    1,0,131,137,2,0,20,21,126,127,2,0,26,27,141,142,2,0,26,27,143,143,710,
    0,88,1,0,0,0,2,122,1,0,0,0,4,124,1,0,0,0,6,138,1,0,0,0,8,141,1,0,0,0,
    10,149,1,0,0,0,12,154,1,0,0,0,14,156,1,0,0,0,16,159,1,0,0,0,18,161,1,
    0,0,0,20,165,1,0,0,0,22,179,1,0,0,0,24,181,1,0,0,0,26,193,1,0,0,0,28,
    236,1,0,0,0,30,241,1,0,0,0,32,243,1,0,0,0,34,251,1,0,0,0,36,258,1,0,0,
    0,38,260,1,0,0,0,40,292,1,0,0,0,42,330,1,0,0,0,44,337,1,0,0,0,46,353,
    1,0,0,0,48,355,1,0,0,0,50,386,1,0,0,0,52,389,1,0,0,0,54,392,1,0,0,0,56,
    395,1,0,0,0,58,406,1,0,0,0,60,408,1,0,0,0,62,426,1,0,0,0,64,433,1,0,0,
    0,66,435,1,0,0,0,68,437,1,0,0,0,70,505,1,0,0,0,72,525,1,0,0,0,74,552,
    1,0,0,0,76,559,1,0,0,0,78,577,1,0,0,0,80,582,1,0,0,0,82,591,1,0,0,0,84,
    609,1,0,0,0,86,611,1,0,0,0,88,89,5,1,0,0,89,90,3,2,1,0,90,91,5,0,0,1,
    91,1,1,0,0,0,92,123,3,4,2,0,93,123,3,8,4,0,94,123,3,10,5,0,95,123,3,12,
    6,0,96,123,3,14,7,0,97,123,3,16,8,0,98,123,3,18,9,0,99,123,3,20,10,0,
    100,123,3,22,11,0,101,123,3,24,12,0,102,123,3,28,14,0,103,123,3,38,19,
    0,104,123,3,40,20,0,105,123,3,42,21,0,106,123,3,44,22,0,107,123,3,48,
    24,0,108,123,3,50,25,0,109,123,3,52,26,0,110,123,3,54,27,0,111,123,3,
    56,28,0,112,123,3,58,29,0,113,123,3,60,30,0,114,123,3,68,34,0,115,123,
    3,70,35,0,116,123,3,72,36,0,117,123,3,74,37,0,118,123,3,78,39,0,119,123,
    3,80,40,0,120,123,3,82,41,0,121,123,3,86,43,0,122,92,1,0,0,0,122,93,1,
    0,0,0,122,94,1,0,0,0,122,95,1,0,0,0,122,96,1,0,0,0,122,97,1,0,0,0,122,
    98,1,0,0,0,122,99,1,0,0,0,122,100,1,0,0,0,122,101,1,0,0,0,122,102,1,0,
    0,0,122,103,1,0,0,0,122,104,1,0,0,0,122,105,1,0,0,0,122,106,1,0,0,0,122,
    107,1,0,0,0,122,108,1,0,0,0,122,109,1,0,0,0,122,110,1,0,0,0,122,111,1,
    0,0,0,122,112,1,0,0,0,122,113,1,0,0,0,122,114,1,0,0,0,122,115,1,0,0,0,
    122,116,1,0,0,0,122,117,1,0,0,0,122,118,1,0,0,0,122,119,1,0,0,0,122,120,
    1,0,0,0,122,121,1,0,0,0,123,3,1,0,0,0,124,136,5,2,0,0,125,129,5,3,0,0,
    126,128,3,6,3,0,127,126,1,0,0,0,128,131,1,0,0,0,129,127,1,0,0,0,129,130,
    1,0,0,0,130,137,1,0,0,0,131,129,1,0,0,0,132,133,5,4,0,0,133,137,3,64,
    32,0,134,137,5,5,0,0,135,137,5,6,0,0,136,125,1,0,0,0,136,132,1,0,0,0,
    136,134,1,0,0,0,136,135,1,0,0,0,137,5,1,0,0,0,138,139,7,0,0,0,139,140,
    3,64,32,0,140,7,1,0,0,0,141,147,5,9,0,0,142,145,5,144,0,0,143,144,5,10,
    0,0,144,146,5,144,0,0,145,143,1,0,0,0,145,146,1,0,0,0,146,148,1,0,0,0,
    147,142,1,0,0,0,147,148,1,0,0,0,148,9,1,0,0,0,149,152,5,11,0,0,150,151,
    5,12,0,0,151,153,3,64,32,0,152,150,1,0,0,0,152,153,1,0,0,0,153,11,1,0,
    0,0,154,155,5,13,0,0,155,13,1,0,0,0,156,157,5,14,0,0,157,158,3,64,32,
    0,158,15,1,0,0,0,159,160,5,5,0,0,160,17,1,0,0,0,161,163,5,15,0,0,162,
    164,3,64,32,0,163,162,1,0,0,0,163,164,1,0,0,0,164,19,1,0,0,0,165,177,
    5,16,0,0,166,178,5,17,0,0,167,168,5,18,0,0,168,178,3,64,32,0,169,174,
    5,19,0,0,170,171,7,1,0,0,171,173,5,144,0,0,172,170,1,0,0,0,173,176,1,
    0,0,0,174,172,1,0,0,0,174,175,1,0,0,0,175,178,1,0,0,0,176,174,1,0,0,0,
    177,166,1,0,0,0,177,167,1,0,0,0,177,169,1,0,0,0,177,178,1,0,0,0,178,21,
    1,0,0,0,179,180,5,22,0,0,180,23,1,0,0,0,181,185,5,23,0,0,182,184,3,26,
    13,0,183,182,1,0,0,0,184,187,1,0,0,0,185,183,1,0,0,0,185,186,1,0,0,0,
    186,25,1,0,0,0,187,185,1,0,0,0,188,189,7,2,0,0,189,194,3,64,32,0,190,
    191,5,28,0,0,191,194,5,144,0,0,192,194,5,29,0,0,193,188,1,0,0,0,193,190,
    1,0,0,0,193,192,1,0,0,0,194,27,1,0,0,0,195,233,5,30,0,0,196,197,5,31,
    0,0,197,199,3,64,32,0,198,200,3,30,15,0,199,198,1,0,0,0,200,201,1,0,0,
    0,201,199,1,0,0,0,201,202,1,0,0,0,202,234,1,0,0,0,203,204,5,32,0,0,204,
    208,3,64,32,0,205,207,3,32,16,0,206,205,1,0,0,0,207,210,1,0,0,0,208,206,
    1,0,0,0,208,209,1,0,0,0,209,234,1,0,0,0,210,208,1,0,0,0,211,212,5,33,
    0,0,212,213,3,64,32,0,213,214,5,34,0,0,214,218,3,64,32,0,215,217,3,36,
    18,0,216,215,1,0,0,0,217,220,1,0,0,0,218,216,1,0,0,0,218,219,1,0,0,0,
    219,234,1,0,0,0,220,218,1,0,0,0,221,222,5,35,0,0,222,223,3,64,32,0,223,
    224,5,34,0,0,224,228,3,64,32,0,225,227,3,34,17,0,226,225,1,0,0,0,227,
    230,1,0,0,0,228,226,1,0,0,0,228,229,1,0,0,0,229,234,1,0,0,0,230,228,1,
    0,0,0,231,232,5,36,0,0,232,234,5,144,0,0,233,196,1,0,0,0,233,203,1,0,
    0,0,233,211,1,0,0,0,233,221,1,0,0,0,233,231,1,0,0,0,234,237,1,0,0,0,235,
    237,5,37,0,0,236,195,1,0,0,0,236,235,1,0,0,0,237,29,1,0,0,0,238,239,7,
    3,0,0,239,242,3,64,32,0,240,242,5,147,0,0,241,238,1,0,0,0,241,240,1,0,
    0,0,242,31,1,0,0,0,243,244,7,4,0,0,244,245,3,64,32,0,245,33,1,0,0,0,246,
    252,3,32,16,0,247,252,5,45,0,0,248,252,5,46,0,0,249,252,5,47,0,0,250,
    252,5,48,0,0,251,246,1,0,0,0,251,247,1,0,0,0,251,248,1,0,0,0,251,249,
    1,0,0,0,251,250,1,0,0,0,252,35,1,0,0,0,253,259,3,34,17,0,254,255,5,49,
    0,0,255,259,5,144,0,0,256,259,5,50,0,0,257,259,5,51,0,0,258,253,1,0,0,
    0,258,254,1,0,0,0,258,256,1,0,0,0,258,257,1,0,0,0,259,37,1,0,0,0,260,
    290,5,52,0,0,261,262,5,53,0,0,262,263,3,64,32,0,263,264,5,54,0,0,264,
    265,3,64,32,0,265,291,1,0,0,0,266,267,5,55,0,0,267,291,3,64,32,0,268,
    269,5,56,0,0,269,291,3,64,32,0,270,271,5,35,0,0,271,272,3,64,32,0,272,
    273,5,57,0,0,273,277,3,64,32,0,274,276,3,34,17,0,275,274,1,0,0,0,276,
    279,1,0,0,0,277,275,1,0,0,0,277,278,1,0,0,0,278,291,1,0,0,0,279,277,1,
    0,0,0,280,281,5,58,0,0,281,282,3,64,32,0,282,283,5,57,0,0,283,287,3,64,
    32,0,284,286,3,36,18,0,285,284,1,0,0,0,286,289,1,0,0,0,287,285,1,0,0,
    0,287,288,1,0,0,0,288,291,1,0,0,0,289,287,1,0,0,0,290,261,1,0,0,0,290,
    266,1,0,0,0,290,268,1,0,0,0,290,270,1,0,0,0,290,280,1,0,0,0,291,39,1,
    0,0,0,292,328,5,59,0,0,293,295,3,46,23,0,294,293,1,0,0,0,295,298,1,0,
    0,0,296,294,1,0,0,0,296,297,1,0,0,0,297,329,1,0,0,0,298,296,1,0,0,0,299,
    300,5,60,0,0,300,303,3,64,32,0,301,302,5,28,0,0,302,304,5,144,0,0,303,
    301,1,0,0,0,303,304,1,0,0,0,304,329,1,0,0,0,305,306,5,56,0,0,306,307,
    3,64,32,0,307,308,5,61,0,0,308,309,5,144,0,0,309,329,1,0,0,0,310,311,
    5,62,0,0,311,329,3,64,32,0,312,313,5,63,0,0,313,329,3,64,32,0,314,315,
    5,64,0,0,315,329,3,64,32,0,316,317,5,65,0,0,317,318,3,64,32,0,318,319,
    5,54,0,0,319,320,3,64,32,0,320,329,1,0,0,0,321,322,5,66,0,0,322,323,3,
    64,32,0,323,324,5,67,0,0,324,325,5,68,0,0,325,326,5,41,0,0,326,327,3,
    64,32,0,327,329,1,0,0,0,328,296,1,0,0,0,328,299,1,0,0,0,328,305,1,0,0,
    0,328,310,1,0,0,0,328,312,1,0,0,0,328,314,1,0,0,0,328,316,1,0,0,0,328,
    321,1,0,0,0,329,41,1,0,0,0,330,334,5,69,0,0,331,333,3,46,23,0,332,331,
    1,0,0,0,333,336,1,0,0,0,334,332,1,0,0,0,334,335,1,0,0,0,335,43,1,0,0,
    0,336,334,1,0,0,0,337,338,5,70,0,0,338,343,3,64,32,0,339,342,3,46,23,
    0,340,342,5,148,0,0,341,339,1,0,0,0,341,340,1,0,0,0,342,345,1,0,0,0,343,
    341,1,0,0,0,343,344,1,0,0,0,344,45,1,0,0,0,345,343,1,0,0,0,346,347,7,
    5,0,0,347,354,5,144,0,0,348,349,5,73,0,0,349,354,7,6,0,0,350,351,7,7,
    0,0,351,354,3,64,32,0,352,354,5,78,0,0,353,346,1,0,0,0,353,348,1,0,0,
    0,353,350,1,0,0,0,353,352,1,0,0,0,354,47,1,0,0,0,355,384,5,79,0,0,356,
    357,5,53,0,0,357,385,3,64,32,0,358,385,5,80,0,0,359,360,7,8,0,0,360,365,
    3,64,32,0,361,362,7,9,0,0,362,364,3,64,32,0,363,361,1,0,0,0,364,367,1,
    0,0,0,365,363,1,0,0,0,365,366,1,0,0,0,366,385,1,0,0,0,367,365,1,0,0,0,
    368,382,5,84,0,0,369,372,5,85,0,0,370,373,3,64,32,0,371,373,5,146,0,0,
    372,370,1,0,0,0,372,371,1,0,0,0,373,374,1,0,0,0,374,372,1,0,0,0,374,375,
    1,0,0,0,375,383,1,0,0,0,376,383,5,64,0,0,377,378,5,86,0,0,378,379,3,64,
    32,0,379,380,5,87,0,0,380,381,3,64,32,0,381,383,1,0,0,0,382,369,1,0,0,
    0,382,376,1,0,0,0,382,377,1,0,0,0,382,383,1,0,0,0,383,385,1,0,0,0,384,
    356,1,0,0,0,384,358,1,0,0,0,384,359,1,0,0,0,384,368,1,0,0,0,385,49,1,
    0,0,0,386,387,5,88,0,0,387,388,3,64,32,0,388,51,1,0,0,0,389,390,5,89,
    0,0,390,391,3,64,32,0,391,53,1,0,0,0,392,393,5,90,0,0,393,394,3,64,32,
    0,394,55,1,0,0,0,395,404,5,91,0,0,396,398,5,1,0,0,397,396,1,0,0,0,397,
    398,1,0,0,0,398,400,1,0,0,0,399,401,3,64,32,0,400,399,1,0,0,0,401,402,
    1,0,0,0,402,400,1,0,0,0,402,403,1,0,0,0,403,405,1,0,0,0,404,397,1,0,0,
    0,404,405,1,0,0,0,405,57,1,0,0,0,406,407,7,10,0,0,407,59,1,0,0,0,408,
    424,5,94,0,0,409,411,5,95,0,0,410,412,3,62,31,0,411,410,1,0,0,0,412,413,
    1,0,0,0,413,411,1,0,0,0,413,414,1,0,0,0,414,425,1,0,0,0,415,416,5,96,
    0,0,416,417,5,97,0,0,417,420,3,64,32,0,418,419,5,34,0,0,419,421,3,64,
    32,0,420,418,1,0,0,0,420,421,1,0,0,0,421,425,1,0,0,0,422,423,5,36,0,0,
    423,425,3,64,32,0,424,409,1,0,0,0,424,415,1,0,0,0,424,422,1,0,0,0,425,
    61,1,0,0,0,426,427,7,11,0,0,427,428,3,64,32,0,428,63,1,0,0,0,429,434,
    5,145,0,0,430,434,5,149,0,0,431,434,5,144,0,0,432,434,3,66,33,0,433,429,
    1,0,0,0,433,430,1,0,0,0,433,431,1,0,0,0,433,432,1,0,0,0,434,65,1,0,0,
    0,435,436,7,12,0,0,436,67,1,0,0,0,437,503,5,101,0,0,438,504,5,102,0,0,
    439,440,5,103,0,0,440,441,3,64,32,0,441,442,5,27,0,0,442,447,3,64,32,
    0,443,444,5,21,0,0,444,448,5,144,0,0,445,446,5,83,0,0,446,448,3,64,32,
    0,447,443,1,0,0,0,447,445,1,0,0,0,448,504,1,0,0,0,449,450,5,104,0,0,450,
    451,3,64,32,0,451,452,5,27,0,0,452,453,3,64,32,0,453,454,5,21,0,0,454,
    455,5,144,0,0,455,456,5,122,0,0,456,459,3,64,32,0,457,458,5,28,0,0,458,
    460,5,144,0,0,459,457,1,0,0,0,459,460,1,0,0,0,460,504,1,0,0,0,461,462,
    5,105,0,0,462,463,5,24,0,0,463,464,3,64,32,0,464,465,5,123,0,0,465,466,
    3,64,32,0,466,467,5,27,0,0,467,468,3,64,32,0,468,469,5,124,0,0,469,470,
    3,64,32,0,470,471,5,125,0,0,471,472,3,64,32,0,472,504,1,0,0,0,473,474,
    5,106,0,0,474,504,5,144,0,0,475,476,5,108,0,0,476,477,5,126,0,0,477,478,
    5,144,0,0,478,479,5,26,0,0,479,480,3,64,32,0,480,481,5,127,0,0,481,482,
    5,144,0,0,482,483,5,27,0,0,483,486,3,64,32,0,484,485,5,20,0,0,485,487,
    5,144,0,0,486,484,1,0,0,0,486,487,1,0,0,0,487,504,1,0,0,0,488,504,5,109,
    0,0,489,490,5,107,0,0,490,491,5,24,0,0,491,492,3,64,32,0,492,493,5,128,
    0,0,493,494,3,64,32,0,494,495,5,129,0,0,495,496,3,64,32,0,496,497,5,26,
    0,0,497,498,3,64,32,0,498,499,5,27,0,0,499,500,3,64,32,0,500,501,5,130,
    0,0,501,502,3,64,32,0,502,504,1,0,0,0,503,438,1,0,0,0,503,439,1,0,0,0,
    503,449,1,0,0,0,503,461,1,0,0,0,503,473,1,0,0,0,503,475,1,0,0,0,503,488,
    1,0,0,0,503,489,1,0,0,0,504,69,1,0,0,0,505,506,5,30,0,0,506,523,5,110,
    0,0,507,508,3,64,32,0,508,509,5,27,0,0,509,512,3,64,32,0,510,511,5,24,
    0,0,511,513,3,64,32,0,512,510,1,0,0,0,512,513,1,0,0,0,513,524,1,0,0,0,
    514,515,5,24,0,0,515,516,3,64,32,0,516,517,5,123,0,0,517,518,3,64,32,
    0,518,519,5,130,0,0,519,520,3,64,32,0,520,521,5,27,0,0,521,522,3,64,32,
    0,522,524,1,0,0,0,523,507,1,0,0,0,523,514,1,0,0,0,524,71,1,0,0,0,525,
    526,5,30,0,0,526,527,5,111,0,0,527,528,5,144,0,0,528,529,5,34,0,0,529,
    538,3,64,32,0,530,535,5,19,0,0,531,532,7,1,0,0,532,534,5,144,0,0,533,
    531,1,0,0,0,534,537,1,0,0,0,535,533,1,0,0,0,535,536,1,0,0,0,536,539,1,
    0,0,0,537,535,1,0,0,0,538,530,1,0,0,0,538,539,1,0,0,0,539,73,1,0,0,0,
    540,541,5,112,0,0,541,545,3,64,32,0,542,544,3,76,38,0,543,542,1,0,0,0,
    544,547,1,0,0,0,545,543,1,0,0,0,545,546,1,0,0,0,546,553,1,0,0,0,547,545,
    1,0,0,0,548,550,7,13,0,0,549,551,5,144,0,0,550,549,1,0,0,0,550,551,1,
    0,0,0,551,553,1,0,0,0,552,540,1,0,0,0,552,548,1,0,0,0,553,75,1,0,0,0,
    554,555,7,14,0,0,555,560,3,64,32,0,556,557,5,20,0,0,557,560,5,144,0,0,
    558,560,5,138,0,0,559,554,1,0,0,0,559,556,1,0,0,0,559,558,1,0,0,0,560,
    77,1,0,0,0,561,562,5,115,0,0,562,569,3,64,32,0,563,564,5,131,0,0,564,
    568,3,64,32,0,565,566,5,20,0,0,566,568,5,144,0,0,567,563,1,0,0,0,567,
    565,1,0,0,0,568,571,1,0,0,0,569,567,1,0,0,0,569,570,1,0,0,0,570,578,1,
    0,0,0,571,569,1,0,0,0,572,575,5,116,0,0,573,574,5,20,0,0,574,576,5,144,
    0,0,575,573,1,0,0,0,575,576,1,0,0,0,576,578,1,0,0,0,577,561,1,0,0,0,577,
    572,1,0,0,0,578,79,1,0,0,0,579,583,5,117,0,0,580,581,5,118,0,0,581,583,
    5,119,0,0,582,579,1,0,0,0,582,580,1,0,0,0,583,588,1,0,0,0,584,585,7,1,
    0,0,585,587,5,144,0,0,586,584,1,0,0,0,587,590,1,0,0,0,588,586,1,0,0,0,
    588,589,1,0,0,0,589,81,1,0,0,0,590,588,1,0,0,0,591,600,5,120,0,0,592,
    596,5,3,0,0,593,595,3,84,42,0,594,593,1,0,0,0,595,598,1,0,0,0,596,594,
    1,0,0,0,596,597,1,0,0,0,597,601,1,0,0,0,598,596,1,0,0,0,599,601,5,6,0,
    0,600,592,1,0,0,0,600,599,1,0,0,0,601,83,1,0,0,0,602,603,7,15,0,0,603,
    610,5,144,0,0,604,605,5,139,0,0,605,610,5,144,0,0,606,610,5,140,0,0,607,
    608,7,16,0,0,608,610,3,64,32,0,609,602,1,0,0,0,609,604,1,0,0,0,609,606,
    1,0,0,0,609,607,1,0,0,0,610,85,1,0,0,0,611,618,5,121,0,0,612,613,7,15,
    0,0,613,617,5,144,0,0,614,615,7,17,0,0,615,617,3,64,32,0,616,612,1,0,
    0,0,616,614,1,0,0,0,617,620,1,0,0,0,618,616,1,0,0,0,618,619,1,0,0,0,619,
    87,1,0,0,0,620,618,1,0,0,0,65,122,129,136,145,147,152,163,174,177,185,
    193,201,208,218,228,233,236,241,251,258,277,287,290,296,303,328,334,341,
    343,353,365,372,374,382,384,397,402,404,413,420,424,433,447,459,486,503,
    512,523,535,538,545,550,552,559,567,569,575,577,582,588,596,600,609,616,
    618
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
    setState(88);
    match(TracerCommandsParser::T__0);
    setState(89);
    command();
    setState(90);
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
    setState(122);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 0, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(92);
      server();
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(93);
      attach();
      break;
    }

    case 3: {
      enterOuterAlt(_localctx, 3);
      setState(94);
      processes();
      break;
    }

    case 4: {
      enterOuterAlt(_localctx, 4);
      setState(95);
      sessions();
      break;
    }

    case 5: {
      enterOuterAlt(_localctx, 5);
      setState(96);
      use();
      break;
    }

    case 6: {
      enterOuterAlt(_localctx, 6);
      setState(97);
      status();
      break;
    }

    case 7: {
      enterOuterAlt(_localctx, 7);
      setState(98);
      detach();
      break;
    }

    case 8: {
      enterOuterAlt(_localctx, 8);
      setState(99);
      modules();
      break;
    }

    case 9: {
      enterOuterAlt(_localctx, 9);
      setState(100);
      threads();
      break;
    }

    case 10: {
      enterOuterAlt(_localctx, 10);
      setState(101);
      symbols();
      break;
    }

    case 11: {
      enterOuterAlt(_localctx, 11);
      setState(102);
      probe();
      break;
    }

    case 12: {
      enterOuterAlt(_localctx, 12);
      setState(103);
      interceptor();
      break;
    }

    case 13: {
      enterOuterAlt(_localctx, 13);
      setState(104);
      events();
      break;
    }

    case 14: {
      enterOuterAlt(_localctx, 14);
      setState(105);
      collect();
      break;
    }

    case 15: {
      enterOuterAlt(_localctx, 15);
      setState(106);
      record();
      break;
    }

    case 16: {
      enterOuterAlt(_localctx, 16);
      setState(107);
      cpp();
      break;
    }

    case 17: {
      enterOuterAlt(_localctx, 17);
      setState(108);
      shell();
      break;
    }

    case 18: {
      enterOuterAlt(_localctx, 18);
      setState(109);
      complete();
      break;
    }

    case 19: {
      enterOuterAlt(_localctx, 19);
      setState(110);
      type();
      break;
    }

    case 20: {
      enterOuterAlt(_localctx, 20);
      setState(111);
      help();
      break;
    }

    case 21: {
      enterOuterAlt(_localctx, 21);
      setState(112);
      quit();
      break;
    }

    case 22: {
      enterOuterAlt(_localctx, 22);
      setState(113);
      bulkTrace();
      break;
    }

    case 23: {
      enterOuterAlt(_localctx, 23);
      setState(114);
      memory();
      break;
    }

    case 24: {
      enterOuterAlt(_localctx, 24);
      setState(115);
      probeEntry();
      break;
    }

    case 25: {
      enterOuterAlt(_localctx, 25);
      setState(116);
      probeReload();
      break;
    }

    case 26: {
      enterOuterAlt(_localctx, 26);
      setState(117);
      targetLaunch();
      break;
    }

    case 27: {
      enterOuterAlt(_localctx, 27);
      setState(118);
      targetCall();
      break;
    }

    case 28: {
      enterOuterAlt(_localctx, 28);
      setState(119);
      localPerf();
      break;
    }

    case 29: {
      enterOuterAlt(_localctx, 29);
      setState(120);
      instructions();
      break;
    }

    case 30: {
      enterOuterAlt(_localctx, 30);
      setState(121);
      watchpoint();
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
    setState(124);
    match(TracerCommandsParser::T__1);
    setState(136);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__2: {
        setState(125);
        match(TracerCommandsParser::T__2);
        setState(129);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__6

        || _la == TracerCommandsParser::T__7) {
          setState(126);
          serverOption();
          setState(131);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__3: {
        setState(132);
        match(TracerCommandsParser::T__3);
        setState(133);
        value();
        break;
      }

      case TracerCommandsParser::T__4: {
        setState(134);
        match(TracerCommandsParser::T__4);
        break;
      }

      case TracerCommandsParser::T__5: {
        setState(135);
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
    enterOuterAlt(_localctx, 1);
    setState(138);
    _la = _input->LA(1);
    if (!(_la == TracerCommandsParser::T__6

    || _la == TracerCommandsParser::T__7)) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(139);
    value();

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
    setState(141);
    match(TracerCommandsParser::T__8);
    setState(147);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::UINT) {
      setState(142);
      match(TracerCommandsParser::UINT);
      setState(145);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == TracerCommandsParser::T__9) {
        setState(143);
        match(TracerCommandsParser::T__9);
        setState(144);
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
    setState(149);
    match(TracerCommandsParser::T__10);
    setState(152);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::T__11) {
      setState(150);
      match(TracerCommandsParser::T__11);
      setState(151);
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
    setState(154);
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
    setState(156);
    match(TracerCommandsParser::T__13);
    setState(157);
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
    setState(159);
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
    setState(161);
    match(TracerCommandsParser::T__14);
    setState(163);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & -2472475938789397892) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 64)) & 288230247293750391) != 0) || ((((_la - 144) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 144)) & 35) != 0)) {
      setState(162);
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
    setState(165);
    match(TracerCommandsParser::T__15);
    setState(177);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__16: {
        setState(166);
        match(TracerCommandsParser::T__16);
        break;
      }

      case TracerCommandsParser::T__17: {
        setState(167);
        match(TracerCommandsParser::T__17);
        setState(168);
        value();
        break;
      }

      case TracerCommandsParser::T__18: {
        setState(169);
        match(TracerCommandsParser::T__18);
        setState(174);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__19

        || _la == TracerCommandsParser::T__20) {
          setState(170);
          _la = _input->LA(1);
          if (!(_la == TracerCommandsParser::T__19

          || _la == TracerCommandsParser::T__20)) {
          _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(171);
          match(TracerCommandsParser::UINT);
          setState(176);
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
    setState(179);
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
    setState(181);
    match(TracerCommandsParser::T__22);
    setState(185);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 1056968704) != 0)) {
      setState(182);
      symbolOption();
      setState(187);
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
    setState(193);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__11:
      case TracerCommandsParser::T__23:
      case TracerCommandsParser::T__24:
      case TracerCommandsParser::T__25:
      case TracerCommandsParser::T__26: {
        enterOuterAlt(_localctx, 1);
        setState(188);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 251662336) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(189);
        value();
        break;
      }

      case TracerCommandsParser::T__27: {
        enterOuterAlt(_localctx, 2);
        setState(190);
        match(TracerCommandsParser::T__27);
        setState(191);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__28: {
        enterOuterAlt(_localctx, 3);
        setState(192);
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
    setState(236);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__29: {
        enterOuterAlt(_localctx, 1);
        setState(195);
        match(TracerCommandsParser::T__29);
        setState(233);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__30: {
            setState(196);
            match(TracerCommandsParser::T__30);
            setState(197);
            value();
            setState(199);
            _errHandler->sync(this);
            _la = _input->LA(1);
            do {
              setState(198);
              prepareOption();
              setState(201);
              _errHandler->sync(this);
              _la = _input->LA(1);
            } while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 4123185381376) != 0) || _la == TracerCommandsParser::METADATA_ONLY);
            break;
          }

          case TracerCommandsParser::T__31: {
            setState(203);
            match(TracerCommandsParser::T__31);
            setState(204);
            value();
            setState(208);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 31610976075776) != 0)) {
              setState(205);
              resolveOption();
              setState(210);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__32: {
            setState(211);
            match(TracerCommandsParser::T__32);
            setState(212);
            value();
            setState(213);
            match(TracerCommandsParser::T__33);
            setState(214);
            value();
            setState(218);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 4500026231357440) != 0)) {
              setState(215);
              deployOption();
              setState(220);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__34: {
            setState(221);
            match(TracerCommandsParser::T__34);
            setState(222);
            value();
            setState(223);
            match(TracerCommandsParser::T__33);
            setState(224);
            value();
            setState(228);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 559376557408256) != 0)) {
              setState(225);
              validateOption();
              setState(230);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__35: {
            setState(231);
            match(TracerCommandsParser::T__35);
            setState(232);
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
        setState(235);
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
    setState(241);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__23:
      case TracerCommandsParser::T__37:
      case TracerCommandsParser::T__38:
      case TracerCommandsParser::T__39:
      case TracerCommandsParser::T__40: {
        enterOuterAlt(_localctx, 1);
        setState(238);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 4123185381376) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(239);
        value();
        break;
      }

      case TracerCommandsParser::METADATA_ONLY: {
        enterOuterAlt(_localctx, 2);
        setState(240);
        match(TracerCommandsParser::METADATA_ONLY);
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
    enterOuterAlt(_localctx, 1);
    setState(243);
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 31610976075776) != 0))) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(244);
    value();

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
    setState(251);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__23:
      case TracerCommandsParser::T__37:
      case TracerCommandsParser::T__38:
      case TracerCommandsParser::T__41:
      case TracerCommandsParser::T__42:
      case TracerCommandsParser::T__43: {
        enterOuterAlt(_localctx, 1);
        setState(246);
        resolveOption();
        break;
      }

      case TracerCommandsParser::T__44: {
        enterOuterAlt(_localctx, 2);
        setState(247);
        match(TracerCommandsParser::T__44);
        break;
      }

      case TracerCommandsParser::T__45: {
        enterOuterAlt(_localctx, 3);
        setState(248);
        match(TracerCommandsParser::T__45);
        break;
      }

      case TracerCommandsParser::T__46: {
        enterOuterAlt(_localctx, 4);
        setState(249);
        match(TracerCommandsParser::T__46);
        break;
      }

      case TracerCommandsParser::T__47: {
        enterOuterAlt(_localctx, 5);
        setState(250);
        match(TracerCommandsParser::T__47);
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
      case TracerCommandsParser::T__23:
      case TracerCommandsParser::T__37:
      case TracerCommandsParser::T__38:
      case TracerCommandsParser::T__41:
      case TracerCommandsParser::T__42:
      case TracerCommandsParser::T__43:
      case TracerCommandsParser::T__44:
      case TracerCommandsParser::T__45:
      case TracerCommandsParser::T__46:
      case TracerCommandsParser::T__47: {
        enterOuterAlt(_localctx, 1);
        setState(253);
        validateOption();
        break;
      }

      case TracerCommandsParser::T__48: {
        enterOuterAlt(_localctx, 2);
        setState(254);
        match(TracerCommandsParser::T__48);
        setState(255);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__49: {
        enterOuterAlt(_localctx, 3);
        setState(256);
        match(TracerCommandsParser::T__49);
        break;
      }

      case TracerCommandsParser::T__50: {
        enterOuterAlt(_localctx, 4);
        setState(257);
        match(TracerCommandsParser::T__50);
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
    setState(260);
    match(TracerCommandsParser::T__51);
    setState(290);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__52: {
        setState(261);
        match(TracerCommandsParser::T__52);
        setState(262);
        value();
        setState(263);
        match(TracerCommandsParser::T__53);
        setState(264);
        value();
        break;
      }

      case TracerCommandsParser::T__54: {
        setState(266);
        match(TracerCommandsParser::T__54);
        setState(267);
        value();
        break;
      }

      case TracerCommandsParser::T__55: {
        setState(268);
        match(TracerCommandsParser::T__55);
        setState(269);
        value();
        break;
      }

      case TracerCommandsParser::T__34: {
        setState(270);
        match(TracerCommandsParser::T__34);
        setState(271);
        value();
        setState(272);
        match(TracerCommandsParser::T__56);
        setState(273);
        value();
        setState(277);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 559376557408256) != 0)) {
          setState(274);
          validateOption();
          setState(279);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__57: {
        setState(280);
        match(TracerCommandsParser::T__57);
        setState(281);
        value();
        setState(282);
        match(TracerCommandsParser::T__56);
        setState(283);
        value();
        setState(287);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 4500026231357440) != 0)) {
          setState(284);
          deployOption();
          setState(289);
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
    setState(292);
    match(TracerCommandsParser::T__58);
    setState(328);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::EOF:
      case TracerCommandsParser::T__19:
      case TracerCommandsParser::T__20:
      case TracerCommandsParser::T__53:
      case TracerCommandsParser::T__70:
      case TracerCommandsParser::T__71:
      case TracerCommandsParser::T__72:
      case TracerCommandsParser::T__75:
      case TracerCommandsParser::T__76:
      case TracerCommandsParser::T__77: {
        setState(296);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (((((_la - 20) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 20)) & 520165774141161475) != 0)) {
          setState(293);
          streamOption();
          setState(298);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__59: {
        setState(299);
        match(TracerCommandsParser::T__59);
        setState(300);
        value();
        setState(303);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__27) {
          setState(301);
          match(TracerCommandsParser::T__27);
          setState(302);
          match(TracerCommandsParser::UINT);
        }
        break;
      }

      case TracerCommandsParser::T__55: {
        setState(305);
        match(TracerCommandsParser::T__55);
        setState(306);
        value();
        setState(307);
        match(TracerCommandsParser::T__60);
        setState(308);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__61: {
        setState(310);
        match(TracerCommandsParser::T__61);
        setState(311);
        value();
        break;
      }

      case TracerCommandsParser::T__62: {
        setState(312);
        match(TracerCommandsParser::T__62);
        setState(313);
        value();
        break;
      }

      case TracerCommandsParser::T__63: {
        setState(314);
        match(TracerCommandsParser::T__63);
        setState(315);
        value();
        break;
      }

      case TracerCommandsParser::T__64: {
        setState(316);
        match(TracerCommandsParser::T__64);
        setState(317);
        value();
        setState(318);
        match(TracerCommandsParser::T__53);
        setState(319);
        value();
        break;
      }

      case TracerCommandsParser::T__65: {
        setState(321);
        match(TracerCommandsParser::T__65);
        setState(322);
        value();
        setState(323);
        match(TracerCommandsParser::T__66);
        setState(324);
        match(TracerCommandsParser::T__67);
        setState(325);
        match(TracerCommandsParser::T__40);
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
    setState(330);
    match(TracerCommandsParser::T__68);
    setState(334);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (((((_la - 20) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 20)) & 520165774141161475) != 0)) {
      setState(331);
      streamOption();
      setState(336);
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
    setState(337);
    match(TracerCommandsParser::T__69);
    setState(338);
    value();
    setState(343);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (((((_la - 20) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 20)) & 520165774141161475) != 0) || _la == TracerCommandsParser::SQLITE) {
      setState(341);
      _errHandler->sync(this);
      switch (_input->LA(1)) {
        case TracerCommandsParser::T__19:
        case TracerCommandsParser::T__20:
        case TracerCommandsParser::T__53:
        case TracerCommandsParser::T__70:
        case TracerCommandsParser::T__71:
        case TracerCommandsParser::T__72:
        case TracerCommandsParser::T__75:
        case TracerCommandsParser::T__76:
        case TracerCommandsParser::T__77: {
          setState(339);
          streamOption();
          break;
        }

        case TracerCommandsParser::SQLITE: {
          setState(340);
          match(TracerCommandsParser::SQLITE);
          break;
        }

      default:
        throw NoViableAltException(this);
      }
      setState(345);
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
    setState(353);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__19:
      case TracerCommandsParser::T__20:
      case TracerCommandsParser::T__70:
      case TracerCommandsParser::T__71: {
        enterOuterAlt(_localctx, 1);
        setState(346);
        _la = _input->LA(1);
        if (!(((((_la - 20) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 20)) & 6755399441055747) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(347);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__72: {
        enterOuterAlt(_localctx, 2);
        setState(348);
        match(TracerCommandsParser::T__72);
        setState(349);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__73

        || _la == TracerCommandsParser::T__74)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        break;
      }

      case TracerCommandsParser::T__53:
      case TracerCommandsParser::T__75:
      case TracerCommandsParser::T__76: {
        enterOuterAlt(_localctx, 3);
        setState(350);
        _la = _input->LA(1);
        if (!(((((_la - 54) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 54)) & 12582913) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(351);
        value();
        break;
      }

      case TracerCommandsParser::T__77: {
        enterOuterAlt(_localctx, 4);
        setState(352);
        match(TracerCommandsParser::T__77);
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
    setState(355);
    match(TracerCommandsParser::T__78);
    setState(384);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__52: {
        setState(356);
        match(TracerCommandsParser::T__52);
        setState(357);
        value();
        break;
      }

      case TracerCommandsParser::T__79: {
        setState(358);
        match(TracerCommandsParser::T__79);
        break;
      }

      case TracerCommandsParser::T__80:
      case TracerCommandsParser::T__81: {
        setState(359);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__80

        || _la == TracerCommandsParser::T__81)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(360);
        value();
        setState(365);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__40

        || _la == TracerCommandsParser::T__82) {
          setState(361);
          _la = _input->LA(1);
          if (!(_la == TracerCommandsParser::T__40

          || _la == TracerCommandsParser::T__82)) {
          _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(362);
          value();
          setState(367);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__83: {
        setState(368);
        match(TracerCommandsParser::T__83);
        setState(382);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__84: {
            setState(369);
            match(TracerCommandsParser::T__84);
            setState(372);
            _errHandler->sync(this);
            _la = _input->LA(1);
            do {
              setState(372);
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
                case TracerCommandsParser::T__51:
                case TracerCommandsParser::T__52:
                case TracerCommandsParser::T__54:
                case TracerCommandsParser::T__55:
                case TracerCommandsParser::T__57:
                case TracerCommandsParser::T__58:
                case TracerCommandsParser::T__59:
                case TracerCommandsParser::T__61:
                case TracerCommandsParser::T__62:
                case TracerCommandsParser::T__63:
                case TracerCommandsParser::T__64:
                case TracerCommandsParser::T__65:
                case TracerCommandsParser::T__67:
                case TracerCommandsParser::T__68:
                case TracerCommandsParser::T__69:
                case TracerCommandsParser::T__73:
                case TracerCommandsParser::T__74:
                case TracerCommandsParser::T__78:
                case TracerCommandsParser::T__79:
                case TracerCommandsParser::T__80:
                case TracerCommandsParser::T__81:
                case TracerCommandsParser::T__83:
                case TracerCommandsParser::T__84:
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
                case TracerCommandsParser::T__100:
                case TracerCommandsParser::T__101:
                case TracerCommandsParser::T__102:
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
                case TracerCommandsParser::UINT:
                case TracerCommandsParser::STRING:
                case TracerCommandsParser::WORD: {
                  setState(370);
                  value();
                  break;
                }

                case TracerCommandsParser::SHELL: {
                  setState(371);
                  match(TracerCommandsParser::SHELL);
                  break;
                }

              default:
                throw NoViableAltException(this);
              }
              setState(374);
              _errHandler->sync(this);
              _la = _input->LA(1);
            } while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & -2472475938789397892) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 64)) & 288230247293750391) != 0) || ((((_la - 144) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 144)) & 39) != 0));
            break;
          }

          case TracerCommandsParser::T__63: {
            setState(376);
            match(TracerCommandsParser::T__63);
            break;
          }

          case TracerCommandsParser::T__85: {
            setState(377);
            match(TracerCommandsParser::T__85);
            setState(378);
            value();
            setState(379);
            match(TracerCommandsParser::T__86);
            setState(380);
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
    setState(386);
    match(TracerCommandsParser::T__87);
    setState(387);
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
    setState(389);
    match(TracerCommandsParser::T__88);
    setState(390);
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
    setState(392);
    match(TracerCommandsParser::T__89);
    setState(393);
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
    setState(395);
    match(TracerCommandsParser::T__90);
    setState(404);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & -2472475938789397890) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 64)) & 288230247293750391) != 0) || ((((_la - 144) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 144)) & 35) != 0)) {
      setState(397);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == TracerCommandsParser::T__0) {
        setState(396);
        match(TracerCommandsParser::T__0);
      }
      setState(400);
      _errHandler->sync(this);
      _la = _input->LA(1);
      do {
        setState(399);
        value();
        setState(402);
        _errHandler->sync(this);
        _la = _input->LA(1);
      } while ((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & -2472475938789397892) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 64)) & 288230247293750391) != 0) || ((((_la - 144) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 144)) & 35) != 0));
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
    setState(406);
    _la = _input->LA(1);
    if (!(_la == TracerCommandsParser::T__91

    || _la == TracerCommandsParser::T__92)) {
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
    setState(408);
    match(TracerCommandsParser::T__93);
    setState(424);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__94: {
        setState(409);
        match(TracerCommandsParser::T__94);
        setState(411);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(410);
          bulkTraceOption();
          setState(413);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while (_la == TracerCommandsParser::T__23

        || _la == TracerCommandsParser::T__37 || ((((_la - 97) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 97)) & 15) != 0));
        break;
      }

      case TracerCommandsParser::T__95: {
        setState(415);
        match(TracerCommandsParser::T__95);
        setState(416);
        match(TracerCommandsParser::T__96);
        setState(417);
        value();
        setState(420);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__33) {
          setState(418);
          match(TracerCommandsParser::T__33);
          setState(419);
          value();
        }
        break;
      }

      case TracerCommandsParser::T__35: {
        setState(422);
        match(TracerCommandsParser::T__35);
        setState(423);
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
    setState(426);
    _la = _input->LA(1);
    if (!(_la == TracerCommandsParser::T__23

    || _la == TracerCommandsParser::T__37 || ((((_la - 97) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 97)) & 15) != 0))) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
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
    setState(433);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::STRING: {
        enterOuterAlt(_localctx, 1);
        setState(429);
        match(TracerCommandsParser::STRING);
        break;
      }

      case TracerCommandsParser::WORD: {
        enterOuterAlt(_localctx, 2);
        setState(430);
        match(TracerCommandsParser::WORD);
        break;
      }

      case TracerCommandsParser::UINT: {
        enterOuterAlt(_localctx, 3);
        setState(431);
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
      case TracerCommandsParser::T__51:
      case TracerCommandsParser::T__52:
      case TracerCommandsParser::T__54:
      case TracerCommandsParser::T__55:
      case TracerCommandsParser::T__57:
      case TracerCommandsParser::T__58:
      case TracerCommandsParser::T__59:
      case TracerCommandsParser::T__61:
      case TracerCommandsParser::T__62:
      case TracerCommandsParser::T__63:
      case TracerCommandsParser::T__64:
      case TracerCommandsParser::T__65:
      case TracerCommandsParser::T__67:
      case TracerCommandsParser::T__68:
      case TracerCommandsParser::T__69:
      case TracerCommandsParser::T__73:
      case TracerCommandsParser::T__74:
      case TracerCommandsParser::T__78:
      case TracerCommandsParser::T__79:
      case TracerCommandsParser::T__80:
      case TracerCommandsParser::T__81:
      case TracerCommandsParser::T__83:
      case TracerCommandsParser::T__84:
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
      case TracerCommandsParser::T__100:
      case TracerCommandsParser::T__101:
      case TracerCommandsParser::T__102:
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
      case TracerCommandsParser::T__120: {
        enterOuterAlt(_localctx, 4);
        setState(432);
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
    setState(435);
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & -2472475938789397892) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 64)) & 288230247293750391) != 0))) {
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
    setState(437);
    match(TracerCommandsParser::T__100);
    setState(503);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__101: {
        setState(438);
        match(TracerCommandsParser::T__101);
        break;
      }

      case TracerCommandsParser::T__102: {
        setState(439);
        match(TracerCommandsParser::T__102);
        setState(440);
        value();
        setState(441);
        match(TracerCommandsParser::T__26);
        setState(442);
        value();
        setState(447);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__20: {
            setState(443);
            match(TracerCommandsParser::T__20);
            setState(444);
            match(TracerCommandsParser::UINT);
            break;
          }

          case TracerCommandsParser::T__82: {
            setState(445);
            match(TracerCommandsParser::T__82);
            setState(446);
            value();
            break;
          }

        default:
          throw NoViableAltException(this);
        }
        break;
      }

      case TracerCommandsParser::T__103: {
        setState(449);
        match(TracerCommandsParser::T__103);
        setState(450);
        value();
        setState(451);
        match(TracerCommandsParser::T__26);
        setState(452);
        value();
        setState(453);
        match(TracerCommandsParser::T__20);
        setState(454);
        match(TracerCommandsParser::UINT);
        setState(455);
        match(TracerCommandsParser::T__121);
        setState(456);
        value();
        setState(459);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__27) {
          setState(457);
          match(TracerCommandsParser::T__27);
          setState(458);
          match(TracerCommandsParser::UINT);
        }
        break;
      }

      case TracerCommandsParser::T__104: {
        setState(461);
        match(TracerCommandsParser::T__104);
        setState(462);
        match(TracerCommandsParser::T__23);
        setState(463);
        value();
        setState(464);
        match(TracerCommandsParser::T__122);
        setState(465);
        value();
        setState(466);
        match(TracerCommandsParser::T__26);
        setState(467);
        value();
        setState(468);
        match(TracerCommandsParser::T__123);
        setState(469);
        value();
        setState(470);
        match(TracerCommandsParser::T__124);
        setState(471);
        value();
        break;
      }

      case TracerCommandsParser::T__105: {
        setState(473);
        match(TracerCommandsParser::T__105);
        setState(474);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__107: {
        setState(475);
        match(TracerCommandsParser::T__107);
        setState(476);
        match(TracerCommandsParser::T__125);
        setState(477);
        match(TracerCommandsParser::UINT);
        setState(478);
        match(TracerCommandsParser::T__25);
        setState(479);
        value();
        setState(480);
        match(TracerCommandsParser::T__126);
        setState(481);
        match(TracerCommandsParser::UINT);
        setState(482);
        match(TracerCommandsParser::T__26);
        setState(483);
        value();
        setState(486);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__19) {
          setState(484);
          match(TracerCommandsParser::T__19);
          setState(485);
          match(TracerCommandsParser::UINT);
        }
        break;
      }

      case TracerCommandsParser::T__108: {
        setState(488);
        match(TracerCommandsParser::T__108);
        break;
      }

      case TracerCommandsParser::T__106: {
        setState(489);
        match(TracerCommandsParser::T__106);
        setState(490);
        match(TracerCommandsParser::T__23);
        setState(491);
        value();
        setState(492);
        match(TracerCommandsParser::T__127);
        setState(493);
        value();
        setState(494);
        match(TracerCommandsParser::T__128);
        setState(495);
        value();
        setState(496);
        match(TracerCommandsParser::T__25);
        setState(497);
        value();
        setState(498);
        match(TracerCommandsParser::T__26);
        setState(499);
        value();
        setState(500);
        match(TracerCommandsParser::T__129);
        setState(501);
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
    setState(505);
    match(TracerCommandsParser::T__29);
    setState(506);
    match(TracerCommandsParser::T__109);
    setState(523);
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
      case TracerCommandsParser::T__51:
      case TracerCommandsParser::T__52:
      case TracerCommandsParser::T__54:
      case TracerCommandsParser::T__55:
      case TracerCommandsParser::T__57:
      case TracerCommandsParser::T__58:
      case TracerCommandsParser::T__59:
      case TracerCommandsParser::T__61:
      case TracerCommandsParser::T__62:
      case TracerCommandsParser::T__63:
      case TracerCommandsParser::T__64:
      case TracerCommandsParser::T__65:
      case TracerCommandsParser::T__67:
      case TracerCommandsParser::T__68:
      case TracerCommandsParser::T__69:
      case TracerCommandsParser::T__73:
      case TracerCommandsParser::T__74:
      case TracerCommandsParser::T__78:
      case TracerCommandsParser::T__79:
      case TracerCommandsParser::T__80:
      case TracerCommandsParser::T__81:
      case TracerCommandsParser::T__83:
      case TracerCommandsParser::T__84:
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
      case TracerCommandsParser::T__100:
      case TracerCommandsParser::T__101:
      case TracerCommandsParser::T__102:
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
      case TracerCommandsParser::UINT:
      case TracerCommandsParser::STRING:
      case TracerCommandsParser::WORD: {
        setState(507);
        value();
        setState(508);
        match(TracerCommandsParser::T__26);
        setState(509);
        value();
        setState(512);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__23) {
          setState(510);
          match(TracerCommandsParser::T__23);
          setState(511);
          value();
        }
        break;
      }

      case TracerCommandsParser::T__23: {
        setState(514);
        match(TracerCommandsParser::T__23);
        setState(515);
        value();
        setState(516);
        match(TracerCommandsParser::T__122);
        setState(517);
        value();
        setState(518);
        match(TracerCommandsParser::T__129);
        setState(519);
        value();
        setState(520);
        match(TracerCommandsParser::T__26);
        setState(521);
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
    setState(525);
    match(TracerCommandsParser::T__29);
    setState(526);
    match(TracerCommandsParser::T__110);
    setState(527);
    match(TracerCommandsParser::UINT);
    setState(528);
    match(TracerCommandsParser::T__33);
    setState(529);
    value();
    setState(538);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::T__18) {
      setState(530);
      match(TracerCommandsParser::T__18);
      setState(535);
      _errHandler->sync(this);
      _la = _input->LA(1);
      while (_la == TracerCommandsParser::T__19

      || _la == TracerCommandsParser::T__20) {
        setState(531);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__19

        || _la == TracerCommandsParser::T__20)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(532);
        match(TracerCommandsParser::UINT);
        setState(537);
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
    setState(552);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__111: {
        enterOuterAlt(_localctx, 1);
        setState(540);
        match(TracerCommandsParser::T__111);
        setState(541);
        value();
        setState(545);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__19 || ((((_la - 131) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 131)) & 255) != 0)) {
          setState(542);
          launchOption();
          setState(547);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__112:
      case TracerCommandsParser::T__113: {
        enterOuterAlt(_localctx, 2);
        setState(548);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__112

        || _la == TracerCommandsParser::T__113)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(550);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::UINT) {
          setState(549);
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
    setState(559);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__130:
      case TracerCommandsParser::T__131:
      case TracerCommandsParser::T__132:
      case TracerCommandsParser::T__133:
      case TracerCommandsParser::T__134:
      case TracerCommandsParser::T__135:
      case TracerCommandsParser::T__136: {
        enterOuterAlt(_localctx, 1);
        setState(554);
        _la = _input->LA(1);
        if (!(((((_la - 131) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 131)) & 127) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(555);
        value();
        break;
      }

      case TracerCommandsParser::T__19: {
        enterOuterAlt(_localctx, 2);
        setState(556);
        match(TracerCommandsParser::T__19);
        setState(557);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__137: {
        enterOuterAlt(_localctx, 3);
        setState(558);
        match(TracerCommandsParser::T__137);
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
  enterRule(_localctx, 78, TracerCommandsParser::RuleTargetCall);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(577);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__114: {
        enterOuterAlt(_localctx, 1);
        setState(561);
        match(TracerCommandsParser::T__114);
        setState(562);
        value();
        setState(569);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__19 || _la == TracerCommandsParser::T__130) {
          setState(567);
          _errHandler->sync(this);
          switch (_input->LA(1)) {
            case TracerCommandsParser::T__130: {
              setState(563);
              match(TracerCommandsParser::T__130);
              setState(564);
              value();
              break;
            }

            case TracerCommandsParser::T__19: {
              setState(565);
              match(TracerCommandsParser::T__19);
              setState(566);
              match(TracerCommandsParser::UINT);
              break;
            }

          default:
            throw NoViableAltException(this);
          }
          setState(571);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__115: {
        enterOuterAlt(_localctx, 2);
        setState(572);
        match(TracerCommandsParser::T__115);
        setState(575);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__19) {
          setState(573);
          match(TracerCommandsParser::T__19);
          setState(574);
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
  enterRule(_localctx, 80, TracerCommandsParser::RuleLocalPerf);
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
    setState(582);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__116: {
        setState(579);
        match(TracerCommandsParser::T__116);
        break;
      }

      case TracerCommandsParser::T__117: {
        setState(580);
        match(TracerCommandsParser::T__117);
        setState(581);
        match(TracerCommandsParser::T__118);
        break;
      }

    default:
      throw NoViableAltException(this);
    }
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
  enterRule(_localctx, 82, TracerCommandsParser::RuleInstructions);
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
    setState(591);
    match(TracerCommandsParser::T__119);
    setState(600);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__2: {
        setState(592);
        match(TracerCommandsParser::T__2);
        setState(596);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 204472320) != 0) || ((((_la - 126) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 126)) & 122883) != 0)) {
          setState(593);
          instructionOption();
          setState(598);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__5: {
        setState(599);
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
  enterRule(_localctx, 84, TracerCommandsParser::RuleInstructionOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(609);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__19:
      case TracerCommandsParser::T__20:
      case TracerCommandsParser::T__125:
      case TracerCommandsParser::T__126: {
        enterOuterAlt(_localctx, 1);
        setState(602);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__19

        || _la == TracerCommandsParser::T__20 || _la == TracerCommandsParser::T__125

        || _la == TracerCommandsParser::T__126)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(603);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__138: {
        enterOuterAlt(_localctx, 2);
        setState(604);
        match(TracerCommandsParser::T__138);
        setState(605);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__139: {
        enterOuterAlt(_localctx, 3);
        setState(606);
        match(TracerCommandsParser::T__139);
        break;
      }

      case TracerCommandsParser::T__25:
      case TracerCommandsParser::T__26:
      case TracerCommandsParser::T__140:
      case TracerCommandsParser::T__141: {
        enterOuterAlt(_localctx, 4);
        setState(607);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__25

        || _la == TracerCommandsParser::T__26 || _la == TracerCommandsParser::T__140

        || _la == TracerCommandsParser::T__141)) {
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
  enterRule(_localctx, 86, TracerCommandsParser::RuleWatchpoint);
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
    setState(611);
    match(TracerCommandsParser::T__120);
    setState(618);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 204472320) != 0) || ((((_la - 126) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 126)) & 131075) != 0)) {
      setState(616);
      _errHandler->sync(this);
      switch (_input->LA(1)) {
        case TracerCommandsParser::T__19:
        case TracerCommandsParser::T__20:
        case TracerCommandsParser::T__125:
        case TracerCommandsParser::T__126: {
          setState(612);
          _la = _input->LA(1);
          if (!(_la == TracerCommandsParser::T__19

          || _la == TracerCommandsParser::T__20 || _la == TracerCommandsParser::T__125

          || _la == TracerCommandsParser::T__126)) {
          _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(613);
          match(TracerCommandsParser::UINT);
          break;
        }

        case TracerCommandsParser::T__25:
        case TracerCommandsParser::T__26:
        case TracerCommandsParser::T__142: {
          setState(614);
          _la = _input->LA(1);
          if (!(_la == TracerCommandsParser::T__25

          || _la == TracerCommandsParser::T__26 || _la == TracerCommandsParser::T__142)) {
          _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(615);
          value();
          break;
        }

      default:
        throw NoViableAltException(this);
      }
      setState(620);
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

void TracerCommandsParser::initialize() {
#if ANTLR4_USE_THREAD_LOCAL_CACHE
  tracercommandsParserInitialize();
#else
  ::antlr4::internal::call_once(tracercommandsParserOnceFlag, tracercommandsParserInitialize);
#endif
}
