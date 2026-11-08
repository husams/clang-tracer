
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
      "'cpu'", "'instructions'", "'watchpoint'", "'--pattern'", "'--elf-address'",
      "'--abi'", "'--arg'", "'--cwd'", "'--env'", "'--stdin'", "'--stdout'",
      "'--stderr'", "'--tid'", "'--size'", "'--memory-size'", "'--registers'",
      "'--callback-library'", "'--memory-address'", "'--access'", "", "",
      "", "'--metadata-only'", "'--sqlite'"
    },
    std::vector<std::string>{
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
      "", "", "", "", "", "", "", "", "", "", "UINT", "STRING", "SHELL",
      "METADATA_ONLY", "SQLITE", "WORD", "WS"
    }
  );
  static const int32_t serializedATNSegment[] = {
    4,1,135,558,2,0,7,0,2,1,7,1,2,2,7,2,2,3,7,3,2,4,7,4,2,5,7,5,2,6,7,6,2,
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
    8,9,1,10,1,10,3,10,168,8,10,1,11,1,11,1,12,1,12,5,12,174,8,12,10,12,12,
    12,177,9,12,1,13,1,13,1,13,1,13,1,13,3,13,184,8,13,1,14,1,14,1,14,1,14,
    4,14,190,8,14,11,14,12,14,191,1,14,1,14,1,14,5,14,197,8,14,10,14,12,14,
    200,9,14,1,14,1,14,1,14,1,14,1,14,5,14,207,8,14,10,14,12,14,210,9,14,
    1,14,1,14,1,14,1,14,1,14,5,14,217,8,14,10,14,12,14,220,9,14,1,14,1,14,
    3,14,224,8,14,1,14,3,14,227,8,14,1,15,1,15,1,15,3,15,232,8,15,1,16,1,
    16,1,16,1,17,1,17,1,17,3,17,240,8,17,1,18,1,18,1,18,1,18,1,18,3,18,247,
    8,18,1,19,1,19,1,19,1,19,1,19,1,19,1,19,1,19,1,19,1,19,1,19,1,19,1,19,
    1,19,1,19,5,19,264,8,19,10,19,12,19,267,9,19,1,19,1,19,1,19,1,19,1,19,
    5,19,274,8,19,10,19,12,19,277,9,19,3,19,279,8,19,1,20,1,20,5,20,283,8,
    20,10,20,12,20,286,9,20,1,20,1,20,1,20,1,20,3,20,292,8,20,1,20,1,20,1,
    20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,20,1,
    20,1,20,1,20,1,20,1,20,1,20,1,20,3,20,317,8,20,1,21,1,21,5,21,321,8,21,
    10,21,12,21,324,9,21,1,22,1,22,1,22,1,22,5,22,330,8,22,10,22,12,22,333,
    9,22,1,23,1,23,1,23,1,23,1,23,1,23,1,23,3,23,342,8,23,1,24,1,24,1,24,
    1,24,1,24,1,24,1,24,1,24,5,24,352,8,24,10,24,12,24,355,9,24,1,24,1,24,
    1,24,1,24,4,24,361,8,24,11,24,12,24,362,1,24,1,24,1,24,1,24,1,24,1,24,
    3,24,371,8,24,3,24,373,8,24,1,25,1,25,1,25,1,26,1,26,1,26,1,27,1,27,1,
    27,1,28,1,28,3,28,386,8,28,1,28,4,28,389,8,28,11,28,12,28,390,3,28,393,
    8,28,1,29,1,29,1,30,1,30,1,30,4,30,400,8,30,11,30,12,30,401,1,30,1,30,
    1,30,1,30,1,30,3,30,409,8,30,1,30,1,30,3,30,413,8,30,1,31,1,31,1,31,1,
    32,1,32,1,32,1,32,3,32,422,8,32,1,33,1,33,1,34,1,34,1,34,1,34,1,34,1,
    34,1,34,1,34,1,34,1,34,3,34,436,8,34,1,34,1,34,1,34,1,34,1,34,1,34,1,
    34,1,34,1,34,1,34,3,34,448,8,34,3,34,450,8,34,1,35,1,35,1,35,1,35,1,35,
    1,35,1,35,3,35,459,8,35,1,35,1,35,1,35,1,35,1,35,1,35,1,35,1,35,1,35,
    3,35,470,8,35,1,36,1,36,1,36,1,36,1,36,1,36,1,37,1,37,1,37,5,37,481,8,
    37,10,37,12,37,484,9,37,1,37,1,37,3,37,488,8,37,3,37,490,8,37,1,38,1,
    38,1,38,1,38,3,38,496,8,38,1,39,1,39,1,39,1,39,1,39,1,39,5,39,504,8,39,
    10,39,12,39,507,9,39,1,39,1,39,1,39,3,39,512,8,39,3,39,514,8,39,1,40,
    1,40,1,40,3,40,519,8,40,1,40,1,40,5,40,523,8,40,10,40,12,40,526,9,40,
    1,41,1,41,1,41,5,41,531,8,41,10,41,12,41,534,9,41,1,41,3,41,537,8,41,
    1,42,1,42,1,42,1,42,1,42,1,42,1,42,3,42,546,8,42,1,43,1,43,1,43,1,43,
    1,43,5,43,553,8,43,10,43,12,43,556,9,43,1,43,0,0,44,0,2,4,6,8,10,12,14,
    16,18,20,22,24,26,28,30,32,34,36,38,40,42,44,46,48,50,52,54,56,58,60,
    62,64,66,68,70,72,74,76,78,80,82,84,86,0,18,1,0,7,8,2,0,12,12,20,23,2,
    0,20,20,34,37,3,0,20,20,34,35,38,40,1,0,65,68,1,0,70,71,2,0,48,48,72,
    73,1,0,77,78,2,0,37,37,79,79,1,0,88,89,3,0,20,20,34,34,93,96,17,0,2,6,
    9,9,11,11,13,16,18,19,26,29,31,33,46,47,49,50,52,54,56,60,62,64,70,71,
    75,78,80,82,84,92,97,112,1,0,104,105,1,0,116,121,1,0,65,66,2,0,65,66,
    122,123,2,0,22,23,126,127,2,0,22,23,128,128,632,0,88,1,0,0,0,2,122,1,
    0,0,0,4,124,1,0,0,0,6,138,1,0,0,0,8,141,1,0,0,0,10,149,1,0,0,0,12,154,
    1,0,0,0,14,156,1,0,0,0,16,159,1,0,0,0,18,161,1,0,0,0,20,165,1,0,0,0,22,
    169,1,0,0,0,24,171,1,0,0,0,26,183,1,0,0,0,28,226,1,0,0,0,30,231,1,0,0,
    0,32,233,1,0,0,0,34,239,1,0,0,0,36,246,1,0,0,0,38,248,1,0,0,0,40,280,
    1,0,0,0,42,318,1,0,0,0,44,325,1,0,0,0,46,341,1,0,0,0,48,343,1,0,0,0,50,
    374,1,0,0,0,52,377,1,0,0,0,54,380,1,0,0,0,56,383,1,0,0,0,58,394,1,0,0,
    0,60,396,1,0,0,0,62,414,1,0,0,0,64,421,1,0,0,0,66,423,1,0,0,0,68,425,
    1,0,0,0,70,451,1,0,0,0,72,471,1,0,0,0,74,489,1,0,0,0,76,495,1,0,0,0,78,
    513,1,0,0,0,80,518,1,0,0,0,82,527,1,0,0,0,84,545,1,0,0,0,86,547,1,0,0,
    0,88,89,5,1,0,0,89,90,3,2,1,0,90,91,5,0,0,1,91,1,1,0,0,0,92,123,3,4,2,
    0,93,123,3,8,4,0,94,123,3,10,5,0,95,123,3,12,6,0,96,123,3,14,7,0,97,123,
    3,16,8,0,98,123,3,18,9,0,99,123,3,20,10,0,100,123,3,22,11,0,101,123,3,
    24,12,0,102,123,3,28,14,0,103,123,3,38,19,0,104,123,3,40,20,0,105,123,
    3,42,21,0,106,123,3,44,22,0,107,123,3,48,24,0,108,123,3,50,25,0,109,123,
    3,52,26,0,110,123,3,54,27,0,111,123,3,56,28,0,112,123,3,58,29,0,113,123,
    3,60,30,0,114,123,3,68,34,0,115,123,3,70,35,0,116,123,3,72,36,0,117,123,
    3,74,37,0,118,123,3,78,39,0,119,123,3,80,40,0,120,123,3,82,41,0,121,123,
    3,86,43,0,122,92,1,0,0,0,122,93,1,0,0,0,122,94,1,0,0,0,122,95,1,0,0,0,
    122,96,1,0,0,0,122,97,1,0,0,0,122,98,1,0,0,0,122,99,1,0,0,0,122,100,1,
    0,0,0,122,101,1,0,0,0,122,102,1,0,0,0,122,103,1,0,0,0,122,104,1,0,0,0,
    122,105,1,0,0,0,122,106,1,0,0,0,122,107,1,0,0,0,122,108,1,0,0,0,122,109,
    1,0,0,0,122,110,1,0,0,0,122,111,1,0,0,0,122,112,1,0,0,0,122,113,1,0,0,
    0,122,114,1,0,0,0,122,115,1,0,0,0,122,116,1,0,0,0,122,117,1,0,0,0,122,
    118,1,0,0,0,122,119,1,0,0,0,122,120,1,0,0,0,122,121,1,0,0,0,123,3,1,0,
    0,0,124,136,5,2,0,0,125,129,5,3,0,0,126,128,3,6,3,0,127,126,1,0,0,0,128,
    131,1,0,0,0,129,127,1,0,0,0,129,130,1,0,0,0,130,137,1,0,0,0,131,129,1,
    0,0,0,132,133,5,4,0,0,133,137,3,64,32,0,134,137,5,5,0,0,135,137,5,6,0,
    0,136,125,1,0,0,0,136,132,1,0,0,0,136,134,1,0,0,0,136,135,1,0,0,0,137,
    5,1,0,0,0,138,139,7,0,0,0,139,140,3,64,32,0,140,7,1,0,0,0,141,147,5,9,
    0,0,142,145,5,129,0,0,143,144,5,10,0,0,144,146,5,129,0,0,145,143,1,0,
    0,0,145,146,1,0,0,0,146,148,1,0,0,0,147,142,1,0,0,0,147,148,1,0,0,0,148,
    9,1,0,0,0,149,152,5,11,0,0,150,151,5,12,0,0,151,153,3,64,32,0,152,150,
    1,0,0,0,152,153,1,0,0,0,153,11,1,0,0,0,154,155,5,13,0,0,155,13,1,0,0,
    0,156,157,5,14,0,0,157,158,3,64,32,0,158,15,1,0,0,0,159,160,5,5,0,0,160,
    17,1,0,0,0,161,163,5,15,0,0,162,164,3,64,32,0,163,162,1,0,0,0,163,164,
    1,0,0,0,164,19,1,0,0,0,165,167,5,16,0,0,166,168,5,17,0,0,167,166,1,0,
    0,0,167,168,1,0,0,0,168,21,1,0,0,0,169,170,5,18,0,0,170,23,1,0,0,0,171,
    175,5,19,0,0,172,174,3,26,13,0,173,172,1,0,0,0,174,177,1,0,0,0,175,173,
    1,0,0,0,175,176,1,0,0,0,176,25,1,0,0,0,177,175,1,0,0,0,178,179,7,1,0,
    0,179,184,3,64,32,0,180,181,5,24,0,0,181,184,5,129,0,0,182,184,5,25,0,
    0,183,178,1,0,0,0,183,180,1,0,0,0,183,182,1,0,0,0,184,27,1,0,0,0,185,
    223,5,26,0,0,186,187,5,27,0,0,187,189,3,64,32,0,188,190,3,30,15,0,189,
    188,1,0,0,0,190,191,1,0,0,0,191,189,1,0,0,0,191,192,1,0,0,0,192,224,1,
    0,0,0,193,194,5,28,0,0,194,198,3,64,32,0,195,197,3,32,16,0,196,195,1,
    0,0,0,197,200,1,0,0,0,198,196,1,0,0,0,198,199,1,0,0,0,199,224,1,0,0,0,
    200,198,1,0,0,0,201,202,5,29,0,0,202,203,3,64,32,0,203,204,5,30,0,0,204,
    208,3,64,32,0,205,207,3,36,18,0,206,205,1,0,0,0,207,210,1,0,0,0,208,206,
    1,0,0,0,208,209,1,0,0,0,209,224,1,0,0,0,210,208,1,0,0,0,211,212,5,31,
    0,0,212,213,3,64,32,0,213,214,5,30,0,0,214,218,3,64,32,0,215,217,3,34,
    17,0,216,215,1,0,0,0,217,220,1,0,0,0,218,216,1,0,0,0,218,219,1,0,0,0,
    219,224,1,0,0,0,220,218,1,0,0,0,221,222,5,32,0,0,222,224,5,129,0,0,223,
    186,1,0,0,0,223,193,1,0,0,0,223,201,1,0,0,0,223,211,1,0,0,0,223,221,1,
    0,0,0,224,227,1,0,0,0,225,227,5,33,0,0,226,185,1,0,0,0,226,225,1,0,0,
    0,227,29,1,0,0,0,228,229,7,2,0,0,229,232,3,64,32,0,230,232,5,132,0,0,
    231,228,1,0,0,0,231,230,1,0,0,0,232,31,1,0,0,0,233,234,7,3,0,0,234,235,
    3,64,32,0,235,33,1,0,0,0,236,240,3,32,16,0,237,240,5,41,0,0,238,240,5,
    42,0,0,239,236,1,0,0,0,239,237,1,0,0,0,239,238,1,0,0,0,240,35,1,0,0,0,
    241,247,3,34,17,0,242,243,5,43,0,0,243,247,5,129,0,0,244,247,5,44,0,0,
    245,247,5,45,0,0,246,241,1,0,0,0,246,242,1,0,0,0,246,244,1,0,0,0,246,
    245,1,0,0,0,247,37,1,0,0,0,248,278,5,46,0,0,249,250,5,47,0,0,250,251,
    3,64,32,0,251,252,5,48,0,0,252,253,3,64,32,0,253,279,1,0,0,0,254,255,
    5,49,0,0,255,279,3,64,32,0,256,257,5,50,0,0,257,279,3,64,32,0,258,259,
    5,31,0,0,259,260,3,64,32,0,260,261,5,51,0,0,261,265,3,64,32,0,262,264,
    3,34,17,0,263,262,1,0,0,0,264,267,1,0,0,0,265,263,1,0,0,0,265,266,1,0,
    0,0,266,279,1,0,0,0,267,265,1,0,0,0,268,269,5,52,0,0,269,270,3,64,32,
    0,270,271,5,51,0,0,271,275,3,64,32,0,272,274,3,36,18,0,273,272,1,0,0,
    0,274,277,1,0,0,0,275,273,1,0,0,0,275,276,1,0,0,0,276,279,1,0,0,0,277,
    275,1,0,0,0,278,249,1,0,0,0,278,254,1,0,0,0,278,256,1,0,0,0,278,258,1,
    0,0,0,278,268,1,0,0,0,279,39,1,0,0,0,280,316,5,53,0,0,281,283,3,46,23,
    0,282,281,1,0,0,0,283,286,1,0,0,0,284,282,1,0,0,0,284,285,1,0,0,0,285,
    317,1,0,0,0,286,284,1,0,0,0,287,288,5,54,0,0,288,291,3,64,32,0,289,290,
    5,24,0,0,290,292,5,129,0,0,291,289,1,0,0,0,291,292,1,0,0,0,292,317,1,
    0,0,0,293,294,5,50,0,0,294,295,3,64,32,0,295,296,5,55,0,0,296,297,5,129,
    0,0,297,317,1,0,0,0,298,299,5,56,0,0,299,317,3,64,32,0,300,301,5,57,0,
    0,301,317,3,64,32,0,302,303,5,58,0,0,303,317,3,64,32,0,304,305,5,59,0,
    0,305,306,3,64,32,0,306,307,5,48,0,0,307,308,3,64,32,0,308,317,1,0,0,
    0,309,310,5,60,0,0,310,311,3,64,32,0,311,312,5,61,0,0,312,313,5,62,0,
    0,313,314,5,37,0,0,314,315,3,64,32,0,315,317,1,0,0,0,316,284,1,0,0,0,
    316,287,1,0,0,0,316,293,1,0,0,0,316,298,1,0,0,0,316,300,1,0,0,0,316,302,
    1,0,0,0,316,304,1,0,0,0,316,309,1,0,0,0,317,41,1,0,0,0,318,322,5,63,0,
    0,319,321,3,46,23,0,320,319,1,0,0,0,321,324,1,0,0,0,322,320,1,0,0,0,322,
    323,1,0,0,0,323,43,1,0,0,0,324,322,1,0,0,0,325,326,5,64,0,0,326,331,3,
    64,32,0,327,330,3,46,23,0,328,330,5,133,0,0,329,327,1,0,0,0,329,328,1,
    0,0,0,330,333,1,0,0,0,331,329,1,0,0,0,331,332,1,0,0,0,332,45,1,0,0,0,
    333,331,1,0,0,0,334,335,7,4,0,0,335,342,5,129,0,0,336,337,5,69,0,0,337,
    342,7,5,0,0,338,339,7,6,0,0,339,342,3,64,32,0,340,342,5,74,0,0,341,334,
    1,0,0,0,341,336,1,0,0,0,341,338,1,0,0,0,341,340,1,0,0,0,342,47,1,0,0,
    0,343,372,5,75,0,0,344,345,5,47,0,0,345,373,3,64,32,0,346,373,5,76,0,
    0,347,348,7,7,0,0,348,353,3,64,32,0,349,350,7,8,0,0,350,352,3,64,32,0,
    351,349,1,0,0,0,352,355,1,0,0,0,353,351,1,0,0,0,353,354,1,0,0,0,354,373,
    1,0,0,0,355,353,1,0,0,0,356,370,5,80,0,0,357,360,5,81,0,0,358,361,3,64,
    32,0,359,361,5,131,0,0,360,358,1,0,0,0,360,359,1,0,0,0,361,362,1,0,0,
    0,362,360,1,0,0,0,362,363,1,0,0,0,363,371,1,0,0,0,364,371,5,58,0,0,365,
    366,5,82,0,0,366,367,3,64,32,0,367,368,5,83,0,0,368,369,3,64,32,0,369,
    371,1,0,0,0,370,357,1,0,0,0,370,364,1,0,0,0,370,365,1,0,0,0,370,371,1,
    0,0,0,371,373,1,0,0,0,372,344,1,0,0,0,372,346,1,0,0,0,372,347,1,0,0,0,
    372,356,1,0,0,0,373,49,1,0,0,0,374,375,5,84,0,0,375,376,3,64,32,0,376,
    51,1,0,0,0,377,378,5,85,0,0,378,379,3,64,32,0,379,53,1,0,0,0,380,381,
    5,86,0,0,381,382,3,64,32,0,382,55,1,0,0,0,383,392,5,87,0,0,384,386,5,
    1,0,0,385,384,1,0,0,0,385,386,1,0,0,0,386,388,1,0,0,0,387,389,3,64,32,
    0,388,387,1,0,0,0,389,390,1,0,0,0,390,388,1,0,0,0,390,391,1,0,0,0,391,
    393,1,0,0,0,392,385,1,0,0,0,392,393,1,0,0,0,393,57,1,0,0,0,394,395,7,
    9,0,0,395,59,1,0,0,0,396,412,5,90,0,0,397,399,5,91,0,0,398,400,3,62,31,
    0,399,398,1,0,0,0,400,401,1,0,0,0,401,399,1,0,0,0,401,402,1,0,0,0,402,
    413,1,0,0,0,403,404,5,92,0,0,404,405,5,93,0,0,405,408,3,64,32,0,406,407,
    5,30,0,0,407,409,3,64,32,0,408,406,1,0,0,0,408,409,1,0,0,0,409,413,1,
    0,0,0,410,411,5,32,0,0,411,413,3,64,32,0,412,397,1,0,0,0,412,403,1,0,
    0,0,412,410,1,0,0,0,413,61,1,0,0,0,414,415,7,10,0,0,415,416,3,64,32,0,
    416,63,1,0,0,0,417,422,5,130,0,0,418,422,5,134,0,0,419,422,5,129,0,0,
    420,422,3,66,33,0,421,417,1,0,0,0,421,418,1,0,0,0,421,419,1,0,0,0,421,
    420,1,0,0,0,422,65,1,0,0,0,423,424,7,11,0,0,424,67,1,0,0,0,425,449,5,
    97,0,0,426,450,5,98,0,0,427,428,5,99,0,0,428,429,3,64,32,0,429,430,5,
    23,0,0,430,435,3,64,32,0,431,432,5,65,0,0,432,436,5,129,0,0,433,434,5,
    79,0,0,434,436,3,64,32,0,435,431,1,0,0,0,435,433,1,0,0,0,436,450,1,0,
    0,0,437,438,5,100,0,0,438,439,3,64,32,0,439,440,5,23,0,0,440,441,3,64,
    32,0,441,442,5,65,0,0,442,443,5,129,0,0,443,444,5,113,0,0,444,447,3,64,
    32,0,445,446,5,24,0,0,446,448,5,129,0,0,447,445,1,0,0,0,447,448,1,0,0,
    0,448,450,1,0,0,0,449,426,1,0,0,0,449,427,1,0,0,0,449,437,1,0,0,0,450,
    69,1,0,0,0,451,452,5,26,0,0,452,469,5,101,0,0,453,454,3,64,32,0,454,455,
    5,23,0,0,455,458,3,64,32,0,456,457,5,20,0,0,457,459,3,64,32,0,458,456,
    1,0,0,0,458,459,1,0,0,0,459,470,1,0,0,0,460,461,5,20,0,0,461,462,3,64,
    32,0,462,463,5,114,0,0,463,464,3,64,32,0,464,465,5,115,0,0,465,466,3,
    64,32,0,466,467,5,23,0,0,467,468,3,64,32,0,468,470,1,0,0,0,469,453,1,
    0,0,0,469,460,1,0,0,0,470,71,1,0,0,0,471,472,5,26,0,0,472,473,5,102,0,
    0,473,474,5,129,0,0,474,475,5,30,0,0,475,476,3,64,32,0,476,73,1,0,0,0,
    477,478,5,103,0,0,478,482,3,64,32,0,479,481,3,76,38,0,480,479,1,0,0,0,
    481,484,1,0,0,0,482,480,1,0,0,0,482,483,1,0,0,0,483,490,1,0,0,0,484,482,
    1,0,0,0,485,487,7,12,0,0,486,488,5,129,0,0,487,486,1,0,0,0,487,488,1,
    0,0,0,488,490,1,0,0,0,489,477,1,0,0,0,489,485,1,0,0,0,490,75,1,0,0,0,
    491,492,7,13,0,0,492,496,3,64,32,0,493,494,5,66,0,0,494,496,5,129,0,0,
    495,491,1,0,0,0,495,493,1,0,0,0,496,77,1,0,0,0,497,498,5,106,0,0,498,
    505,3,64,32,0,499,500,5,116,0,0,500,504,3,64,32,0,501,502,5,66,0,0,502,
    504,5,129,0,0,503,499,1,0,0,0,503,501,1,0,0,0,504,507,1,0,0,0,505,503,
    1,0,0,0,505,506,1,0,0,0,506,514,1,0,0,0,507,505,1,0,0,0,508,511,5,107,
    0,0,509,510,5,66,0,0,510,512,5,129,0,0,511,509,1,0,0,0,511,512,1,0,0,
    0,512,514,1,0,0,0,513,497,1,0,0,0,513,508,1,0,0,0,514,79,1,0,0,0,515,
    519,5,108,0,0,516,517,5,109,0,0,517,519,5,110,0,0,518,515,1,0,0,0,518,
    516,1,0,0,0,519,524,1,0,0,0,520,521,7,14,0,0,521,523,5,129,0,0,522,520,
    1,0,0,0,523,526,1,0,0,0,524,522,1,0,0,0,524,525,1,0,0,0,525,81,1,0,0,
    0,526,524,1,0,0,0,527,536,5,111,0,0,528,532,5,3,0,0,529,531,3,84,42,0,
    530,529,1,0,0,0,531,534,1,0,0,0,532,530,1,0,0,0,532,533,1,0,0,0,533,537,
    1,0,0,0,534,532,1,0,0,0,535,537,5,6,0,0,536,528,1,0,0,0,536,535,1,0,0,
    0,537,83,1,0,0,0,538,539,7,15,0,0,539,546,5,129,0,0,540,541,5,124,0,0,
    541,546,5,129,0,0,542,546,5,125,0,0,543,544,7,16,0,0,544,546,3,64,32,
    0,545,538,1,0,0,0,545,540,1,0,0,0,545,542,1,0,0,0,545,543,1,0,0,0,546,
    85,1,0,0,0,547,554,5,112,0,0,548,549,7,15,0,0,549,553,5,129,0,0,550,551,
    7,17,0,0,551,553,3,64,32,0,552,548,1,0,0,0,552,550,1,0,0,0,553,556,1,
    0,0,0,554,552,1,0,0,0,554,555,1,0,0,0,555,87,1,0,0,0,556,554,1,0,0,0,
    61,122,129,136,145,147,152,163,167,175,183,191,198,208,218,223,226,231,
    239,246,265,275,278,284,291,316,322,329,331,341,353,360,362,370,372,385,
    390,392,401,408,412,421,435,447,449,458,469,482,487,489,495,503,505,511,
    513,518,524,532,536,545,552,554
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
      ((1ULL << _la) & -2344475433727300996) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 64)) & 562941899798721) != 0) || ((((_la - 129) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 129)) & 35) != 0)) {
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
    setState(167);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::T__16) {
      setState(166);
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
    setState(169);
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
    setState(171);
    match(TracerCommandsParser::T__18);
    setState(175);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 66064384) != 0)) {
      setState(172);
      symbolOption();
      setState(177);
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
    setState(183);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__11:
      case TracerCommandsParser::T__19:
      case TracerCommandsParser::T__20:
      case TracerCommandsParser::T__21:
      case TracerCommandsParser::T__22: {
        enterOuterAlt(_localctx, 1);
        setState(178);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 15732736) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(179);
        value();
        break;
      }

      case TracerCommandsParser::T__23: {
        enterOuterAlt(_localctx, 2);
        setState(180);
        match(TracerCommandsParser::T__23);
        setState(181);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__24: {
        enterOuterAlt(_localctx, 3);
        setState(182);
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
    setState(226);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__25: {
        enterOuterAlt(_localctx, 1);
        setState(185);
        match(TracerCommandsParser::T__25);
        setState(223);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__26: {
            setState(186);
            match(TracerCommandsParser::T__26);
            setState(187);
            value();
            setState(189);
            _errHandler->sync(this);
            _la = _input->LA(1);
            do {
              setState(188);
              prepareOption();
              setState(191);
              _errHandler->sync(this);
              _la = _input->LA(1);
            } while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 257699086336) != 0) || _la == TracerCommandsParser::METADATA_ONLY);
            break;
          }

          case TracerCommandsParser::T__27: {
            setState(193);
            match(TracerCommandsParser::T__27);
            setState(194);
            value();
            setState(198);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 1975686004736) != 0)) {
              setState(195);
              resolveOption();
              setState(200);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__28: {
            setState(201);
            match(TracerCommandsParser::T__28);
            setState(202);
            value();
            setState(203);
            match(TracerCommandsParser::T__29);
            setState(204);
            value();
            setState(208);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 70145406926848) != 0)) {
              setState(205);
              deployOption();
              setState(210);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__30: {
            setState(211);
            match(TracerCommandsParser::T__30);
            setState(212);
            value();
            setState(213);
            match(TracerCommandsParser::T__29);
            setState(214);
            value();
            setState(218);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 8572755771392) != 0)) {
              setState(215);
              validateOption();
              setState(220);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__31: {
            setState(221);
            match(TracerCommandsParser::T__31);
            setState(222);
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
        setState(225);
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
    setState(231);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__19:
      case TracerCommandsParser::T__33:
      case TracerCommandsParser::T__34:
      case TracerCommandsParser::T__35:
      case TracerCommandsParser::T__36: {
        enterOuterAlt(_localctx, 1);
        setState(228);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 257699086336) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(229);
        value();
        break;
      }

      case TracerCommandsParser::METADATA_ONLY: {
        enterOuterAlt(_localctx, 2);
        setState(230);
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
    setState(233);
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 1975686004736) != 0))) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(234);
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
    setState(239);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__19:
      case TracerCommandsParser::T__33:
      case TracerCommandsParser::T__34:
      case TracerCommandsParser::T__37:
      case TracerCommandsParser::T__38:
      case TracerCommandsParser::T__39: {
        enterOuterAlt(_localctx, 1);
        setState(236);
        resolveOption();
        break;
      }

      case TracerCommandsParser::T__40: {
        enterOuterAlt(_localctx, 2);
        setState(237);
        match(TracerCommandsParser::T__40);
        break;
      }

      case TracerCommandsParser::T__41: {
        enterOuterAlt(_localctx, 3);
        setState(238);
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
    setState(246);
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
        setState(241);
        validateOption();
        break;
      }

      case TracerCommandsParser::T__42: {
        enterOuterAlt(_localctx, 2);
        setState(242);
        match(TracerCommandsParser::T__42);
        setState(243);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__43: {
        enterOuterAlt(_localctx, 3);
        setState(244);
        match(TracerCommandsParser::T__43);
        break;
      }

      case TracerCommandsParser::T__44: {
        enterOuterAlt(_localctx, 4);
        setState(245);
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
    setState(248);
    match(TracerCommandsParser::T__45);
    setState(278);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__46: {
        setState(249);
        match(TracerCommandsParser::T__46);
        setState(250);
        value();
        setState(251);
        match(TracerCommandsParser::T__47);
        setState(252);
        value();
        break;
      }

      case TracerCommandsParser::T__48: {
        setState(254);
        match(TracerCommandsParser::T__48);
        setState(255);
        value();
        break;
      }

      case TracerCommandsParser::T__49: {
        setState(256);
        match(TracerCommandsParser::T__49);
        setState(257);
        value();
        break;
      }

      case TracerCommandsParser::T__30: {
        setState(258);
        match(TracerCommandsParser::T__30);
        setState(259);
        value();
        setState(260);
        match(TracerCommandsParser::T__50);
        setState(261);
        value();
        setState(265);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 8572755771392) != 0)) {
          setState(262);
          validateOption();
          setState(267);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__51: {
        setState(268);
        match(TracerCommandsParser::T__51);
        setState(269);
        value();
        setState(270);
        match(TracerCommandsParser::T__50);
        setState(271);
        value();
        setState(275);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 70145406926848) != 0)) {
          setState(272);
          deployOption();
          setState(277);
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
    setState(280);
    match(TracerCommandsParser::T__52);
    setState(316);
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
        setState(284);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (((((_la - 48) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 48)) & 121503745) != 0)) {
          setState(281);
          streamOption();
          setState(286);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__53: {
        setState(287);
        match(TracerCommandsParser::T__53);
        setState(288);
        value();
        setState(291);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__23) {
          setState(289);
          match(TracerCommandsParser::T__23);
          setState(290);
          match(TracerCommandsParser::UINT);
        }
        break;
      }

      case TracerCommandsParser::T__49: {
        setState(293);
        match(TracerCommandsParser::T__49);
        setState(294);
        value();
        setState(295);
        match(TracerCommandsParser::T__54);
        setState(296);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__55: {
        setState(298);
        match(TracerCommandsParser::T__55);
        setState(299);
        value();
        break;
      }

      case TracerCommandsParser::T__56: {
        setState(300);
        match(TracerCommandsParser::T__56);
        setState(301);
        value();
        break;
      }

      case TracerCommandsParser::T__57: {
        setState(302);
        match(TracerCommandsParser::T__57);
        setState(303);
        value();
        break;
      }

      case TracerCommandsParser::T__58: {
        setState(304);
        match(TracerCommandsParser::T__58);
        setState(305);
        value();
        setState(306);
        match(TracerCommandsParser::T__47);
        setState(307);
        value();
        break;
      }

      case TracerCommandsParser::T__59: {
        setState(309);
        match(TracerCommandsParser::T__59);
        setState(310);
        value();
        setState(311);
        match(TracerCommandsParser::T__60);
        setState(312);
        match(TracerCommandsParser::T__61);
        setState(313);
        match(TracerCommandsParser::T__36);
        setState(314);
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
    setState(318);
    match(TracerCommandsParser::T__62);
    setState(322);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (((((_la - 48) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 48)) & 121503745) != 0)) {
      setState(319);
      streamOption();
      setState(324);
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
    setState(325);
    match(TracerCommandsParser::T__63);
    setState(326);
    value();
    setState(331);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (((((_la - 48) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 48)) & 121503745) != 0) || _la == TracerCommandsParser::SQLITE) {
      setState(329);
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
          setState(327);
          streamOption();
          break;
        }

        case TracerCommandsParser::SQLITE: {
          setState(328);
          match(TracerCommandsParser::SQLITE);
          break;
        }

      default:
        throw NoViableAltException(this);
      }
      setState(333);
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
    setState(341);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__64:
      case TracerCommandsParser::T__65:
      case TracerCommandsParser::T__66:
      case TracerCommandsParser::T__67: {
        enterOuterAlt(_localctx, 1);
        setState(334);
        _la = _input->LA(1);
        if (!(((((_la - 65) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 65)) & 15) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(335);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__68: {
        enterOuterAlt(_localctx, 2);
        setState(336);
        match(TracerCommandsParser::T__68);
        setState(337);
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
        setState(338);
        _la = _input->LA(1);
        if (!(((((_la - 48) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 48)) & 50331649) != 0))) {
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

      case TracerCommandsParser::T__73: {
        enterOuterAlt(_localctx, 4);
        setState(340);
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
    setState(343);
    match(TracerCommandsParser::T__74);
    setState(372);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__46: {
        setState(344);
        match(TracerCommandsParser::T__46);
        setState(345);
        value();
        break;
      }

      case TracerCommandsParser::T__75: {
        setState(346);
        match(TracerCommandsParser::T__75);
        break;
      }

      case TracerCommandsParser::T__76:
      case TracerCommandsParser::T__77: {
        setState(347);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__76

        || _la == TracerCommandsParser::T__77)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(348);
        value();
        setState(353);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__36

        || _la == TracerCommandsParser::T__78) {
          setState(349);
          _la = _input->LA(1);
          if (!(_la == TracerCommandsParser::T__36

          || _la == TracerCommandsParser::T__78)) {
          _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(350);
          value();
          setState(355);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__79: {
        setState(356);
        match(TracerCommandsParser::T__79);
        setState(370);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__80: {
            setState(357);
            match(TracerCommandsParser::T__80);
            setState(360);
            _errHandler->sync(this);
            _la = _input->LA(1);
            do {
              setState(360);
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
                case TracerCommandsParser::T__111:
                case TracerCommandsParser::UINT:
                case TracerCommandsParser::STRING:
                case TracerCommandsParser::WORD: {
                  setState(358);
                  value();
                  break;
                }

                case TracerCommandsParser::SHELL: {
                  setState(359);
                  match(TracerCommandsParser::SHELL);
                  break;
                }

              default:
                throw NoViableAltException(this);
              }
              setState(362);
              _errHandler->sync(this);
              _la = _input->LA(1);
            } while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & -2344475433727300996) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 64)) & 562941899798721) != 0) || ((((_la - 129) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 129)) & 39) != 0));
            break;
          }

          case TracerCommandsParser::T__57: {
            setState(364);
            match(TracerCommandsParser::T__57);
            break;
          }

          case TracerCommandsParser::T__81: {
            setState(365);
            match(TracerCommandsParser::T__81);
            setState(366);
            value();
            setState(367);
            match(TracerCommandsParser::T__82);
            setState(368);
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
    setState(374);
    match(TracerCommandsParser::T__83);
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
    setState(377);
    match(TracerCommandsParser::T__84);
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
    setState(380);
    match(TracerCommandsParser::T__85);
    setState(381);
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
    setState(383);
    match(TracerCommandsParser::T__86);
    setState(392);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & -2344475433727300994) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 64)) & 562941899798721) != 0) || ((((_la - 129) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 129)) & 35) != 0)) {
      setState(385);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == TracerCommandsParser::T__0) {
        setState(384);
        match(TracerCommandsParser::T__0);
      }
      setState(388);
      _errHandler->sync(this);
      _la = _input->LA(1);
      do {
        setState(387);
        value();
        setState(390);
        _errHandler->sync(this);
        _la = _input->LA(1);
      } while ((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & -2344475433727300996) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 64)) & 562941899798721) != 0) || ((((_la - 129) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 129)) & 35) != 0));
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
    setState(394);
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
    setState(396);
    match(TracerCommandsParser::T__89);
    setState(412);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__90: {
        setState(397);
        match(TracerCommandsParser::T__90);
        setState(399);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(398);
          bulkTraceOption();
          setState(401);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while (_la == TracerCommandsParser::T__19

        || _la == TracerCommandsParser::T__33 || ((((_la - 93) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 93)) & 15) != 0));
        break;
      }

      case TracerCommandsParser::T__91: {
        setState(403);
        match(TracerCommandsParser::T__91);
        setState(404);
        match(TracerCommandsParser::T__92);
        setState(405);
        value();
        setState(408);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__29) {
          setState(406);
          match(TracerCommandsParser::T__29);
          setState(407);
          value();
        }
        break;
      }

      case TracerCommandsParser::T__31: {
        setState(410);
        match(TracerCommandsParser::T__31);
        setState(411);
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
    setState(414);
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
    setState(421);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::STRING: {
        enterOuterAlt(_localctx, 1);
        setState(417);
        match(TracerCommandsParser::STRING);
        break;
      }

      case TracerCommandsParser::WORD: {
        enterOuterAlt(_localctx, 2);
        setState(418);
        match(TracerCommandsParser::WORD);
        break;
      }

      case TracerCommandsParser::UINT: {
        enterOuterAlt(_localctx, 3);
        setState(419);
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
      case TracerCommandsParser::T__110:
      case TracerCommandsParser::T__111: {
        enterOuterAlt(_localctx, 4);
        setState(420);
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
    setState(423);
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & -2344475433727300996) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 64)) & 562941899798721) != 0))) {
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
    setState(425);
    match(TracerCommandsParser::T__96);
    setState(449);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__97: {
        setState(426);
        match(TracerCommandsParser::T__97);
        break;
      }

      case TracerCommandsParser::T__98: {
        setState(427);
        match(TracerCommandsParser::T__98);
        setState(428);
        value();
        setState(429);
        match(TracerCommandsParser::T__22);
        setState(430);
        value();
        setState(435);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__64: {
            setState(431);
            match(TracerCommandsParser::T__64);
            setState(432);
            match(TracerCommandsParser::UINT);
            break;
          }

          case TracerCommandsParser::T__78: {
            setState(433);
            match(TracerCommandsParser::T__78);
            setState(434);
            value();
            break;
          }

        default:
          throw NoViableAltException(this);
        }
        break;
      }

      case TracerCommandsParser::T__99: {
        setState(437);
        match(TracerCommandsParser::T__99);
        setState(438);
        value();
        setState(439);
        match(TracerCommandsParser::T__22);
        setState(440);
        value();
        setState(441);
        match(TracerCommandsParser::T__64);
        setState(442);
        match(TracerCommandsParser::UINT);
        setState(443);
        match(TracerCommandsParser::T__112);
        setState(444);
        value();
        setState(447);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__23) {
          setState(445);
          match(TracerCommandsParser::T__23);
          setState(446);
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
    setState(451);
    match(TracerCommandsParser::T__25);
    setState(452);
    match(TracerCommandsParser::T__100);
    setState(469);
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
      case TracerCommandsParser::T__111:
      case TracerCommandsParser::UINT:
      case TracerCommandsParser::STRING:
      case TracerCommandsParser::WORD: {
        setState(453);
        value();
        setState(454);
        match(TracerCommandsParser::T__22);
        setState(455);
        value();
        setState(458);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__19) {
          setState(456);
          match(TracerCommandsParser::T__19);
          setState(457);
          value();
        }
        break;
      }

      case TracerCommandsParser::T__19: {
        setState(460);
        match(TracerCommandsParser::T__19);
        setState(461);
        value();
        setState(462);
        match(TracerCommandsParser::T__113);
        setState(463);
        value();
        setState(464);
        match(TracerCommandsParser::T__114);
        setState(465);
        value();
        setState(466);
        match(TracerCommandsParser::T__22);
        setState(467);
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
    setState(471);
    match(TracerCommandsParser::T__25);
    setState(472);
    match(TracerCommandsParser::T__101);
    setState(473);
    match(TracerCommandsParser::UINT);
    setState(474);
    match(TracerCommandsParser::T__29);
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
    setState(489);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__102: {
        enterOuterAlt(_localctx, 1);
        setState(477);
        match(TracerCommandsParser::T__102);
        setState(478);
        value();
        setState(482);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (((((_la - 66) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 66)) & 70931694131085313) != 0)) {
          setState(479);
          launchOption();
          setState(484);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__103:
      case TracerCommandsParser::T__104: {
        enterOuterAlt(_localctx, 2);
        setState(485);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__103

        || _la == TracerCommandsParser::T__104)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(487);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::UINT) {
          setState(486);
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
    setState(495);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__115:
      case TracerCommandsParser::T__116:
      case TracerCommandsParser::T__117:
      case TracerCommandsParser::T__118:
      case TracerCommandsParser::T__119:
      case TracerCommandsParser::T__120: {
        enterOuterAlt(_localctx, 1);
        setState(491);
        _la = _input->LA(1);
        if (!(((((_la - 116) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 116)) & 63) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(492);
        value();
        break;
      }

      case TracerCommandsParser::T__65: {
        enterOuterAlt(_localctx, 2);
        setState(493);
        match(TracerCommandsParser::T__65);
        setState(494);
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
    setState(513);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__105: {
        enterOuterAlt(_localctx, 1);
        setState(497);
        match(TracerCommandsParser::T__105);
        setState(498);
        value();
        setState(505);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__65

        || _la == TracerCommandsParser::T__115) {
          setState(503);
          _errHandler->sync(this);
          switch (_input->LA(1)) {
            case TracerCommandsParser::T__115: {
              setState(499);
              match(TracerCommandsParser::T__115);
              setState(500);
              value();
              break;
            }

            case TracerCommandsParser::T__65: {
              setState(501);
              match(TracerCommandsParser::T__65);
              setState(502);
              match(TracerCommandsParser::UINT);
              break;
            }

          default:
            throw NoViableAltException(this);
          }
          setState(507);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__106: {
        enterOuterAlt(_localctx, 2);
        setState(508);
        match(TracerCommandsParser::T__106);
        setState(511);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__65) {
          setState(509);
          match(TracerCommandsParser::T__65);
          setState(510);
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
    setState(518);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__107: {
        setState(515);
        match(TracerCommandsParser::T__107);
        break;
      }

      case TracerCommandsParser::T__108: {
        setState(516);
        match(TracerCommandsParser::T__108);
        setState(517);
        match(TracerCommandsParser::T__109);
        break;
      }

    default:
      throw NoViableAltException(this);
    }
    setState(524);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == TracerCommandsParser::T__64

    || _la == TracerCommandsParser::T__65) {
      setState(520);
      _la = _input->LA(1);
      if (!(_la == TracerCommandsParser::T__64

      || _la == TracerCommandsParser::T__65)) {
      _errHandler->recoverInline(this);
      }
      else {
        _errHandler->reportMatch(this);
        consume();
      }
      setState(521);
      match(TracerCommandsParser::UINT);
      setState(526);
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
    setState(527);
    match(TracerCommandsParser::T__110);
    setState(536);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__2: {
        setState(528);
        match(TracerCommandsParser::T__2);
        setState(532);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__21

        || _la == TracerCommandsParser::T__22 || ((((_la - 65) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 65)) & 9079256848778919939) != 0)) {
          setState(529);
          instructionOption();
          setState(534);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__5: {
        setState(535);
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
    setState(545);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__64:
      case TracerCommandsParser::T__65:
      case TracerCommandsParser::T__121:
      case TracerCommandsParser::T__122: {
        enterOuterAlt(_localctx, 1);
        setState(538);
        _la = _input->LA(1);
        if (!(((((_la - 65) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 65)) & 432345564227567619) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(539);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__123: {
        enterOuterAlt(_localctx, 2);
        setState(540);
        match(TracerCommandsParser::T__123);
        setState(541);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__124: {
        enterOuterAlt(_localctx, 3);
        setState(542);
        match(TracerCommandsParser::T__124);
        break;
      }

      case TracerCommandsParser::T__21:
      case TracerCommandsParser::T__22:
      case TracerCommandsParser::T__125:
      case TracerCommandsParser::T__126: {
        enterOuterAlt(_localctx, 4);
        setState(543);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__21

        || _la == TracerCommandsParser::T__22 || _la == TracerCommandsParser::T__125

        || _la == TracerCommandsParser::T__126)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(544);
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
    setState(547);
    match(TracerCommandsParser::T__111);
    setState(554);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == TracerCommandsParser::T__21

    || _la == TracerCommandsParser::T__22 || ((((_la - 65) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 65)) & -8791026472627208189) != 0)) {
      setState(552);
      _errHandler->sync(this);
      switch (_input->LA(1)) {
        case TracerCommandsParser::T__64:
        case TracerCommandsParser::T__65:
        case TracerCommandsParser::T__121:
        case TracerCommandsParser::T__122: {
          setState(548);
          _la = _input->LA(1);
          if (!(((((_la - 65) & ~ 0x3fULL) == 0) &&
            ((1ULL << (_la - 65)) & 432345564227567619) != 0))) {
          _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(549);
          match(TracerCommandsParser::UINT);
          break;
        }

        case TracerCommandsParser::T__21:
        case TracerCommandsParser::T__22:
        case TracerCommandsParser::T__127: {
          setState(550);
          _la = _input->LA(1);
          if (!(_la == TracerCommandsParser::T__21

          || _la == TracerCommandsParser::T__22 || _la == TracerCommandsParser::T__127)) {
          _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(551);
          value();
          break;
        }

      default:
        throw NoViableAltException(this);
      }
      setState(556);
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
