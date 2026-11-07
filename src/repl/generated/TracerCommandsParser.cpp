
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
      "launchOption", "targetCall", "localPerf"
    },
    std::vector<std::string>{
      "", "'%'", "'server'", "'start'", "'connect'", "'status'", "'stop'",
      "'--executable'", "'--agent'", "'attach'", "'--start-time'", "'processes'",
      "'--filter'", "'sessions'", "'use'", "'detach'", "'modules'", "'--details'",
      "'threads'", "'symbols'", "'--module'", "'--page'", "'--address'",
      "'--mapping'", "'--limit'", "'--addresses'", "'probe'", "'prepare'",
      "'resolve'", "'inject'", "'--callback'", "'validate'", "'remove'",
      "'probes'", "'--include'", "'--compdb'", "'--translation-unit'", "'--output'",
      "'--compile-option'", "'--prepared'", "'--prepared-mode'", "'--allow-arg-overrides'",
      "'--allow-result-override'", "'--stack-depth'", "'--print-arguments'",
      "'--print-result'", "'interceptor'", "'load'", "'--as'", "'edit'",
      "'show'", "'--target'", "'upload'", "'events'", "'list'", "'--index'",
      "'calls'", "'stats'", "'clear'", "'open'", "'export'", "'--format'",
      "'json'", "'collect'", "'record'", "'--count'", "'--timeout'", "'--after'",
      "'--probe'", "'--phase'", "'PRE'", "'POST'", "'--name'", "'--session'",
      "'--all'", "'cpp'", "'reset'", "'import'", "'generate'", "'--type'",
      "'options'", "'add'", "'compdb'", "'--file'", "'shell'", "'complete'",
      "'type'", "'help'", "'quit'", "'exit'", "'trace'", "'preview'", "'apply'",
      "'--manifest'", "'--exclude'", "'--exact'", "'--save'", "'memory'",
      "'maps'", "'read'", "'scan'", "'entry'", "'reload'", "'launch'", "'resume'",
      "'terminate'", "'call'", "'call-result'", "'syscalls'", "'sample'",
      "'cpu'", "'--pattern'", "'--elf-address'", "'--abi'", "'--arg'", "'--cwd'",
      "'--env'", "'--stdin'", "'--stdout'", "'--stderr'", "", "", "", "'--metadata-only'",
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
      "", "UINT", "STRING", "SHELL", "METADATA_ONLY", "SQLITE", "WORD",
      "WS"
    }
  );
  static const int32_t serializedATNSegment[] = {
    4,1,126,520,2,0,7,0,2,1,7,1,2,2,7,2,2,3,7,3,2,4,7,4,2,5,7,5,2,6,7,6,2,
    7,7,7,2,8,7,8,2,9,7,9,2,10,7,10,2,11,7,11,2,12,7,12,2,13,7,13,2,14,7,
    14,2,15,7,15,2,16,7,16,2,17,7,17,2,18,7,18,2,19,7,19,2,20,7,20,2,21,7,
    21,2,22,7,22,2,23,7,23,2,24,7,24,2,25,7,25,2,26,7,26,2,27,7,27,2,28,7,
    28,2,29,7,29,2,30,7,30,2,31,7,31,2,32,7,32,2,33,7,33,2,34,7,34,2,35,7,
    35,2,36,7,36,2,37,7,37,2,38,7,38,2,39,7,39,2,40,7,40,1,0,1,0,1,0,1,0,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,3,1,115,8,1,1,2,1,2,1,2,5,2,
    120,8,2,10,2,12,2,123,9,2,1,2,1,2,1,2,1,2,3,2,129,8,2,1,3,1,3,1,3,1,4,
    1,4,1,4,1,4,3,4,138,8,4,3,4,140,8,4,1,5,1,5,1,5,3,5,145,8,5,1,6,1,6,1,
    7,1,7,1,7,1,8,1,8,1,9,1,9,3,9,156,8,9,1,10,1,10,3,10,160,8,10,1,11,1,
    11,1,12,1,12,5,12,166,8,12,10,12,12,12,169,9,12,1,13,1,13,1,13,1,13,1,
    13,3,13,176,8,13,1,14,1,14,1,14,1,14,4,14,182,8,14,11,14,12,14,183,1,
    14,1,14,1,14,5,14,189,8,14,10,14,12,14,192,9,14,1,14,1,14,1,14,1,14,1,
    14,5,14,199,8,14,10,14,12,14,202,9,14,1,14,1,14,1,14,1,14,1,14,5,14,209,
    8,14,10,14,12,14,212,9,14,1,14,1,14,3,14,216,8,14,1,14,3,14,219,8,14,
    1,15,1,15,1,15,3,15,224,8,15,1,16,1,16,1,16,1,17,1,17,1,17,3,17,232,8,
    17,1,18,1,18,1,18,1,18,1,18,3,18,239,8,18,1,19,1,19,1,19,1,19,1,19,1,
    19,1,19,1,19,1,19,1,19,1,19,1,19,1,19,1,19,1,19,5,19,256,8,19,10,19,12,
    19,259,9,19,1,19,1,19,1,19,1,19,1,19,5,19,266,8,19,10,19,12,19,269,9,
    19,3,19,271,8,19,1,20,1,20,5,20,275,8,20,10,20,12,20,278,9,20,1,20,1,
    20,1,20,1,20,3,20,284,8,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,
    20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,
    20,3,20,309,8,20,1,21,1,21,5,21,313,8,21,10,21,12,21,316,9,21,1,22,1,
    22,1,22,1,22,5,22,322,8,22,10,22,12,22,325,9,22,1,23,1,23,1,23,1,23,1,
    23,1,23,1,23,3,23,334,8,23,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,5,
    24,344,8,24,10,24,12,24,347,9,24,1,24,1,24,1,24,1,24,4,24,353,8,24,11,
    24,12,24,354,1,24,1,24,1,24,1,24,1,24,1,24,3,24,363,8,24,3,24,365,8,24,
    1,25,1,25,1,25,1,26,1,26,1,26,1,27,1,27,1,27,1,28,1,28,3,28,378,8,28,
    1,28,4,28,381,8,28,11,28,12,28,382,3,28,385,8,28,1,29,1,29,1,30,1,30,
    1,30,4,30,392,8,30,11,30,12,30,393,1,30,1,30,1,30,1,30,1,30,3,30,401,
    8,30,1,30,1,30,3,30,405,8,30,1,31,1,31,1,31,1,32,1,32,1,32,1,32,3,32,
    414,8,32,1,33,1,33,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,
    3,34,428,8,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,3,34,
    440,8,34,3,34,442,8,34,1,35,1,35,1,35,1,35,1,35,1,35,1,35,3,35,451,8,
    35,1,35,1,35,1,35,1,35,1,35,1,35,1,35,1,35,1,35,3,35,462,8,35,1,36,1,
    36,1,36,1,36,1,36,1,36,1,37,1,37,1,37,5,37,473,8,37,10,37,12,37,476,9,
    37,1,37,1,37,3,37,480,8,37,3,37,482,8,37,1,38,1,38,1,38,1,38,3,38,488,
    8,38,1,39,1,39,1,39,1,39,1,39,1,39,5,39,496,8,39,10,39,12,39,499,9,39,
    1,39,1,39,1,39,3,39,504,8,39,3,39,506,8,39,1,40,1,40,1,40,3,40,511,8,
    40,1,40,1,40,5,40,515,8,40,10,40,12,40,518,9,40,1,40,0,0,41,0,2,4,6,8,
    10,12,14,16,18,20,22,24,26,28,30,32,34,36,38,40,42,44,46,48,50,52,54,
    56,58,60,62,64,66,68,70,72,74,76,78,80,0,15,1,0,7,8,2,0,12,12,20,23,2,
    0,20,20,34,37,3,0,20,20,34,35,38,40,1,0,65,68,1,0,70,71,2,0,48,48,72,
    73,1,0,77,78,2,0,37,37,79,79,1,0,88,89,3,0,20,20,34,34,93,96,17,0,2,6,
    9,9,11,11,13,16,18,19,26,29,31,33,46,47,49,50,52,54,56,60,62,64,70,71,
    75,78,80,82,84,92,97,110,1,0,104,105,1,0,114,119,1,0,65,66,588,0,82,1,
    0,0,0,2,114,1,0,0,0,4,116,1,0,0,0,6,130,1,0,0,0,8,133,1,0,0,0,10,141,
    1,0,0,0,12,146,1,0,0,0,14,148,1,0,0,0,16,151,1,0,0,0,18,153,1,0,0,0,20,
    157,1,0,0,0,22,161,1,0,0,0,24,163,1,0,0,0,26,175,1,0,0,0,28,218,1,0,0,
    0,30,223,1,0,0,0,32,225,1,0,0,0,34,231,1,0,0,0,36,238,1,0,0,0,38,240,
    1,0,0,0,40,272,1,0,0,0,42,310,1,0,0,0,44,317,1,0,0,0,46,333,1,0,0,0,48,
    335,1,0,0,0,50,366,1,0,0,0,52,369,1,0,0,0,54,372,1,0,0,0,56,375,1,0,0,
    0,58,386,1,0,0,0,60,388,1,0,0,0,62,406,1,0,0,0,64,413,1,0,0,0,66,415,
    1,0,0,0,68,417,1,0,0,0,70,443,1,0,0,0,72,463,1,0,0,0,74,481,1,0,0,0,76,
    487,1,0,0,0,78,505,1,0,0,0,80,510,1,0,0,0,82,83,5,1,0,0,83,84,3,2,1,0,
    84,85,5,0,0,1,85,1,1,0,0,0,86,115,3,4,2,0,87,115,3,8,4,0,88,115,3,10,
    5,0,89,115,3,12,6,0,90,115,3,14,7,0,91,115,3,16,8,0,92,115,3,18,9,0,93,
    115,3,20,10,0,94,115,3,22,11,0,95,115,3,24,12,0,96,115,3,28,14,0,97,115,
    3,38,19,0,98,115,3,40,20,0,99,115,3,42,21,0,100,115,3,44,22,0,101,115,
    3,48,24,0,102,115,3,50,25,0,103,115,3,52,26,0,104,115,3,54,27,0,105,115,
    3,56,28,0,106,115,3,58,29,0,107,115,3,60,30,0,108,115,3,68,34,0,109,115,
    3,70,35,0,110,115,3,72,36,0,111,115,3,74,37,0,112,115,3,78,39,0,113,115,
    3,80,40,0,114,86,1,0,0,0,114,87,1,0,0,0,114,88,1,0,0,0,114,89,1,0,0,0,
    114,90,1,0,0,0,114,91,1,0,0,0,114,92,1,0,0,0,114,93,1,0,0,0,114,94,1,
    0,0,0,114,95,1,0,0,0,114,96,1,0,0,0,114,97,1,0,0,0,114,98,1,0,0,0,114,
    99,1,0,0,0,114,100,1,0,0,0,114,101,1,0,0,0,114,102,1,0,0,0,114,103,1,
    0,0,0,114,104,1,0,0,0,114,105,1,0,0,0,114,106,1,0,0,0,114,107,1,0,0,0,
    114,108,1,0,0,0,114,109,1,0,0,0,114,110,1,0,0,0,114,111,1,0,0,0,114,112,
    1,0,0,0,114,113,1,0,0,0,115,3,1,0,0,0,116,128,5,2,0,0,117,121,5,3,0,0,
    118,120,3,6,3,0,119,118,1,0,0,0,120,123,1,0,0,0,121,119,1,0,0,0,121,122,
    1,0,0,0,122,129,1,0,0,0,123,121,1,0,0,0,124,125,5,4,0,0,125,129,3,64,
    32,0,126,129,5,5,0,0,127,129,5,6,0,0,128,117,1,0,0,0,128,124,1,0,0,0,
    128,126,1,0,0,0,128,127,1,0,0,0,129,5,1,0,0,0,130,131,7,0,0,0,131,132,
    3,64,32,0,132,7,1,0,0,0,133,139,5,9,0,0,134,137,5,120,0,0,135,136,5,10,
    0,0,136,138,5,120,0,0,137,135,1,0,0,0,137,138,1,0,0,0,138,140,1,0,0,0,
    139,134,1,0,0,0,139,140,1,0,0,0,140,9,1,0,0,0,141,144,5,11,0,0,142,143,
    5,12,0,0,143,145,3,64,32,0,144,142,1,0,0,0,144,145,1,0,0,0,145,11,1,0,
    0,0,146,147,5,13,0,0,147,13,1,0,0,0,148,149,5,14,0,0,149,150,3,64,32,
    0,150,15,1,0,0,0,151,152,5,5,0,0,152,17,1,0,0,0,153,155,5,15,0,0,154,
    156,3,64,32,0,155,154,1,0,0,0,155,156,1,0,0,0,156,19,1,0,0,0,157,159,
    5,16,0,0,158,160,5,17,0,0,159,158,1,0,0,0,159,160,1,0,0,0,160,21,1,0,
    0,0,161,162,5,18,0,0,162,23,1,0,0,0,163,167,5,19,0,0,164,166,3,26,13,
    0,165,164,1,0,0,0,166,169,1,0,0,0,167,165,1,0,0,0,167,168,1,0,0,0,168,
    25,1,0,0,0,169,167,1,0,0,0,170,171,7,1,0,0,171,176,3,64,32,0,172,173,
    5,24,0,0,173,176,5,120,0,0,174,176,5,25,0,0,175,170,1,0,0,0,175,172,1,
    0,0,0,175,174,1,0,0,0,176,27,1,0,0,0,177,215,5,26,0,0,178,179,5,27,0,
    0,179,181,3,64,32,0,180,182,3,30,15,0,181,180,1,0,0,0,182,183,1,0,0,0,
    183,181,1,0,0,0,183,184,1,0,0,0,184,216,1,0,0,0,185,186,5,28,0,0,186,
    190,3,64,32,0,187,189,3,32,16,0,188,187,1,0,0,0,189,192,1,0,0,0,190,188,
    1,0,0,0,190,191,1,0,0,0,191,216,1,0,0,0,192,190,1,0,0,0,193,194,5,29,
    0,0,194,195,3,64,32,0,195,196,5,30,0,0,196,200,3,64,32,0,197,199,3,36,
    18,0,198,197,1,0,0,0,199,202,1,0,0,0,200,198,1,0,0,0,200,201,1,0,0,0,
    201,216,1,0,0,0,202,200,1,0,0,0,203,204,5,31,0,0,204,205,3,64,32,0,205,
    206,5,30,0,0,206,210,3,64,32,0,207,209,3,34,17,0,208,207,1,0,0,0,209,
    212,1,0,0,0,210,208,1,0,0,0,210,211,1,0,0,0,211,216,1,0,0,0,212,210,1,
    0,0,0,213,214,5,32,0,0,214,216,5,120,0,0,215,178,1,0,0,0,215,185,1,0,
    0,0,215,193,1,0,0,0,215,203,1,0,0,0,215,213,1,0,0,0,216,219,1,0,0,0,217,
    219,5,33,0,0,218,177,1,0,0,0,218,217,1,0,0,0,219,29,1,0,0,0,220,221,7,
    2,0,0,221,224,3,64,32,0,222,224,5,123,0,0,223,220,1,0,0,0,223,222,1,0,
    0,0,224,31,1,0,0,0,225,226,7,3,0,0,226,227,3,64,32,0,227,33,1,0,0,0,228,
    232,3,32,16,0,229,232,5,41,0,0,230,232,5,42,0,0,231,228,1,0,0,0,231,229,
    1,0,0,0,231,230,1,0,0,0,232,35,1,0,0,0,233,239,3,34,17,0,234,235,5,43,
    0,0,235,239,5,120,0,0,236,239,5,44,0,0,237,239,5,45,0,0,238,233,1,0,0,
    0,238,234,1,0,0,0,238,236,1,0,0,0,238,237,1,0,0,0,239,37,1,0,0,0,240,
    270,5,46,0,0,241,242,5,47,0,0,242,243,3,64,32,0,243,244,5,48,0,0,244,
    245,3,64,32,0,245,271,1,0,0,0,246,247,5,49,0,0,247,271,3,64,32,0,248,
    249,5,50,0,0,249,271,3,64,32,0,250,251,5,31,0,0,251,252,3,64,32,0,252,
    253,5,51,0,0,253,257,3,64,32,0,254,256,3,34,17,0,255,254,1,0,0,0,256,
    259,1,0,0,0,257,255,1,0,0,0,257,258,1,0,0,0,258,271,1,0,0,0,259,257,1,
    0,0,0,260,261,5,52,0,0,261,262,3,64,32,0,262,263,5,51,0,0,263,267,3,64,
    32,0,264,266,3,36,18,0,265,264,1,0,0,0,266,269,1,0,0,0,267,265,1,0,0,
    0,267,268,1,0,0,0,268,271,1,0,0,0,269,267,1,0,0,0,270,241,1,0,0,0,270,
    246,1,0,0,0,270,248,1,0,0,0,270,250,1,0,0,0,270,260,1,0,0,0,271,39,1,
    0,0,0,272,308,5,53,0,0,273,275,3,46,23,0,274,273,1,0,0,0,275,278,1,0,
    0,0,276,274,1,0,0,0,276,277,1,0,0,0,277,309,1,0,0,0,278,276,1,0,0,0,279,
    280,5,54,0,0,280,283,3,64,32,0,281,282,5,24,0,0,282,284,5,120,0,0,283,
    281,1,0,0,0,283,284,1,0,0,0,284,309,1,0,0,0,285,286,5,50,0,0,286,287,
    3,64,32,0,287,288,5,55,0,0,288,289,5,120,0,0,289,309,1,0,0,0,290,291,
    5,56,0,0,291,309,3,64,32,0,292,293,5,57,0,0,293,309,3,64,32,0,294,295,
    5,58,0,0,295,309,3,64,32,0,296,297,5,59,0,0,297,298,3,64,32,0,298,299,
    5,48,0,0,299,300,3,64,32,0,300,309,1,0,0,0,301,302,5,60,0,0,302,303,3,
    64,32,0,303,304,5,61,0,0,304,305,5,62,0,0,305,306,5,37,0,0,306,307,3,
    64,32,0,307,309,1,0,0,0,308,276,1,0,0,0,308,279,1,0,0,0,308,285,1,0,0,
    0,308,290,1,0,0,0,308,292,1,0,0,0,308,294,1,0,0,0,308,296,1,0,0,0,308,
    301,1,0,0,0,309,41,1,0,0,0,310,314,5,63,0,0,311,313,3,46,23,0,312,311,
    1,0,0,0,313,316,1,0,0,0,314,312,1,0,0,0,314,315,1,0,0,0,315,43,1,0,0,
    0,316,314,1,0,0,0,317,318,5,64,0,0,318,323,3,64,32,0,319,322,3,46,23,
    0,320,322,5,124,0,0,321,319,1,0,0,0,321,320,1,0,0,0,322,325,1,0,0,0,323,
    321,1,0,0,0,323,324,1,0,0,0,324,45,1,0,0,0,325,323,1,0,0,0,326,327,7,
    4,0,0,327,334,5,120,0,0,328,329,5,69,0,0,329,334,7,5,0,0,330,331,7,6,
    0,0,331,334,3,64,32,0,332,334,5,74,0,0,333,326,1,0,0,0,333,328,1,0,0,
    0,333,330,1,0,0,0,333,332,1,0,0,0,334,47,1,0,0,0,335,364,5,75,0,0,336,
    337,5,47,0,0,337,365,3,64,32,0,338,365,5,76,0,0,339,340,7,7,0,0,340,345,
    3,64,32,0,341,342,7,8,0,0,342,344,3,64,32,0,343,341,1,0,0,0,344,347,1,
    0,0,0,345,343,1,0,0,0,345,346,1,0,0,0,346,365,1,0,0,0,347,345,1,0,0,0,
    348,362,5,80,0,0,349,352,5,81,0,0,350,353,3,64,32,0,351,353,5,122,0,0,
    352,350,1,0,0,0,352,351,1,0,0,0,353,354,1,0,0,0,354,352,1,0,0,0,354,355,
    1,0,0,0,355,363,1,0,0,0,356,363,5,58,0,0,357,358,5,82,0,0,358,359,3,64,
    32,0,359,360,5,83,0,0,360,361,3,64,32,0,361,363,1,0,0,0,362,349,1,0,0,
    0,362,356,1,0,0,0,362,357,1,0,0,0,362,363,1,0,0,0,363,365,1,0,0,0,364,
    336,1,0,0,0,364,338,1,0,0,0,364,339,1,0,0,0,364,348,1,0,0,0,365,49,1,
    0,0,0,366,367,5,84,0,0,367,368,3,64,32,0,368,51,1,0,0,0,369,370,5,85,
    0,0,370,371,3,64,32,0,371,53,1,0,0,0,372,373,5,86,0,0,373,374,3,64,32,
    0,374,55,1,0,0,0,375,384,5,87,0,0,376,378,5,1,0,0,377,376,1,0,0,0,377,
    378,1,0,0,0,378,380,1,0,0,0,379,381,3,64,32,0,380,379,1,0,0,0,381,382,
    1,0,0,0,382,380,1,0,0,0,382,383,1,0,0,0,383,385,1,0,0,0,384,377,1,0,0,
    0,384,385,1,0,0,0,385,57,1,0,0,0,386,387,7,9,0,0,387,59,1,0,0,0,388,404,
    5,90,0,0,389,391,5,91,0,0,390,392,3,62,31,0,391,390,1,0,0,0,392,393,1,
    0,0,0,393,391,1,0,0,0,393,394,1,0,0,0,394,405,1,0,0,0,395,396,5,92,0,
    0,396,397,5,93,0,0,397,400,3,64,32,0,398,399,5,30,0,0,399,401,3,64,32,
    0,400,398,1,0,0,0,400,401,1,0,0,0,401,405,1,0,0,0,402,403,5,32,0,0,403,
    405,3,64,32,0,404,389,1,0,0,0,404,395,1,0,0,0,404,402,1,0,0,0,405,61,
    1,0,0,0,406,407,7,10,0,0,407,408,3,64,32,0,408,63,1,0,0,0,409,414,5,121,
    0,0,410,414,5,125,0,0,411,414,5,120,0,0,412,414,3,66,33,0,413,409,1,0,
    0,0,413,410,1,0,0,0,413,411,1,0,0,0,413,412,1,0,0,0,414,65,1,0,0,0,415,
    416,7,11,0,0,416,67,1,0,0,0,417,441,5,97,0,0,418,442,5,98,0,0,419,420,
    5,99,0,0,420,421,3,64,32,0,421,422,5,23,0,0,422,427,3,64,32,0,423,424,
    5,65,0,0,424,428,5,120,0,0,425,426,5,79,0,0,426,428,3,64,32,0,427,423,
    1,0,0,0,427,425,1,0,0,0,428,442,1,0,0,0,429,430,5,100,0,0,430,431,3,64,
    32,0,431,432,5,23,0,0,432,433,3,64,32,0,433,434,5,65,0,0,434,435,5,120,
    0,0,435,436,5,111,0,0,436,439,3,64,32,0,437,438,5,24,0,0,438,440,5,120,
    0,0,439,437,1,0,0,0,439,440,1,0,0,0,440,442,1,0,0,0,441,418,1,0,0,0,441,
    419,1,0,0,0,441,429,1,0,0,0,442,69,1,0,0,0,443,444,5,26,0,0,444,461,5,
    101,0,0,445,446,3,64,32,0,446,447,5,23,0,0,447,450,3,64,32,0,448,449,
    5,20,0,0,449,451,3,64,32,0,450,448,1,0,0,0,450,451,1,0,0,0,451,462,1,
    0,0,0,452,453,5,20,0,0,453,454,3,64,32,0,454,455,5,112,0,0,455,456,3,
    64,32,0,456,457,5,113,0,0,457,458,3,64,32,0,458,459,5,23,0,0,459,460,
    3,64,32,0,460,462,1,0,0,0,461,445,1,0,0,0,461,452,1,0,0,0,462,71,1,0,
    0,0,463,464,5,26,0,0,464,465,5,102,0,0,465,466,5,120,0,0,466,467,5,30,
    0,0,467,468,3,64,32,0,468,73,1,0,0,0,469,470,5,103,0,0,470,474,3,64,32,
    0,471,473,3,76,38,0,472,471,1,0,0,0,473,476,1,0,0,0,474,472,1,0,0,0,474,
    475,1,0,0,0,475,482,1,0,0,0,476,474,1,0,0,0,477,479,7,12,0,0,478,480,
    5,120,0,0,479,478,1,0,0,0,479,480,1,0,0,0,480,482,1,0,0,0,481,469,1,0,
    0,0,481,477,1,0,0,0,482,75,1,0,0,0,483,484,7,13,0,0,484,488,3,64,32,0,
    485,486,5,66,0,0,486,488,5,120,0,0,487,483,1,0,0,0,487,485,1,0,0,0,488,
    77,1,0,0,0,489,490,5,106,0,0,490,497,3,64,32,0,491,492,5,114,0,0,492,
    496,3,64,32,0,493,494,5,66,0,0,494,496,5,120,0,0,495,491,1,0,0,0,495,
    493,1,0,0,0,496,499,1,0,0,0,497,495,1,0,0,0,497,498,1,0,0,0,498,506,1,
    0,0,0,499,497,1,0,0,0,500,503,5,107,0,0,501,502,5,66,0,0,502,504,5,120,
    0,0,503,501,1,0,0,0,503,504,1,0,0,0,504,506,1,0,0,0,505,489,1,0,0,0,505,
    500,1,0,0,0,506,79,1,0,0,0,507,511,5,108,0,0,508,509,5,109,0,0,509,511,
    5,110,0,0,510,507,1,0,0,0,510,508,1,0,0,0,511,516,1,0,0,0,512,513,7,14,
    0,0,513,515,5,120,0,0,514,512,1,0,0,0,515,518,1,0,0,0,516,514,1,0,0,0,
    516,517,1,0,0,0,517,81,1,0,0,0,518,516,1,0,0,0,56,114,121,128,137,139,
    144,155,159,167,175,183,190,200,210,215,218,223,231,238,257,267,270,276,
    283,308,314,321,323,333,345,352,354,362,364,377,382,384,393,400,404,413,
    427,439,441,450,461,474,479,481,487,495,497,503,505,510,516
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
    setState(82);
    match(TracerCommandsParser::T__0);
    setState(83);
    command();
    setState(84);
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
    setState(114);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 0, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(86);
      server();
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(87);
      attach();
      break;
    }

    case 3: {
      enterOuterAlt(_localctx, 3);
      setState(88);
      processes();
      break;
    }

    case 4: {
      enterOuterAlt(_localctx, 4);
      setState(89);
      sessions();
      break;
    }

    case 5: {
      enterOuterAlt(_localctx, 5);
      setState(90);
      use();
      break;
    }

    case 6: {
      enterOuterAlt(_localctx, 6);
      setState(91);
      status();
      break;
    }

    case 7: {
      enterOuterAlt(_localctx, 7);
      setState(92);
      detach();
      break;
    }

    case 8: {
      enterOuterAlt(_localctx, 8);
      setState(93);
      modules();
      break;
    }

    case 9: {
      enterOuterAlt(_localctx, 9);
      setState(94);
      threads();
      break;
    }

    case 10: {
      enterOuterAlt(_localctx, 10);
      setState(95);
      symbols();
      break;
    }

    case 11: {
      enterOuterAlt(_localctx, 11);
      setState(96);
      probe();
      break;
    }

    case 12: {
      enterOuterAlt(_localctx, 12);
      setState(97);
      interceptor();
      break;
    }

    case 13: {
      enterOuterAlt(_localctx, 13);
      setState(98);
      events();
      break;
    }

    case 14: {
      enterOuterAlt(_localctx, 14);
      setState(99);
      collect();
      break;
    }

    case 15: {
      enterOuterAlt(_localctx, 15);
      setState(100);
      record();
      break;
    }

    case 16: {
      enterOuterAlt(_localctx, 16);
      setState(101);
      cpp();
      break;
    }

    case 17: {
      enterOuterAlt(_localctx, 17);
      setState(102);
      shell();
      break;
    }

    case 18: {
      enterOuterAlt(_localctx, 18);
      setState(103);
      complete();
      break;
    }

    case 19: {
      enterOuterAlt(_localctx, 19);
      setState(104);
      type();
      break;
    }

    case 20: {
      enterOuterAlt(_localctx, 20);
      setState(105);
      help();
      break;
    }

    case 21: {
      enterOuterAlt(_localctx, 21);
      setState(106);
      quit();
      break;
    }

    case 22: {
      enterOuterAlt(_localctx, 22);
      setState(107);
      bulkTrace();
      break;
    }

    case 23: {
      enterOuterAlt(_localctx, 23);
      setState(108);
      memory();
      break;
    }

    case 24: {
      enterOuterAlt(_localctx, 24);
      setState(109);
      probeEntry();
      break;
    }

    case 25: {
      enterOuterAlt(_localctx, 25);
      setState(110);
      probeReload();
      break;
    }

    case 26: {
      enterOuterAlt(_localctx, 26);
      setState(111);
      targetLaunch();
      break;
    }

    case 27: {
      enterOuterAlt(_localctx, 27);
      setState(112);
      targetCall();
      break;
    }

    case 28: {
      enterOuterAlt(_localctx, 28);
      setState(113);
      localPerf();
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
    setState(116);
    match(TracerCommandsParser::T__1);
    setState(128);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__2: {
        setState(117);
        match(TracerCommandsParser::T__2);
        setState(121);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__6

        || _la == TracerCommandsParser::T__7) {
          setState(118);
          serverOption();
          setState(123);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__3: {
        setState(124);
        match(TracerCommandsParser::T__3);
        setState(125);
        value();
        break;
      }

      case TracerCommandsParser::T__4: {
        setState(126);
        match(TracerCommandsParser::T__4);
        break;
      }

      case TracerCommandsParser::T__5: {
        setState(127);
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
    setState(130);
    _la = _input->LA(1);
    if (!(_la == TracerCommandsParser::T__6

    || _la == TracerCommandsParser::T__7)) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(131);
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
    setState(133);
    match(TracerCommandsParser::T__8);
    setState(139);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::UINT) {
      setState(134);
      match(TracerCommandsParser::UINT);
      setState(137);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == TracerCommandsParser::T__9) {
        setState(135);
        match(TracerCommandsParser::T__9);
        setState(136);
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
    setState(141);
    match(TracerCommandsParser::T__10);
    setState(144);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::T__11) {
      setState(142);
      match(TracerCommandsParser::T__11);
      setState(143);
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
    setState(146);
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
    setState(148);
    match(TracerCommandsParser::T__13);
    setState(149);
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
    setState(151);
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
    setState(153);
    match(TracerCommandsParser::T__14);
    setState(155);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & -2344475433727300996) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 64)) & 2522156520762210497) != 0)) {
      setState(154);
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
    setState(157);
    match(TracerCommandsParser::T__15);
    setState(159);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::T__16) {
      setState(158);
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
    setState(161);
    match(TracerCommandsParser::T__17);

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
    setState(163);
    match(TracerCommandsParser::T__18);
    setState(167);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 66064384) != 0)) {
      setState(164);
      symbolOption();
      setState(169);
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
    setState(175);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__11:
      case TracerCommandsParser::T__19:
      case TracerCommandsParser::T__20:
      case TracerCommandsParser::T__21:
      case TracerCommandsParser::T__22: {
        enterOuterAlt(_localctx, 1);
        setState(170);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 15732736) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(171);
        value();
        break;
      }

      case TracerCommandsParser::T__23: {
        enterOuterAlt(_localctx, 2);
        setState(172);
        match(TracerCommandsParser::T__23);
        setState(173);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__24: {
        enterOuterAlt(_localctx, 3);
        setState(174);
        match(TracerCommandsParser::T__24);
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
    setState(218);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__25: {
        enterOuterAlt(_localctx, 1);
        setState(177);
        match(TracerCommandsParser::T__25);
        setState(215);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__26: {
            setState(178);
            match(TracerCommandsParser::T__26);
            setState(179);
            value();
            setState(181);
            _errHandler->sync(this);
            _la = _input->LA(1);
            do {
              setState(180);
              prepareOption();
              setState(183);
              _errHandler->sync(this);
              _la = _input->LA(1);
            } while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 257699086336) != 0) || _la == TracerCommandsParser::METADATA_ONLY);
            break;
          }

          case TracerCommandsParser::T__27: {
            setState(185);
            match(TracerCommandsParser::T__27);
            setState(186);
            value();
            setState(190);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 1975686004736) != 0)) {
              setState(187);
              resolveOption();
              setState(192);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__28: {
            setState(193);
            match(TracerCommandsParser::T__28);
            setState(194);
            value();
            setState(195);
            match(TracerCommandsParser::T__29);
            setState(196);
            value();
            setState(200);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 70145406926848) != 0)) {
              setState(197);
              deployOption();
              setState(202);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__30: {
            setState(203);
            match(TracerCommandsParser::T__30);
            setState(204);
            value();
            setState(205);
            match(TracerCommandsParser::T__29);
            setState(206);
            value();
            setState(210);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 8572755771392) != 0)) {
              setState(207);
              validateOption();
              setState(212);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__31: {
            setState(213);
            match(TracerCommandsParser::T__31);
            setState(214);
            match(TracerCommandsParser::UINT);
            break;
          }

        default:
          throw NoViableAltException(this);
        }
        break;
      }

      case TracerCommandsParser::T__32: {
        enterOuterAlt(_localctx, 2);
        setState(217);
        match(TracerCommandsParser::T__32);
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
    setState(223);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__19:
      case TracerCommandsParser::T__33:
      case TracerCommandsParser::T__34:
      case TracerCommandsParser::T__35:
      case TracerCommandsParser::T__36: {
        enterOuterAlt(_localctx, 1);
        setState(220);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 257699086336) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(221);
        value();
        break;
      }

      case TracerCommandsParser::METADATA_ONLY: {
        enterOuterAlt(_localctx, 2);
        setState(222);
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
    setState(225);
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 1975686004736) != 0))) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(226);
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
    setState(231);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__19:
      case TracerCommandsParser::T__33:
      case TracerCommandsParser::T__34:
      case TracerCommandsParser::T__37:
      case TracerCommandsParser::T__38:
      case TracerCommandsParser::T__39: {
        enterOuterAlt(_localctx, 1);
        setState(228);
        resolveOption();
        break;
      }

      case TracerCommandsParser::T__40: {
        enterOuterAlt(_localctx, 2);
        setState(229);
        match(TracerCommandsParser::T__40);
        break;
      }

      case TracerCommandsParser::T__41: {
        enterOuterAlt(_localctx, 3);
        setState(230);
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
    setState(238);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__19:
      case TracerCommandsParser::T__33:
      case TracerCommandsParser::T__34:
      case TracerCommandsParser::T__37:
      case TracerCommandsParser::T__38:
      case TracerCommandsParser::T__39:
      case TracerCommandsParser::T__40:
      case TracerCommandsParser::T__41: {
        enterOuterAlt(_localctx, 1);
        setState(233);
        validateOption();
        break;
      }

      case TracerCommandsParser::T__42: {
        enterOuterAlt(_localctx, 2);
        setState(234);
        match(TracerCommandsParser::T__42);
        setState(235);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__43: {
        enterOuterAlt(_localctx, 3);
        setState(236);
        match(TracerCommandsParser::T__43);
        break;
      }

      case TracerCommandsParser::T__44: {
        enterOuterAlt(_localctx, 4);
        setState(237);
        match(TracerCommandsParser::T__44);
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
    setState(240);
    match(TracerCommandsParser::T__45);
    setState(270);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__46: {
        setState(241);
        match(TracerCommandsParser::T__46);
        setState(242);
        value();
        setState(243);
        match(TracerCommandsParser::T__47);
        setState(244);
        value();
        break;
      }

      case TracerCommandsParser::T__48: {
        setState(246);
        match(TracerCommandsParser::T__48);
        setState(247);
        value();
        break;
      }

      case TracerCommandsParser::T__49: {
        setState(248);
        match(TracerCommandsParser::T__49);
        setState(249);
        value();
        break;
      }

      case TracerCommandsParser::T__30: {
        setState(250);
        match(TracerCommandsParser::T__30);
        setState(251);
        value();
        setState(252);
        match(TracerCommandsParser::T__50);
        setState(253);
        value();
        setState(257);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 8572755771392) != 0)) {
          setState(254);
          validateOption();
          setState(259);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__51: {
        setState(260);
        match(TracerCommandsParser::T__51);
        setState(261);
        value();
        setState(262);
        match(TracerCommandsParser::T__50);
        setState(263);
        value();
        setState(267);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 70145406926848) != 0)) {
          setState(264);
          deployOption();
          setState(269);
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
    setState(272);
    match(TracerCommandsParser::T__52);
    setState(308);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::EOF:
      case TracerCommandsParser::T__47:
      case TracerCommandsParser::T__64:
      case TracerCommandsParser::T__65:
      case TracerCommandsParser::T__66:
      case TracerCommandsParser::T__67:
      case TracerCommandsParser::T__68:
      case TracerCommandsParser::T__71:
      case TracerCommandsParser::T__72:
      case TracerCommandsParser::T__73: {
        setState(276);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (((((_la - 48) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 48)) & 121503745) != 0)) {
          setState(273);
          streamOption();
          setState(278);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__53: {
        setState(279);
        match(TracerCommandsParser::T__53);
        setState(280);
        value();
        setState(283);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__23) {
          setState(281);
          match(TracerCommandsParser::T__23);
          setState(282);
          match(TracerCommandsParser::UINT);
        }
        break;
      }

      case TracerCommandsParser::T__49: {
        setState(285);
        match(TracerCommandsParser::T__49);
        setState(286);
        value();
        setState(287);
        match(TracerCommandsParser::T__54);
        setState(288);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__55: {
        setState(290);
        match(TracerCommandsParser::T__55);
        setState(291);
        value();
        break;
      }

      case TracerCommandsParser::T__56: {
        setState(292);
        match(TracerCommandsParser::T__56);
        setState(293);
        value();
        break;
      }

      case TracerCommandsParser::T__57: {
        setState(294);
        match(TracerCommandsParser::T__57);
        setState(295);
        value();
        break;
      }

      case TracerCommandsParser::T__58: {
        setState(296);
        match(TracerCommandsParser::T__58);
        setState(297);
        value();
        setState(298);
        match(TracerCommandsParser::T__47);
        setState(299);
        value();
        break;
      }

      case TracerCommandsParser::T__59: {
        setState(301);
        match(TracerCommandsParser::T__59);
        setState(302);
        value();
        setState(303);
        match(TracerCommandsParser::T__60);
        setState(304);
        match(TracerCommandsParser::T__61);
        setState(305);
        match(TracerCommandsParser::T__36);
        setState(306);
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
    setState(310);
    match(TracerCommandsParser::T__62);
    setState(314);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (((((_la - 48) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 48)) & 121503745) != 0)) {
      setState(311);
      streamOption();
      setState(316);
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
    setState(317);
    match(TracerCommandsParser::T__63);
    setState(318);
    value();
    setState(323);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == TracerCommandsParser::T__47 || ((((_la - 65) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 65)) & 576460752303424415) != 0)) {
      setState(321);
      _errHandler->sync(this);
      switch (_input->LA(1)) {
        case TracerCommandsParser::T__47:
        case TracerCommandsParser::T__64:
        case TracerCommandsParser::T__65:
        case TracerCommandsParser::T__66:
        case TracerCommandsParser::T__67:
        case TracerCommandsParser::T__68:
        case TracerCommandsParser::T__71:
        case TracerCommandsParser::T__72:
        case TracerCommandsParser::T__73: {
          setState(319);
          streamOption();
          break;
        }

        case TracerCommandsParser::SQLITE: {
          setState(320);
          match(TracerCommandsParser::SQLITE);
          break;
        }

      default:
        throw NoViableAltException(this);
      }
      setState(325);
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
    setState(333);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__64:
      case TracerCommandsParser::T__65:
      case TracerCommandsParser::T__66:
      case TracerCommandsParser::T__67: {
        enterOuterAlt(_localctx, 1);
        setState(326);
        _la = _input->LA(1);
        if (!(((((_la - 65) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 65)) & 15) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(327);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__68: {
        enterOuterAlt(_localctx, 2);
        setState(328);
        match(TracerCommandsParser::T__68);
        setState(329);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__69

        || _la == TracerCommandsParser::T__70)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        break;
      }

      case TracerCommandsParser::T__47:
      case TracerCommandsParser::T__71:
      case TracerCommandsParser::T__72: {
        enterOuterAlt(_localctx, 3);
        setState(330);
        _la = _input->LA(1);
        if (!(((((_la - 48) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 48)) & 50331649) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(331);
        value();
        break;
      }

      case TracerCommandsParser::T__73: {
        enterOuterAlt(_localctx, 4);
        setState(332);
        match(TracerCommandsParser::T__73);
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
    setState(335);
    match(TracerCommandsParser::T__74);
    setState(364);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__46: {
        setState(336);
        match(TracerCommandsParser::T__46);
        setState(337);
        value();
        break;
      }

      case TracerCommandsParser::T__75: {
        setState(338);
        match(TracerCommandsParser::T__75);
        break;
      }

      case TracerCommandsParser::T__76:
      case TracerCommandsParser::T__77: {
        setState(339);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__76

        || _la == TracerCommandsParser::T__77)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(340);
        value();
        setState(345);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__36

        || _la == TracerCommandsParser::T__78) {
          setState(341);
          _la = _input->LA(1);
          if (!(_la == TracerCommandsParser::T__36

          || _la == TracerCommandsParser::T__78)) {
          _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(342);
          value();
          setState(347);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__79: {
        setState(348);
        match(TracerCommandsParser::T__79);
        setState(362);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__80: {
            setState(349);
            match(TracerCommandsParser::T__80);
            setState(352);
            _errHandler->sync(this);
            _la = _input->LA(1);
            do {
              setState(352);
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
                case TracerCommandsParser::T__25:
                case TracerCommandsParser::T__26:
                case TracerCommandsParser::T__27:
                case TracerCommandsParser::T__28:
                case TracerCommandsParser::T__30:
                case TracerCommandsParser::T__31:
                case TracerCommandsParser::T__32:
                case TracerCommandsParser::T__45:
                case TracerCommandsParser::T__46:
                case TracerCommandsParser::T__48:
                case TracerCommandsParser::T__49:
                case TracerCommandsParser::T__51:
                case TracerCommandsParser::T__52:
                case TracerCommandsParser::T__53:
                case TracerCommandsParser::T__55:
                case TracerCommandsParser::T__56:
                case TracerCommandsParser::T__57:
                case TracerCommandsParser::T__58:
                case TracerCommandsParser::T__59:
                case TracerCommandsParser::T__61:
                case TracerCommandsParser::T__62:
                case TracerCommandsParser::T__63:
                case TracerCommandsParser::T__69:
                case TracerCommandsParser::T__70:
                case TracerCommandsParser::T__74:
                case TracerCommandsParser::T__75:
                case TracerCommandsParser::T__76:
                case TracerCommandsParser::T__77:
                case TracerCommandsParser::T__79:
                case TracerCommandsParser::T__80:
                case TracerCommandsParser::T__81:
                case TracerCommandsParser::T__83:
                case TracerCommandsParser::T__84:
                case TracerCommandsParser::T__85:
                case TracerCommandsParser::T__86:
                case TracerCommandsParser::T__87:
                case TracerCommandsParser::T__88:
                case TracerCommandsParser::T__89:
                case TracerCommandsParser::T__90:
                case TracerCommandsParser::T__91:
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
                case TracerCommandsParser::T__109:
                case TracerCommandsParser::UINT:
                case TracerCommandsParser::STRING:
                case TracerCommandsParser::WORD: {
                  setState(350);
                  value();
                  break;
                }

                case TracerCommandsParser::SHELL: {
                  setState(351);
                  match(TracerCommandsParser::SHELL);
                  break;
                }

              default:
                throw NoViableAltException(this);
              }
              setState(354);
              _errHandler->sync(this);
              _la = _input->LA(1);
            } while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & -2344475433727300996) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 64)) & 2810386896913922241) != 0));
            break;
          }

          case TracerCommandsParser::T__57: {
            setState(356);
            match(TracerCommandsParser::T__57);
            break;
          }

          case TracerCommandsParser::T__81: {
            setState(357);
            match(TracerCommandsParser::T__81);
            setState(358);
            value();
            setState(359);
            match(TracerCommandsParser::T__82);
            setState(360);
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
    setState(366);
    match(TracerCommandsParser::T__83);
    setState(367);
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
    setState(369);
    match(TracerCommandsParser::T__84);
    setState(370);
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
    setState(372);
    match(TracerCommandsParser::T__85);
    setState(373);
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
    setState(375);
    match(TracerCommandsParser::T__86);
    setState(384);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & -2344475433727300994) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 64)) & 2522156520762210497) != 0)) {
      setState(377);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == TracerCommandsParser::T__0) {
        setState(376);
        match(TracerCommandsParser::T__0);
      }
      setState(380);
      _errHandler->sync(this);
      _la = _input->LA(1);
      do {
        setState(379);
        value();
        setState(382);
        _errHandler->sync(this);
        _la = _input->LA(1);
      } while ((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & -2344475433727300996) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 64)) & 2522156520762210497) != 0));
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
    setState(386);
    _la = _input->LA(1);
    if (!(_la == TracerCommandsParser::T__87

    || _la == TracerCommandsParser::T__88)) {
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
    setState(388);
    match(TracerCommandsParser::T__89);
    setState(404);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__90: {
        setState(389);
        match(TracerCommandsParser::T__90);
        setState(391);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(390);
          bulkTraceOption();
          setState(393);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while (_la == TracerCommandsParser::T__19

        || _la == TracerCommandsParser::T__33 || ((((_la - 93) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 93)) & 15) != 0));
        break;
      }

      case TracerCommandsParser::T__91: {
        setState(395);
        match(TracerCommandsParser::T__91);
        setState(396);
        match(TracerCommandsParser::T__92);
        setState(397);
        value();
        setState(400);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__29) {
          setState(398);
          match(TracerCommandsParser::T__29);
          setState(399);
          value();
        }
        break;
      }

      case TracerCommandsParser::T__31: {
        setState(402);
        match(TracerCommandsParser::T__31);
        setState(403);
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
    setState(406);
    _la = _input->LA(1);
    if (!(_la == TracerCommandsParser::T__19

    || _la == TracerCommandsParser::T__33 || ((((_la - 93) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 93)) & 15) != 0))) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(407);
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
    setState(413);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::STRING: {
        enterOuterAlt(_localctx, 1);
        setState(409);
        match(TracerCommandsParser::STRING);
        break;
      }

      case TracerCommandsParser::WORD: {
        enterOuterAlt(_localctx, 2);
        setState(410);
        match(TracerCommandsParser::WORD);
        break;
      }

      case TracerCommandsParser::UINT: {
        enterOuterAlt(_localctx, 3);
        setState(411);
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
      case TracerCommandsParser::T__25:
      case TracerCommandsParser::T__26:
      case TracerCommandsParser::T__27:
      case TracerCommandsParser::T__28:
      case TracerCommandsParser::T__30:
      case TracerCommandsParser::T__31:
      case TracerCommandsParser::T__32:
      case TracerCommandsParser::T__45:
      case TracerCommandsParser::T__46:
      case TracerCommandsParser::T__48:
      case TracerCommandsParser::T__49:
      case TracerCommandsParser::T__51:
      case TracerCommandsParser::T__52:
      case TracerCommandsParser::T__53:
      case TracerCommandsParser::T__55:
      case TracerCommandsParser::T__56:
      case TracerCommandsParser::T__57:
      case TracerCommandsParser::T__58:
      case TracerCommandsParser::T__59:
      case TracerCommandsParser::T__61:
      case TracerCommandsParser::T__62:
      case TracerCommandsParser::T__63:
      case TracerCommandsParser::T__69:
      case TracerCommandsParser::T__70:
      case TracerCommandsParser::T__74:
      case TracerCommandsParser::T__75:
      case TracerCommandsParser::T__76:
      case TracerCommandsParser::T__77:
      case TracerCommandsParser::T__79:
      case TracerCommandsParser::T__80:
      case TracerCommandsParser::T__81:
      case TracerCommandsParser::T__83:
      case TracerCommandsParser::T__84:
      case TracerCommandsParser::T__85:
      case TracerCommandsParser::T__86:
      case TracerCommandsParser::T__87:
      case TracerCommandsParser::T__88:
      case TracerCommandsParser::T__89:
      case TracerCommandsParser::T__90:
      case TracerCommandsParser::T__91:
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
      case TracerCommandsParser::T__109: {
        enterOuterAlt(_localctx, 4);
        setState(412);
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
    setState(415);
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & -2344475433727300996) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 64)) & 140729434732737) != 0))) {
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
    setState(417);
    match(TracerCommandsParser::T__96);
    setState(441);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__97: {
        setState(418);
        match(TracerCommandsParser::T__97);
        break;
      }

      case TracerCommandsParser::T__98: {
        setState(419);
        match(TracerCommandsParser::T__98);
        setState(420);
        value();
        setState(421);
        match(TracerCommandsParser::T__22);
        setState(422);
        value();
        setState(427);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__64: {
            setState(423);
            match(TracerCommandsParser::T__64);
            setState(424);
            match(TracerCommandsParser::UINT);
            break;
          }

          case TracerCommandsParser::T__78: {
            setState(425);
            match(TracerCommandsParser::T__78);
            setState(426);
            value();
            break;
          }

        default:
          throw NoViableAltException(this);
        }
        break;
      }

      case TracerCommandsParser::T__99: {
        setState(429);
        match(TracerCommandsParser::T__99);
        setState(430);
        value();
        setState(431);
        match(TracerCommandsParser::T__22);
        setState(432);
        value();
        setState(433);
        match(TracerCommandsParser::T__64);
        setState(434);
        match(TracerCommandsParser::UINT);
        setState(435);
        match(TracerCommandsParser::T__110);
        setState(436);
        value();
        setState(439);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__23) {
          setState(437);
          match(TracerCommandsParser::T__23);
          setState(438);
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
    setState(443);
    match(TracerCommandsParser::T__25);
    setState(444);
    match(TracerCommandsParser::T__100);
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
      case TracerCommandsParser::T__25:
      case TracerCommandsParser::T__26:
      case TracerCommandsParser::T__27:
      case TracerCommandsParser::T__28:
      case TracerCommandsParser::T__30:
      case TracerCommandsParser::T__31:
      case TracerCommandsParser::T__32:
      case TracerCommandsParser::T__45:
      case TracerCommandsParser::T__46:
      case TracerCommandsParser::T__48:
      case TracerCommandsParser::T__49:
      case TracerCommandsParser::T__51:
      case TracerCommandsParser::T__52:
      case TracerCommandsParser::T__53:
      case TracerCommandsParser::T__55:
      case TracerCommandsParser::T__56:
      case TracerCommandsParser::T__57:
      case TracerCommandsParser::T__58:
      case TracerCommandsParser::T__59:
      case TracerCommandsParser::T__61:
      case TracerCommandsParser::T__62:
      case TracerCommandsParser::T__63:
      case TracerCommandsParser::T__69:
      case TracerCommandsParser::T__70:
      case TracerCommandsParser::T__74:
      case TracerCommandsParser::T__75:
      case TracerCommandsParser::T__76:
      case TracerCommandsParser::T__77:
      case TracerCommandsParser::T__79:
      case TracerCommandsParser::T__80:
      case TracerCommandsParser::T__81:
      case TracerCommandsParser::T__83:
      case TracerCommandsParser::T__84:
      case TracerCommandsParser::T__85:
      case TracerCommandsParser::T__86:
      case TracerCommandsParser::T__87:
      case TracerCommandsParser::T__88:
      case TracerCommandsParser::T__89:
      case TracerCommandsParser::T__90:
      case TracerCommandsParser::T__91:
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
      case TracerCommandsParser::T__109:
      case TracerCommandsParser::UINT:
      case TracerCommandsParser::STRING:
      case TracerCommandsParser::WORD: {
        setState(445);
        value();
        setState(446);
        match(TracerCommandsParser::T__22);
        setState(447);
        value();
        setState(450);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__19) {
          setState(448);
          match(TracerCommandsParser::T__19);
          setState(449);
          value();
        }
        break;
      }

      case TracerCommandsParser::T__19: {
        setState(452);
        match(TracerCommandsParser::T__19);
        setState(453);
        value();
        setState(454);
        match(TracerCommandsParser::T__111);
        setState(455);
        value();
        setState(456);
        match(TracerCommandsParser::T__112);
        setState(457);
        value();
        setState(458);
        match(TracerCommandsParser::T__22);
        setState(459);
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

tree::TerminalNode* TracerCommandsParser::ProbeReloadContext::UINT() {
  return getToken(TracerCommandsParser::UINT, 0);
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
    match(TracerCommandsParser::T__25);
    setState(464);
    match(TracerCommandsParser::T__101);
    setState(465);
    match(TracerCommandsParser::UINT);
    setState(466);
    match(TracerCommandsParser::T__29);
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
    setState(481);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__102: {
        enterOuterAlt(_localctx, 1);
        setState(469);
        match(TracerCommandsParser::T__102);
        setState(470);
        value();
        setState(474);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (((((_la - 66) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 66)) & 17732923532771329) != 0)) {
          setState(471);
          launchOption();
          setState(476);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__103:
      case TracerCommandsParser::T__104: {
        enterOuterAlt(_localctx, 2);
        setState(477);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__103

        || _la == TracerCommandsParser::T__104)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(479);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::UINT) {
          setState(478);
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
    setState(487);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__113:
      case TracerCommandsParser::T__114:
      case TracerCommandsParser::T__115:
      case TracerCommandsParser::T__116:
      case TracerCommandsParser::T__117:
      case TracerCommandsParser::T__118: {
        enterOuterAlt(_localctx, 1);
        setState(483);
        _la = _input->LA(1);
        if (!(((((_la - 114) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 114)) & 63) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(484);
        value();
        break;
      }

      case TracerCommandsParser::T__65: {
        enterOuterAlt(_localctx, 2);
        setState(485);
        match(TracerCommandsParser::T__65);
        setState(486);
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
    setState(505);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__105: {
        enterOuterAlt(_localctx, 1);
        setState(489);
        match(TracerCommandsParser::T__105);
        setState(490);
        value();
        setState(497);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__65

        || _la == TracerCommandsParser::T__113) {
          setState(495);
          _errHandler->sync(this);
          switch (_input->LA(1)) {
            case TracerCommandsParser::T__113: {
              setState(491);
              match(TracerCommandsParser::T__113);
              setState(492);
              value();
              break;
            }

            case TracerCommandsParser::T__65: {
              setState(493);
              match(TracerCommandsParser::T__65);
              setState(494);
              match(TracerCommandsParser::UINT);
              break;
            }

          default:
            throw NoViableAltException(this);
          }
          setState(499);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__106: {
        enterOuterAlt(_localctx, 2);
        setState(500);
        match(TracerCommandsParser::T__106);
        setState(503);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__65) {
          setState(501);
          match(TracerCommandsParser::T__65);
          setState(502);
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
    setState(510);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__107: {
        setState(507);
        match(TracerCommandsParser::T__107);
        break;
      }

      case TracerCommandsParser::T__108: {
        setState(508);
        match(TracerCommandsParser::T__108);
        setState(509);
        match(TracerCommandsParser::T__109);
        break;
      }

    default:
      throw NoViableAltException(this);
    }
    setState(516);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == TracerCommandsParser::T__64

    || _la == TracerCommandsParser::T__65) {
      setState(512);
      _la = _input->LA(1);
      if (!(_la == TracerCommandsParser::T__64

      || _la == TracerCommandsParser::T__65)) {
      _errHandler->recoverInline(this);
      }
      else {
        _errHandler->reportMatch(this);
        consume();
      }
      setState(513);
      match(TracerCommandsParser::UINT);
      setState(518);
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
