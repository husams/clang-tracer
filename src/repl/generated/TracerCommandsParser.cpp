
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
      "launchOption", "targetCall", "localPerf", "instructions", "instructionOption"
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
      "'cpu'", "'instructions'", "'--pattern'", "'--elf-address'", "'--abi'",
      "'--arg'", "'--cwd'", "'--env'", "'--stdin'", "'--stdout'", "'--stderr'",
      "'--tid'", "'--size'", "", "", "", "'--metadata-only'", "'--sqlite'"
    },
    std::vector<std::string>{
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
      "", "", "", "", "UINT", "STRING", "SHELL", "METADATA_ONLY", "SQLITE",
      "WORD", "WS"
    }
  );
  static const int32_t serializedATNSegment[] = {
    4,1,129,542,2,0,7,0,2,1,7,1,2,2,7,2,2,3,7,3,2,4,7,4,2,5,7,5,2,6,7,6,2,
    7,7,7,2,8,7,8,2,9,7,9,2,10,7,10,2,11,7,11,2,12,7,12,2,13,7,13,2,14,7,
    14,2,15,7,15,2,16,7,16,2,17,7,17,2,18,7,18,2,19,7,19,2,20,7,20,2,21,7,
    21,2,22,7,22,2,23,7,23,2,24,7,24,2,25,7,25,2,26,7,26,2,27,7,27,2,28,7,
    28,2,29,7,29,2,30,7,30,2,31,7,31,2,32,7,32,2,33,7,33,2,34,7,34,2,35,7,
    35,2,36,7,36,2,37,7,37,2,38,7,38,2,39,7,39,2,40,7,40,2,41,7,41,2,42,7,
    42,1,0,1,0,1,0,1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,3,1,
    120,8,1,1,2,1,2,1,2,5,2,125,8,2,10,2,12,2,128,9,2,1,2,1,2,1,2,1,2,3,2,
    134,8,2,1,3,1,3,1,3,1,4,1,4,1,4,1,4,3,4,143,8,4,3,4,145,8,4,1,5,1,5,1,
    5,3,5,150,8,5,1,6,1,6,1,7,1,7,1,7,1,8,1,8,1,9,1,9,3,9,161,8,9,1,10,1,
    10,3,10,165,8,10,1,11,1,11,1,12,1,12,5,12,171,8,12,10,12,12,12,174,9,
    12,1,13,1,13,1,13,1,13,1,13,3,13,181,8,13,1,14,1,14,1,14,1,14,4,14,187,
    8,14,11,14,12,14,188,1,14,1,14,1,14,5,14,194,8,14,10,14,12,14,197,9,14,
    1,14,1,14,1,14,1,14,1,14,5,14,204,8,14,10,14,12,14,207,9,14,1,14,1,14,
    1,14,1,14,1,14,5,14,214,8,14,10,14,12,14,217,9,14,1,14,1,14,3,14,221,
    8,14,1,14,3,14,224,8,14,1,15,1,15,1,15,3,15,229,8,15,1,16,1,16,1,16,1,
    17,1,17,1,17,3,17,237,8,17,1,18,1,18,1,18,1,18,1,18,3,18,244,8,18,1,19,
    1,19,1,19,1,19,1,19,1,19,1,19,1,19,1,19,1,19,1,19,1,19,1,19,1,19,1,19,
    5,19,261,8,19,10,19,12,19,264,9,19,1,19,1,19,1,19,1,19,1,19,5,19,271,
    8,19,10,19,12,19,274,9,19,3,19,276,8,19,1,20,1,20,5,20,280,8,20,10,20,
    12,20,283,9,20,1,20,1,20,1,20,1,20,3,20,289,8,20,1,20,1,20,1,20,1,20,
    1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,
    1,20,1,20,1,20,1,20,1,20,3,20,314,8,20,1,21,1,21,5,21,318,8,21,10,21,
    12,21,321,9,21,1,22,1,22,1,22,1,22,5,22,327,8,22,10,22,12,22,330,9,22,
    1,23,1,23,1,23,1,23,1,23,1,23,1,23,3,23,339,8,23,1,24,1,24,1,24,1,24,
    1,24,1,24,1,24,1,24,5,24,349,8,24,10,24,12,24,352,9,24,1,24,1,24,1,24,
    1,24,4,24,358,8,24,11,24,12,24,359,1,24,1,24,1,24,1,24,1,24,1,24,3,24,
    368,8,24,3,24,370,8,24,1,25,1,25,1,25,1,26,1,26,1,26,1,27,1,27,1,27,1,
    28,1,28,3,28,383,8,28,1,28,4,28,386,8,28,11,28,12,28,387,3,28,390,8,28,
    1,29,1,29,1,30,1,30,1,30,4,30,397,8,30,11,30,12,30,398,1,30,1,30,1,30,
    1,30,1,30,3,30,406,8,30,1,30,1,30,3,30,410,8,30,1,31,1,31,1,31,1,32,1,
    32,1,32,1,32,3,32,419,8,32,1,33,1,33,1,34,1,34,1,34,1,34,1,34,1,34,1,
    34,1,34,1,34,1,34,3,34,433,8,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,
    34,1,34,1,34,3,34,445,8,34,3,34,447,8,34,1,35,1,35,1,35,1,35,1,35,1,35,
    1,35,3,35,456,8,35,1,35,1,35,1,35,1,35,1,35,1,35,1,35,1,35,1,35,3,35,
    467,8,35,1,36,1,36,1,36,1,36,1,36,1,36,1,37,1,37,1,37,5,37,478,8,37,10,
    37,12,37,481,9,37,1,37,1,37,3,37,485,8,37,3,37,487,8,37,1,38,1,38,1,38,
    1,38,3,38,493,8,38,1,39,1,39,1,39,1,39,1,39,1,39,5,39,501,8,39,10,39,
    12,39,504,9,39,1,39,1,39,1,39,3,39,509,8,39,3,39,511,8,39,1,40,1,40,1,
    40,3,40,516,8,40,1,40,1,40,5,40,520,8,40,10,40,12,40,523,9,40,1,41,1,
    41,1,41,5,41,528,8,41,10,41,12,41,531,9,41,1,41,3,41,534,8,41,1,42,1,
    42,1,42,1,42,3,42,540,8,42,1,42,0,0,43,0,2,4,6,8,10,12,14,16,18,20,22,
    24,26,28,30,32,34,36,38,40,42,44,46,48,50,52,54,56,58,60,62,64,66,68,
    70,72,74,76,78,80,82,84,0,17,1,0,7,8,2,0,12,12,20,23,2,0,20,20,34,37,
    3,0,20,20,34,35,38,40,1,0,65,68,1,0,70,71,2,0,48,48,72,73,1,0,77,78,2,
    0,37,37,79,79,1,0,88,89,3,0,20,20,34,34,93,96,17,0,2,6,9,9,11,11,13,16,
    18,19,26,29,31,33,46,47,49,50,52,54,56,60,62,64,70,71,75,78,80,82,84,
    92,97,111,1,0,104,105,1,0,115,120,1,0,65,66,2,0,65,66,121,122,1,0,22,
    23,612,0,86,1,0,0,0,2,119,1,0,0,0,4,121,1,0,0,0,6,135,1,0,0,0,8,138,1,
    0,0,0,10,146,1,0,0,0,12,151,1,0,0,0,14,153,1,0,0,0,16,156,1,0,0,0,18,
    158,1,0,0,0,20,162,1,0,0,0,22,166,1,0,0,0,24,168,1,0,0,0,26,180,1,0,0,
    0,28,223,1,0,0,0,30,228,1,0,0,0,32,230,1,0,0,0,34,236,1,0,0,0,36,243,
    1,0,0,0,38,245,1,0,0,0,40,277,1,0,0,0,42,315,1,0,0,0,44,322,1,0,0,0,46,
    338,1,0,0,0,48,340,1,0,0,0,50,371,1,0,0,0,52,374,1,0,0,0,54,377,1,0,0,
    0,56,380,1,0,0,0,58,391,1,0,0,0,60,393,1,0,0,0,62,411,1,0,0,0,64,418,
    1,0,0,0,66,420,1,0,0,0,68,422,1,0,0,0,70,448,1,0,0,0,72,468,1,0,0,0,74,
    486,1,0,0,0,76,492,1,0,0,0,78,510,1,0,0,0,80,515,1,0,0,0,82,524,1,0,0,
    0,84,539,1,0,0,0,86,87,5,1,0,0,87,88,3,2,1,0,88,89,5,0,0,1,89,1,1,0,0,
    0,90,120,3,4,2,0,91,120,3,8,4,0,92,120,3,10,5,0,93,120,3,12,6,0,94,120,
    3,14,7,0,95,120,3,16,8,0,96,120,3,18,9,0,97,120,3,20,10,0,98,120,3,22,
    11,0,99,120,3,24,12,0,100,120,3,28,14,0,101,120,3,38,19,0,102,120,3,40,
    20,0,103,120,3,42,21,0,104,120,3,44,22,0,105,120,3,48,24,0,106,120,3,
    50,25,0,107,120,3,52,26,0,108,120,3,54,27,0,109,120,3,56,28,0,110,120,
    3,58,29,0,111,120,3,60,30,0,112,120,3,68,34,0,113,120,3,70,35,0,114,120,
    3,72,36,0,115,120,3,74,37,0,116,120,3,78,39,0,117,120,3,80,40,0,118,120,
    3,82,41,0,119,90,1,0,0,0,119,91,1,0,0,0,119,92,1,0,0,0,119,93,1,0,0,0,
    119,94,1,0,0,0,119,95,1,0,0,0,119,96,1,0,0,0,119,97,1,0,0,0,119,98,1,
    0,0,0,119,99,1,0,0,0,119,100,1,0,0,0,119,101,1,0,0,0,119,102,1,0,0,0,
    119,103,1,0,0,0,119,104,1,0,0,0,119,105,1,0,0,0,119,106,1,0,0,0,119,107,
    1,0,0,0,119,108,1,0,0,0,119,109,1,0,0,0,119,110,1,0,0,0,119,111,1,0,0,
    0,119,112,1,0,0,0,119,113,1,0,0,0,119,114,1,0,0,0,119,115,1,0,0,0,119,
    116,1,0,0,0,119,117,1,0,0,0,119,118,1,0,0,0,120,3,1,0,0,0,121,133,5,2,
    0,0,122,126,5,3,0,0,123,125,3,6,3,0,124,123,1,0,0,0,125,128,1,0,0,0,126,
    124,1,0,0,0,126,127,1,0,0,0,127,134,1,0,0,0,128,126,1,0,0,0,129,130,5,
    4,0,0,130,134,3,64,32,0,131,134,5,5,0,0,132,134,5,6,0,0,133,122,1,0,0,
    0,133,129,1,0,0,0,133,131,1,0,0,0,133,132,1,0,0,0,134,5,1,0,0,0,135,136,
    7,0,0,0,136,137,3,64,32,0,137,7,1,0,0,0,138,144,5,9,0,0,139,142,5,123,
    0,0,140,141,5,10,0,0,141,143,5,123,0,0,142,140,1,0,0,0,142,143,1,0,0,
    0,143,145,1,0,0,0,144,139,1,0,0,0,144,145,1,0,0,0,145,9,1,0,0,0,146,149,
    5,11,0,0,147,148,5,12,0,0,148,150,3,64,32,0,149,147,1,0,0,0,149,150,1,
    0,0,0,150,11,1,0,0,0,151,152,5,13,0,0,152,13,1,0,0,0,153,154,5,14,0,0,
    154,155,3,64,32,0,155,15,1,0,0,0,156,157,5,5,0,0,157,17,1,0,0,0,158,160,
    5,15,0,0,159,161,3,64,32,0,160,159,1,0,0,0,160,161,1,0,0,0,161,19,1,0,
    0,0,162,164,5,16,0,0,163,165,5,17,0,0,164,163,1,0,0,0,164,165,1,0,0,0,
    165,21,1,0,0,0,166,167,5,18,0,0,167,23,1,0,0,0,168,172,5,19,0,0,169,171,
    3,26,13,0,170,169,1,0,0,0,171,174,1,0,0,0,172,170,1,0,0,0,172,173,1,0,
    0,0,173,25,1,0,0,0,174,172,1,0,0,0,175,176,7,1,0,0,176,181,3,64,32,0,
    177,178,5,24,0,0,178,181,5,123,0,0,179,181,5,25,0,0,180,175,1,0,0,0,180,
    177,1,0,0,0,180,179,1,0,0,0,181,27,1,0,0,0,182,220,5,26,0,0,183,184,5,
    27,0,0,184,186,3,64,32,0,185,187,3,30,15,0,186,185,1,0,0,0,187,188,1,
    0,0,0,188,186,1,0,0,0,188,189,1,0,0,0,189,221,1,0,0,0,190,191,5,28,0,
    0,191,195,3,64,32,0,192,194,3,32,16,0,193,192,1,0,0,0,194,197,1,0,0,0,
    195,193,1,0,0,0,195,196,1,0,0,0,196,221,1,0,0,0,197,195,1,0,0,0,198,199,
    5,29,0,0,199,200,3,64,32,0,200,201,5,30,0,0,201,205,3,64,32,0,202,204,
    3,36,18,0,203,202,1,0,0,0,204,207,1,0,0,0,205,203,1,0,0,0,205,206,1,0,
    0,0,206,221,1,0,0,0,207,205,1,0,0,0,208,209,5,31,0,0,209,210,3,64,32,
    0,210,211,5,30,0,0,211,215,3,64,32,0,212,214,3,34,17,0,213,212,1,0,0,
    0,214,217,1,0,0,0,215,213,1,0,0,0,215,216,1,0,0,0,216,221,1,0,0,0,217,
    215,1,0,0,0,218,219,5,32,0,0,219,221,5,123,0,0,220,183,1,0,0,0,220,190,
    1,0,0,0,220,198,1,0,0,0,220,208,1,0,0,0,220,218,1,0,0,0,221,224,1,0,0,
    0,222,224,5,33,0,0,223,182,1,0,0,0,223,222,1,0,0,0,224,29,1,0,0,0,225,
    226,7,2,0,0,226,229,3,64,32,0,227,229,5,126,0,0,228,225,1,0,0,0,228,227,
    1,0,0,0,229,31,1,0,0,0,230,231,7,3,0,0,231,232,3,64,32,0,232,33,1,0,0,
    0,233,237,3,32,16,0,234,237,5,41,0,0,235,237,5,42,0,0,236,233,1,0,0,0,
    236,234,1,0,0,0,236,235,1,0,0,0,237,35,1,0,0,0,238,244,3,34,17,0,239,
    240,5,43,0,0,240,244,5,123,0,0,241,244,5,44,0,0,242,244,5,45,0,0,243,
    238,1,0,0,0,243,239,1,0,0,0,243,241,1,0,0,0,243,242,1,0,0,0,244,37,1,
    0,0,0,245,275,5,46,0,0,246,247,5,47,0,0,247,248,3,64,32,0,248,249,5,48,
    0,0,249,250,3,64,32,0,250,276,1,0,0,0,251,252,5,49,0,0,252,276,3,64,32,
    0,253,254,5,50,0,0,254,276,3,64,32,0,255,256,5,31,0,0,256,257,3,64,32,
    0,257,258,5,51,0,0,258,262,3,64,32,0,259,261,3,34,17,0,260,259,1,0,0,
    0,261,264,1,0,0,0,262,260,1,0,0,0,262,263,1,0,0,0,263,276,1,0,0,0,264,
    262,1,0,0,0,265,266,5,52,0,0,266,267,3,64,32,0,267,268,5,51,0,0,268,272,
    3,64,32,0,269,271,3,36,18,0,270,269,1,0,0,0,271,274,1,0,0,0,272,270,1,
    0,0,0,272,273,1,0,0,0,273,276,1,0,0,0,274,272,1,0,0,0,275,246,1,0,0,0,
    275,251,1,0,0,0,275,253,1,0,0,0,275,255,1,0,0,0,275,265,1,0,0,0,276,39,
    1,0,0,0,277,313,5,53,0,0,278,280,3,46,23,0,279,278,1,0,0,0,280,283,1,
    0,0,0,281,279,1,0,0,0,281,282,1,0,0,0,282,314,1,0,0,0,283,281,1,0,0,0,
    284,285,5,54,0,0,285,288,3,64,32,0,286,287,5,24,0,0,287,289,5,123,0,0,
    288,286,1,0,0,0,288,289,1,0,0,0,289,314,1,0,0,0,290,291,5,50,0,0,291,
    292,3,64,32,0,292,293,5,55,0,0,293,294,5,123,0,0,294,314,1,0,0,0,295,
    296,5,56,0,0,296,314,3,64,32,0,297,298,5,57,0,0,298,314,3,64,32,0,299,
    300,5,58,0,0,300,314,3,64,32,0,301,302,5,59,0,0,302,303,3,64,32,0,303,
    304,5,48,0,0,304,305,3,64,32,0,305,314,1,0,0,0,306,307,5,60,0,0,307,308,
    3,64,32,0,308,309,5,61,0,0,309,310,5,62,0,0,310,311,5,37,0,0,311,312,
    3,64,32,0,312,314,1,0,0,0,313,281,1,0,0,0,313,284,1,0,0,0,313,290,1,0,
    0,0,313,295,1,0,0,0,313,297,1,0,0,0,313,299,1,0,0,0,313,301,1,0,0,0,313,
    306,1,0,0,0,314,41,1,0,0,0,315,319,5,63,0,0,316,318,3,46,23,0,317,316,
    1,0,0,0,318,321,1,0,0,0,319,317,1,0,0,0,319,320,1,0,0,0,320,43,1,0,0,
    0,321,319,1,0,0,0,322,323,5,64,0,0,323,328,3,64,32,0,324,327,3,46,23,
    0,325,327,5,127,0,0,326,324,1,0,0,0,326,325,1,0,0,0,327,330,1,0,0,0,328,
    326,1,0,0,0,328,329,1,0,0,0,329,45,1,0,0,0,330,328,1,0,0,0,331,332,7,
    4,0,0,332,339,5,123,0,0,333,334,5,69,0,0,334,339,7,5,0,0,335,336,7,6,
    0,0,336,339,3,64,32,0,337,339,5,74,0,0,338,331,1,0,0,0,338,333,1,0,0,
    0,338,335,1,0,0,0,338,337,1,0,0,0,339,47,1,0,0,0,340,369,5,75,0,0,341,
    342,5,47,0,0,342,370,3,64,32,0,343,370,5,76,0,0,344,345,7,7,0,0,345,350,
    3,64,32,0,346,347,7,8,0,0,347,349,3,64,32,0,348,346,1,0,0,0,349,352,1,
    0,0,0,350,348,1,0,0,0,350,351,1,0,0,0,351,370,1,0,0,0,352,350,1,0,0,0,
    353,367,5,80,0,0,354,357,5,81,0,0,355,358,3,64,32,0,356,358,5,125,0,0,
    357,355,1,0,0,0,357,356,1,0,0,0,358,359,1,0,0,0,359,357,1,0,0,0,359,360,
    1,0,0,0,360,368,1,0,0,0,361,368,5,58,0,0,362,363,5,82,0,0,363,364,3,64,
    32,0,364,365,5,83,0,0,365,366,3,64,32,0,366,368,1,0,0,0,367,354,1,0,0,
    0,367,361,1,0,0,0,367,362,1,0,0,0,367,368,1,0,0,0,368,370,1,0,0,0,369,
    341,1,0,0,0,369,343,1,0,0,0,369,344,1,0,0,0,369,353,1,0,0,0,370,49,1,
    0,0,0,371,372,5,84,0,0,372,373,3,64,32,0,373,51,1,0,0,0,374,375,5,85,
    0,0,375,376,3,64,32,0,376,53,1,0,0,0,377,378,5,86,0,0,378,379,3,64,32,
    0,379,55,1,0,0,0,380,389,5,87,0,0,381,383,5,1,0,0,382,381,1,0,0,0,382,
    383,1,0,0,0,383,385,1,0,0,0,384,386,3,64,32,0,385,384,1,0,0,0,386,387,
    1,0,0,0,387,385,1,0,0,0,387,388,1,0,0,0,388,390,1,0,0,0,389,382,1,0,0,
    0,389,390,1,0,0,0,390,57,1,0,0,0,391,392,7,9,0,0,392,59,1,0,0,0,393,409,
    5,90,0,0,394,396,5,91,0,0,395,397,3,62,31,0,396,395,1,0,0,0,397,398,1,
    0,0,0,398,396,1,0,0,0,398,399,1,0,0,0,399,410,1,0,0,0,400,401,5,92,0,
    0,401,402,5,93,0,0,402,405,3,64,32,0,403,404,5,30,0,0,404,406,3,64,32,
    0,405,403,1,0,0,0,405,406,1,0,0,0,406,410,1,0,0,0,407,408,5,32,0,0,408,
    410,3,64,32,0,409,394,1,0,0,0,409,400,1,0,0,0,409,407,1,0,0,0,410,61,
    1,0,0,0,411,412,7,10,0,0,412,413,3,64,32,0,413,63,1,0,0,0,414,419,5,124,
    0,0,415,419,5,128,0,0,416,419,5,123,0,0,417,419,3,66,33,0,418,414,1,0,
    0,0,418,415,1,0,0,0,418,416,1,0,0,0,418,417,1,0,0,0,419,65,1,0,0,0,420,
    421,7,11,0,0,421,67,1,0,0,0,422,446,5,97,0,0,423,447,5,98,0,0,424,425,
    5,99,0,0,425,426,3,64,32,0,426,427,5,23,0,0,427,432,3,64,32,0,428,429,
    5,65,0,0,429,433,5,123,0,0,430,431,5,79,0,0,431,433,3,64,32,0,432,428,
    1,0,0,0,432,430,1,0,0,0,433,447,1,0,0,0,434,435,5,100,0,0,435,436,3,64,
    32,0,436,437,5,23,0,0,437,438,3,64,32,0,438,439,5,65,0,0,439,440,5,123,
    0,0,440,441,5,112,0,0,441,444,3,64,32,0,442,443,5,24,0,0,443,445,5,123,
    0,0,444,442,1,0,0,0,444,445,1,0,0,0,445,447,1,0,0,0,446,423,1,0,0,0,446,
    424,1,0,0,0,446,434,1,0,0,0,447,69,1,0,0,0,448,449,5,26,0,0,449,466,5,
    101,0,0,450,451,3,64,32,0,451,452,5,23,0,0,452,455,3,64,32,0,453,454,
    5,20,0,0,454,456,3,64,32,0,455,453,1,0,0,0,455,456,1,0,0,0,456,467,1,
    0,0,0,457,458,5,20,0,0,458,459,3,64,32,0,459,460,5,113,0,0,460,461,3,
    64,32,0,461,462,5,114,0,0,462,463,3,64,32,0,463,464,5,23,0,0,464,465,
    3,64,32,0,465,467,1,0,0,0,466,450,1,0,0,0,466,457,1,0,0,0,467,71,1,0,
    0,0,468,469,5,26,0,0,469,470,5,102,0,0,470,471,5,123,0,0,471,472,5,30,
    0,0,472,473,3,64,32,0,473,73,1,0,0,0,474,475,5,103,0,0,475,479,3,64,32,
    0,476,478,3,76,38,0,477,476,1,0,0,0,478,481,1,0,0,0,479,477,1,0,0,0,479,
    480,1,0,0,0,480,487,1,0,0,0,481,479,1,0,0,0,482,484,7,12,0,0,483,485,
    5,123,0,0,484,483,1,0,0,0,484,485,1,0,0,0,485,487,1,0,0,0,486,474,1,0,
    0,0,486,482,1,0,0,0,487,75,1,0,0,0,488,489,7,13,0,0,489,493,3,64,32,0,
    490,491,5,66,0,0,491,493,5,123,0,0,492,488,1,0,0,0,492,490,1,0,0,0,493,
    77,1,0,0,0,494,495,5,106,0,0,495,502,3,64,32,0,496,497,5,115,0,0,497,
    501,3,64,32,0,498,499,5,66,0,0,499,501,5,123,0,0,500,496,1,0,0,0,500,
    498,1,0,0,0,501,504,1,0,0,0,502,500,1,0,0,0,502,503,1,0,0,0,503,511,1,
    0,0,0,504,502,1,0,0,0,505,508,5,107,0,0,506,507,5,66,0,0,507,509,5,123,
    0,0,508,506,1,0,0,0,508,509,1,0,0,0,509,511,1,0,0,0,510,494,1,0,0,0,510,
    505,1,0,0,0,511,79,1,0,0,0,512,516,5,108,0,0,513,514,5,109,0,0,514,516,
    5,110,0,0,515,512,1,0,0,0,515,513,1,0,0,0,516,521,1,0,0,0,517,518,7,14,
    0,0,518,520,5,123,0,0,519,517,1,0,0,0,520,523,1,0,0,0,521,519,1,0,0,0,
    521,522,1,0,0,0,522,81,1,0,0,0,523,521,1,0,0,0,524,533,5,111,0,0,525,
    529,5,3,0,0,526,528,3,84,42,0,527,526,1,0,0,0,528,531,1,0,0,0,529,527,
    1,0,0,0,529,530,1,0,0,0,530,534,1,0,0,0,531,529,1,0,0,0,532,534,5,6,0,
    0,533,525,1,0,0,0,533,532,1,0,0,0,534,83,1,0,0,0,535,536,7,15,0,0,536,
    540,5,123,0,0,537,538,7,16,0,0,538,540,3,64,32,0,539,535,1,0,0,0,539,
    537,1,0,0,0,540,85,1,0,0,0,59,119,126,133,142,144,149,160,164,172,180,
    188,195,205,215,220,223,228,236,243,262,272,275,281,288,313,319,326,328,
    338,350,357,359,367,369,382,387,389,398,405,409,418,432,444,446,455,466,
    479,484,486,492,500,502,508,510,515,521,529,533,539
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
    setState(86);
    match(TracerCommandsParser::T__0);
    setState(87);
    command();
    setState(88);
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
    setState(119);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 0, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(90);
      server();
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(91);
      attach();
      break;
    }

    case 3: {
      enterOuterAlt(_localctx, 3);
      setState(92);
      processes();
      break;
    }

    case 4: {
      enterOuterAlt(_localctx, 4);
      setState(93);
      sessions();
      break;
    }

    case 5: {
      enterOuterAlt(_localctx, 5);
      setState(94);
      use();
      break;
    }

    case 6: {
      enterOuterAlt(_localctx, 6);
      setState(95);
      status();
      break;
    }

    case 7: {
      enterOuterAlt(_localctx, 7);
      setState(96);
      detach();
      break;
    }

    case 8: {
      enterOuterAlt(_localctx, 8);
      setState(97);
      modules();
      break;
    }

    case 9: {
      enterOuterAlt(_localctx, 9);
      setState(98);
      threads();
      break;
    }

    case 10: {
      enterOuterAlt(_localctx, 10);
      setState(99);
      symbols();
      break;
    }

    case 11: {
      enterOuterAlt(_localctx, 11);
      setState(100);
      probe();
      break;
    }

    case 12: {
      enterOuterAlt(_localctx, 12);
      setState(101);
      interceptor();
      break;
    }

    case 13: {
      enterOuterAlt(_localctx, 13);
      setState(102);
      events();
      break;
    }

    case 14: {
      enterOuterAlt(_localctx, 14);
      setState(103);
      collect();
      break;
    }

    case 15: {
      enterOuterAlt(_localctx, 15);
      setState(104);
      record();
      break;
    }

    case 16: {
      enterOuterAlt(_localctx, 16);
      setState(105);
      cpp();
      break;
    }

    case 17: {
      enterOuterAlt(_localctx, 17);
      setState(106);
      shell();
      break;
    }

    case 18: {
      enterOuterAlt(_localctx, 18);
      setState(107);
      complete();
      break;
    }

    case 19: {
      enterOuterAlt(_localctx, 19);
      setState(108);
      type();
      break;
    }

    case 20: {
      enterOuterAlt(_localctx, 20);
      setState(109);
      help();
      break;
    }

    case 21: {
      enterOuterAlt(_localctx, 21);
      setState(110);
      quit();
      break;
    }

    case 22: {
      enterOuterAlt(_localctx, 22);
      setState(111);
      bulkTrace();
      break;
    }

    case 23: {
      enterOuterAlt(_localctx, 23);
      setState(112);
      memory();
      break;
    }

    case 24: {
      enterOuterAlt(_localctx, 24);
      setState(113);
      probeEntry();
      break;
    }

    case 25: {
      enterOuterAlt(_localctx, 25);
      setState(114);
      probeReload();
      break;
    }

    case 26: {
      enterOuterAlt(_localctx, 26);
      setState(115);
      targetLaunch();
      break;
    }

    case 27: {
      enterOuterAlt(_localctx, 27);
      setState(116);
      targetCall();
      break;
    }

    case 28: {
      enterOuterAlt(_localctx, 28);
      setState(117);
      localPerf();
      break;
    }

    case 29: {
      enterOuterAlt(_localctx, 29);
      setState(118);
      instructions();
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
    setState(121);
    match(TracerCommandsParser::T__1);
    setState(133);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__2: {
        setState(122);
        match(TracerCommandsParser::T__2);
        setState(126);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__6

        || _la == TracerCommandsParser::T__7) {
          setState(123);
          serverOption();
          setState(128);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__3: {
        setState(129);
        match(TracerCommandsParser::T__3);
        setState(130);
        value();
        break;
      }

      case TracerCommandsParser::T__4: {
        setState(131);
        match(TracerCommandsParser::T__4);
        break;
      }

      case TracerCommandsParser::T__5: {
        setState(132);
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
    setState(135);
    _la = _input->LA(1);
    if (!(_la == TracerCommandsParser::T__6

    || _la == TracerCommandsParser::T__7)) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(136);
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
    setState(138);
    match(TracerCommandsParser::T__8);
    setState(144);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::UINT) {
      setState(139);
      match(TracerCommandsParser::UINT);
      setState(142);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == TracerCommandsParser::T__9) {
        setState(140);
        match(TracerCommandsParser::T__9);
        setState(141);
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
    setState(146);
    match(TracerCommandsParser::T__10);
    setState(149);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::T__11) {
      setState(147);
      match(TracerCommandsParser::T__11);
      setState(148);
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
    setState(151);
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
    setState(153);
    match(TracerCommandsParser::T__13);
    setState(154);
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
    setState(156);
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
    setState(158);
    match(TracerCommandsParser::T__14);
    setState(160);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (((((_la - 2) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 2)) & 8637253178422950559) != 0) || ((((_la - 70) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 70)) & 315256371836607971) != 0)) {
      setState(159);
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
    setState(162);
    match(TracerCommandsParser::T__15);
    setState(164);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::T__16) {
      setState(163);
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
    setState(166);
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
    setState(168);
    match(TracerCommandsParser::T__18);
    setState(172);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 66064384) != 0)) {
      setState(169);
      symbolOption();
      setState(174);
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
    setState(180);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__11:
      case TracerCommandsParser::T__19:
      case TracerCommandsParser::T__20:
      case TracerCommandsParser::T__21:
      case TracerCommandsParser::T__22: {
        enterOuterAlt(_localctx, 1);
        setState(175);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 15732736) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(176);
        value();
        break;
      }

      case TracerCommandsParser::T__23: {
        enterOuterAlt(_localctx, 2);
        setState(177);
        match(TracerCommandsParser::T__23);
        setState(178);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__24: {
        enterOuterAlt(_localctx, 3);
        setState(179);
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
    setState(223);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__25: {
        enterOuterAlt(_localctx, 1);
        setState(182);
        match(TracerCommandsParser::T__25);
        setState(220);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__26: {
            setState(183);
            match(TracerCommandsParser::T__26);
            setState(184);
            value();
            setState(186);
            _errHandler->sync(this);
            _la = _input->LA(1);
            do {
              setState(185);
              prepareOption();
              setState(188);
              _errHandler->sync(this);
              _la = _input->LA(1);
            } while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 257699086336) != 0) || _la == TracerCommandsParser::METADATA_ONLY);
            break;
          }

          case TracerCommandsParser::T__27: {
            setState(190);
            match(TracerCommandsParser::T__27);
            setState(191);
            value();
            setState(195);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 1975686004736) != 0)) {
              setState(192);
              resolveOption();
              setState(197);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__28: {
            setState(198);
            match(TracerCommandsParser::T__28);
            setState(199);
            value();
            setState(200);
            match(TracerCommandsParser::T__29);
            setState(201);
            value();
            setState(205);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 70145406926848) != 0)) {
              setState(202);
              deployOption();
              setState(207);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__30: {
            setState(208);
            match(TracerCommandsParser::T__30);
            setState(209);
            value();
            setState(210);
            match(TracerCommandsParser::T__29);
            setState(211);
            value();
            setState(215);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 8572755771392) != 0)) {
              setState(212);
              validateOption();
              setState(217);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__31: {
            setState(218);
            match(TracerCommandsParser::T__31);
            setState(219);
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
        setState(222);
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
    setState(228);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__19:
      case TracerCommandsParser::T__33:
      case TracerCommandsParser::T__34:
      case TracerCommandsParser::T__35:
      case TracerCommandsParser::T__36: {
        enterOuterAlt(_localctx, 1);
        setState(225);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 257699086336) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(226);
        value();
        break;
      }

      case TracerCommandsParser::METADATA_ONLY: {
        enterOuterAlt(_localctx, 2);
        setState(227);
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
    setState(230);
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 1975686004736) != 0))) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(231);
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
    setState(236);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__19:
      case TracerCommandsParser::T__33:
      case TracerCommandsParser::T__34:
      case TracerCommandsParser::T__37:
      case TracerCommandsParser::T__38:
      case TracerCommandsParser::T__39: {
        enterOuterAlt(_localctx, 1);
        setState(233);
        resolveOption();
        break;
      }

      case TracerCommandsParser::T__40: {
        enterOuterAlt(_localctx, 2);
        setState(234);
        match(TracerCommandsParser::T__40);
        break;
      }

      case TracerCommandsParser::T__41: {
        enterOuterAlt(_localctx, 3);
        setState(235);
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
    setState(243);
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
        setState(238);
        validateOption();
        break;
      }

      case TracerCommandsParser::T__42: {
        enterOuterAlt(_localctx, 2);
        setState(239);
        match(TracerCommandsParser::T__42);
        setState(240);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__43: {
        enterOuterAlt(_localctx, 3);
        setState(241);
        match(TracerCommandsParser::T__43);
        break;
      }

      case TracerCommandsParser::T__44: {
        enterOuterAlt(_localctx, 4);
        setState(242);
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
    setState(245);
    match(TracerCommandsParser::T__45);
    setState(275);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__46: {
        setState(246);
        match(TracerCommandsParser::T__46);
        setState(247);
        value();
        setState(248);
        match(TracerCommandsParser::T__47);
        setState(249);
        value();
        break;
      }

      case TracerCommandsParser::T__48: {
        setState(251);
        match(TracerCommandsParser::T__48);
        setState(252);
        value();
        break;
      }

      case TracerCommandsParser::T__49: {
        setState(253);
        match(TracerCommandsParser::T__49);
        setState(254);
        value();
        break;
      }

      case TracerCommandsParser::T__30: {
        setState(255);
        match(TracerCommandsParser::T__30);
        setState(256);
        value();
        setState(257);
        match(TracerCommandsParser::T__50);
        setState(258);
        value();
        setState(262);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 8572755771392) != 0)) {
          setState(259);
          validateOption();
          setState(264);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__51: {
        setState(265);
        match(TracerCommandsParser::T__51);
        setState(266);
        value();
        setState(267);
        match(TracerCommandsParser::T__50);
        setState(268);
        value();
        setState(272);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 70145406926848) != 0)) {
          setState(269);
          deployOption();
          setState(274);
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
    setState(277);
    match(TracerCommandsParser::T__52);
    setState(313);
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
        setState(281);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (((((_la - 48) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 48)) & 121503745) != 0)) {
          setState(278);
          streamOption();
          setState(283);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__53: {
        setState(284);
        match(TracerCommandsParser::T__53);
        setState(285);
        value();
        setState(288);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__23) {
          setState(286);
          match(TracerCommandsParser::T__23);
          setState(287);
          match(TracerCommandsParser::UINT);
        }
        break;
      }

      case TracerCommandsParser::T__49: {
        setState(290);
        match(TracerCommandsParser::T__49);
        setState(291);
        value();
        setState(292);
        match(TracerCommandsParser::T__54);
        setState(293);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__55: {
        setState(295);
        match(TracerCommandsParser::T__55);
        setState(296);
        value();
        break;
      }

      case TracerCommandsParser::T__56: {
        setState(297);
        match(TracerCommandsParser::T__56);
        setState(298);
        value();
        break;
      }

      case TracerCommandsParser::T__57: {
        setState(299);
        match(TracerCommandsParser::T__57);
        setState(300);
        value();
        break;
      }

      case TracerCommandsParser::T__58: {
        setState(301);
        match(TracerCommandsParser::T__58);
        setState(302);
        value();
        setState(303);
        match(TracerCommandsParser::T__47);
        setState(304);
        value();
        break;
      }

      case TracerCommandsParser::T__59: {
        setState(306);
        match(TracerCommandsParser::T__59);
        setState(307);
        value();
        setState(308);
        match(TracerCommandsParser::T__60);
        setState(309);
        match(TracerCommandsParser::T__61);
        setState(310);
        match(TracerCommandsParser::T__36);
        setState(311);
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
    setState(315);
    match(TracerCommandsParser::T__62);
    setState(319);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (((((_la - 48) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 48)) & 121503745) != 0)) {
      setState(316);
      streamOption();
      setState(321);
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
    setState(322);
    match(TracerCommandsParser::T__63);
    setState(323);
    value();
    setState(328);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == TracerCommandsParser::T__47 || ((((_la - 65) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 65)) & 4611686018427388831) != 0)) {
      setState(326);
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
          setState(324);
          streamOption();
          break;
        }

        case TracerCommandsParser::SQLITE: {
          setState(325);
          match(TracerCommandsParser::SQLITE);
          break;
        }

      default:
        throw NoViableAltException(this);
      }
      setState(330);
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
    setState(338);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__64:
      case TracerCommandsParser::T__65:
      case TracerCommandsParser::T__66:
      case TracerCommandsParser::T__67: {
        enterOuterAlt(_localctx, 1);
        setState(331);
        _la = _input->LA(1);
        if (!(((((_la - 65) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 65)) & 15) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(332);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__68: {
        enterOuterAlt(_localctx, 2);
        setState(333);
        match(TracerCommandsParser::T__68);
        setState(334);
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
        setState(335);
        _la = _input->LA(1);
        if (!(((((_la - 48) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 48)) & 50331649) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(336);
        value();
        break;
      }

      case TracerCommandsParser::T__73: {
        enterOuterAlt(_localctx, 4);
        setState(337);
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
    setState(340);
    match(TracerCommandsParser::T__74);
    setState(369);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__46: {
        setState(341);
        match(TracerCommandsParser::T__46);
        setState(342);
        value();
        break;
      }

      case TracerCommandsParser::T__75: {
        setState(343);
        match(TracerCommandsParser::T__75);
        break;
      }

      case TracerCommandsParser::T__76:
      case TracerCommandsParser::T__77: {
        setState(344);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__76

        || _la == TracerCommandsParser::T__77)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(345);
        value();
        setState(350);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__36

        || _la == TracerCommandsParser::T__78) {
          setState(346);
          _la = _input->LA(1);
          if (!(_la == TracerCommandsParser::T__36

          || _la == TracerCommandsParser::T__78)) {
          _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(347);
          value();
          setState(352);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__79: {
        setState(353);
        match(TracerCommandsParser::T__79);
        setState(367);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__80: {
            setState(354);
            match(TracerCommandsParser::T__80);
            setState(357);
            _errHandler->sync(this);
            _la = _input->LA(1);
            do {
              setState(357);
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
                case TracerCommandsParser::T__110:
                case TracerCommandsParser::UINT:
                case TracerCommandsParser::STRING:
                case TracerCommandsParser::WORD: {
                  setState(355);
                  value();
                  break;
                }

                case TracerCommandsParser::SHELL: {
                  setState(356);
                  match(TracerCommandsParser::SHELL);
                  break;
                }

              default:
                throw NoViableAltException(this);
              }
              setState(359);
              _errHandler->sync(this);
              _la = _input->LA(1);
            } while (((((_la - 2) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 2)) & 8637253178422950559) != 0) || ((((_la - 70) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 70)) & 351285168855571939) != 0));
            break;
          }

          case TracerCommandsParser::T__57: {
            setState(361);
            match(TracerCommandsParser::T__57);
            break;
          }

          case TracerCommandsParser::T__81: {
            setState(362);
            match(TracerCommandsParser::T__81);
            setState(363);
            value();
            setState(364);
            match(TracerCommandsParser::T__82);
            setState(365);
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
    setState(371);
    match(TracerCommandsParser::T__83);
    setState(372);
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
    setState(374);
    match(TracerCommandsParser::T__84);
    setState(375);
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
    setState(377);
    match(TracerCommandsParser::T__85);
    setState(378);
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
    setState(380);
    match(TracerCommandsParser::T__86);
    setState(389);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (((((_la - 1) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 1)) & -1172237716863650497) != 0) || ((((_la - 70) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 70)) & 315256371836607971) != 0)) {
      setState(382);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == TracerCommandsParser::T__0) {
        setState(381);
        match(TracerCommandsParser::T__0);
      }
      setState(385);
      _errHandler->sync(this);
      _la = _input->LA(1);
      do {
        setState(384);
        value();
        setState(387);
        _errHandler->sync(this);
        _la = _input->LA(1);
      } while (((((_la - 2) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 2)) & 8637253178422950559) != 0) || ((((_la - 70) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 70)) & 315256371836607971) != 0));
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
    setState(391);
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
    setState(393);
    match(TracerCommandsParser::T__89);
    setState(409);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__90: {
        setState(394);
        match(TracerCommandsParser::T__90);
        setState(396);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(395);
          bulkTraceOption();
          setState(398);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while (_la == TracerCommandsParser::T__19

        || _la == TracerCommandsParser::T__33 || ((((_la - 93) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 93)) & 15) != 0));
        break;
      }

      case TracerCommandsParser::T__91: {
        setState(400);
        match(TracerCommandsParser::T__91);
        setState(401);
        match(TracerCommandsParser::T__92);
        setState(402);
        value();
        setState(405);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__29) {
          setState(403);
          match(TracerCommandsParser::T__29);
          setState(404);
          value();
        }
        break;
      }

      case TracerCommandsParser::T__31: {
        setState(407);
        match(TracerCommandsParser::T__31);
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
    setState(411);
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
    setState(418);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::STRING: {
        enterOuterAlt(_localctx, 1);
        setState(414);
        match(TracerCommandsParser::STRING);
        break;
      }

      case TracerCommandsParser::WORD: {
        enterOuterAlt(_localctx, 2);
        setState(415);
        match(TracerCommandsParser::WORD);
        break;
      }

      case TracerCommandsParser::UINT: {
        enterOuterAlt(_localctx, 3);
        setState(416);
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
      case TracerCommandsParser::T__109:
      case TracerCommandsParser::T__110: {
        enterOuterAlt(_localctx, 4);
        setState(417);
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
    setState(420);
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & -2344475433727300996) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 64)) & 281466923088065) != 0))) {
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
    setState(422);
    match(TracerCommandsParser::T__96);
    setState(446);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__97: {
        setState(423);
        match(TracerCommandsParser::T__97);
        break;
      }

      case TracerCommandsParser::T__98: {
        setState(424);
        match(TracerCommandsParser::T__98);
        setState(425);
        value();
        setState(426);
        match(TracerCommandsParser::T__22);
        setState(427);
        value();
        setState(432);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__64: {
            setState(428);
            match(TracerCommandsParser::T__64);
            setState(429);
            match(TracerCommandsParser::UINT);
            break;
          }

          case TracerCommandsParser::T__78: {
            setState(430);
            match(TracerCommandsParser::T__78);
            setState(431);
            value();
            break;
          }

        default:
          throw NoViableAltException(this);
        }
        break;
      }

      case TracerCommandsParser::T__99: {
        setState(434);
        match(TracerCommandsParser::T__99);
        setState(435);
        value();
        setState(436);
        match(TracerCommandsParser::T__22);
        setState(437);
        value();
        setState(438);
        match(TracerCommandsParser::T__64);
        setState(439);
        match(TracerCommandsParser::UINT);
        setState(440);
        match(TracerCommandsParser::T__111);
        setState(441);
        value();
        setState(444);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__23) {
          setState(442);
          match(TracerCommandsParser::T__23);
          setState(443);
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
    setState(448);
    match(TracerCommandsParser::T__25);
    setState(449);
    match(TracerCommandsParser::T__100);
    setState(466);
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
      case TracerCommandsParser::T__110:
      case TracerCommandsParser::UINT:
      case TracerCommandsParser::STRING:
      case TracerCommandsParser::WORD: {
        setState(450);
        value();
        setState(451);
        match(TracerCommandsParser::T__22);
        setState(452);
        value();
        setState(455);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__19) {
          setState(453);
          match(TracerCommandsParser::T__19);
          setState(454);
          value();
        }
        break;
      }

      case TracerCommandsParser::T__19: {
        setState(457);
        match(TracerCommandsParser::T__19);
        setState(458);
        value();
        setState(459);
        match(TracerCommandsParser::T__112);
        setState(460);
        value();
        setState(461);
        match(TracerCommandsParser::T__113);
        setState(462);
        value();
        setState(463);
        match(TracerCommandsParser::T__22);
        setState(464);
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
    setState(468);
    match(TracerCommandsParser::T__25);
    setState(469);
    match(TracerCommandsParser::T__101);
    setState(470);
    match(TracerCommandsParser::UINT);
    setState(471);
    match(TracerCommandsParser::T__29);
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
    setState(486);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__102: {
        enterOuterAlt(_localctx, 1);
        setState(474);
        match(TracerCommandsParser::T__102);
        setState(475);
        value();
        setState(479);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (((((_la - 66) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 66)) & 35465847065542657) != 0)) {
          setState(476);
          launchOption();
          setState(481);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__103:
      case TracerCommandsParser::T__104: {
        enterOuterAlt(_localctx, 2);
        setState(482);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__103

        || _la == TracerCommandsParser::T__104)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(484);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::UINT) {
          setState(483);
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
    setState(492);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__114:
      case TracerCommandsParser::T__115:
      case TracerCommandsParser::T__116:
      case TracerCommandsParser::T__117:
      case TracerCommandsParser::T__118:
      case TracerCommandsParser::T__119: {
        enterOuterAlt(_localctx, 1);
        setState(488);
        _la = _input->LA(1);
        if (!(((((_la - 115) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 115)) & 63) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(489);
        value();
        break;
      }

      case TracerCommandsParser::T__65: {
        enterOuterAlt(_localctx, 2);
        setState(490);
        match(TracerCommandsParser::T__65);
        setState(491);
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
    setState(510);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__105: {
        enterOuterAlt(_localctx, 1);
        setState(494);
        match(TracerCommandsParser::T__105);
        setState(495);
        value();
        setState(502);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__65

        || _la == TracerCommandsParser::T__114) {
          setState(500);
          _errHandler->sync(this);
          switch (_input->LA(1)) {
            case TracerCommandsParser::T__114: {
              setState(496);
              match(TracerCommandsParser::T__114);
              setState(497);
              value();
              break;
            }

            case TracerCommandsParser::T__65: {
              setState(498);
              match(TracerCommandsParser::T__65);
              setState(499);
              match(TracerCommandsParser::UINT);
              break;
            }

          default:
            throw NoViableAltException(this);
          }
          setState(504);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__106: {
        enterOuterAlt(_localctx, 2);
        setState(505);
        match(TracerCommandsParser::T__106);
        setState(508);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__65) {
          setState(506);
          match(TracerCommandsParser::T__65);
          setState(507);
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
    setState(515);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__107: {
        setState(512);
        match(TracerCommandsParser::T__107);
        break;
      }

      case TracerCommandsParser::T__108: {
        setState(513);
        match(TracerCommandsParser::T__108);
        setState(514);
        match(TracerCommandsParser::T__109);
        break;
      }

    default:
      throw NoViableAltException(this);
    }
    setState(521);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == TracerCommandsParser::T__64

    || _la == TracerCommandsParser::T__65) {
      setState(517);
      _la = _input->LA(1);
      if (!(_la == TracerCommandsParser::T__64

      || _la == TracerCommandsParser::T__65)) {
      _errHandler->recoverInline(this);
      }
      else {
        _errHandler->reportMatch(this);
        consume();
      }
      setState(518);
      match(TracerCommandsParser::UINT);
      setState(523);
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
    setState(524);
    match(TracerCommandsParser::T__110);
    setState(533);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__2: {
        setState(525);
        match(TracerCommandsParser::T__2);
        setState(529);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__21

        || _la == TracerCommandsParser::T__22 || ((((_la - 65) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 65)) & 216172782113783811) != 0)) {
          setState(526);
          instructionOption();
          setState(531);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__5: {
        setState(532);
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
    setState(539);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__64:
      case TracerCommandsParser::T__65:
      case TracerCommandsParser::T__120:
      case TracerCommandsParser::T__121: {
        enterOuterAlt(_localctx, 1);
        setState(535);
        _la = _input->LA(1);
        if (!(((((_la - 65) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 65)) & 216172782113783811) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(536);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__21:
      case TracerCommandsParser::T__22: {
        enterOuterAlt(_localctx, 2);
        setState(537);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__21

        || _la == TracerCommandsParser::T__22)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(538);
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

void TracerCommandsParser::initialize() {
#if ANTLR4_USE_THREAD_LOCAL_CACHE
  tracercommandsParserInitialize();
#else
  ::antlr4::internal::call_once(tracercommandsParserOnceFlag, tracercommandsParserInitialize);
#endif
}
