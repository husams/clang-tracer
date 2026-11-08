
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
      "symbolOption", "probe", "prepareOption", "resolveOption", "deployOption",
      "interceptor", "events", "collect", "record", "streamOption", "cpp",
      "shell", "complete", "type", "help", "quit", "bulkTrace", "bulkTraceOption",
      "value", "keyword", "memory", "probeEntry", "probeReload", "stackFilter",
      "contextEventBytes", "extraCompileOption", "cwdOption"
    },
    std::vector<std::string>{
      "", "'%'", "'server'", "'start'", "'connect'", "'status'", "'stop'",
      "'--executable'", "'--agent'", "'attach'", "'--start-time'", "'processes'",
      "'--filter'", "'sessions'", "'use'", "'detach'", "'modules'", "'--details'",
      "'threads'", "'symbols'", "'--module'", "'--page'", "'--address'",
      "'--mapping'", "'--limit'", "'--addresses'", "'probe'", "'prepare'",
      "'resolve'", "'inject'", "'--callback'", "'validate'", "'remove'",
      "'probes'", "'--include'", "'--compdb'", "'--translation-unit'", "'--output'",
      "'--compile-option'", "'--prepared'", "'--prepared-mode'", "'--stack-depth'",
      "'--print-arguments'", "'--print-result'", "'interceptor'", "'load'",
      "'--as'", "'edit'", "'show'", "'--target'", "'upload'", "'events'",
      "'list'", "'--index'", "'calls'", "'stats'", "'clear'", "'open'",
      "'export'", "'--format'", "'json'", "'collect'", "'record'", "'--count'",
      "'--timeout'", "'--after'", "'--probe'", "'--phase'", "'PRE'", "'POST'",
      "'--name'", "'--session'", "'--all'", "'cpp'", "'reset'", "'import'",
      "'generate'", "'--type'", "'options'", "'add'", "'compdb'", "'--file'",
      "'shell'", "'complete'", "'type'", "'help'", "'quit'", "'exit'", "'trace'",
      "'preview'", "'apply'", "'--manifest'", "'--exclude'", "'--exact'",
      "'--save'", "'memory'", "'maps'", "'read'", "'scan'", "'entry'", "'reload'",
      "'--pattern'", "'--elf-address'", "'--abi'", "'--stack-application-only'",
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
      "", "", "", "", "", "", "", "UINT", "STRING", "SHELL", "METADATA_ONLY",
      "SQLITE", "STACK", "CWD", "SIGNATURE", "MEMBER", "WORD", "WS"
    }
  );
  static const int32_t serializedATNSegment[] = {
    4,1,119,511,2,0,7,0,2,1,7,1,2,2,7,2,2,3,7,3,2,4,7,4,2,5,7,5,2,6,7,6,2,
    7,7,7,2,8,7,8,2,9,7,9,2,10,7,10,2,11,7,11,2,12,7,12,2,13,7,13,2,14,7,
    14,2,15,7,15,2,16,7,16,2,17,7,17,2,18,7,18,2,19,7,19,2,20,7,20,2,21,7,
    21,2,22,7,22,2,23,7,23,2,24,7,24,2,25,7,25,2,26,7,26,2,27,7,27,2,28,7,
    28,2,29,7,29,2,30,7,30,2,31,7,31,2,32,7,32,2,33,7,33,2,34,7,34,2,35,7,
    35,2,36,7,36,2,37,7,37,2,38,7,38,2,39,7,39,1,0,1,0,1,0,1,0,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,3,1,110,8,1,1,2,1,2,1,2,5,2,115,8,2,10,2,12,2,118,
    9,2,1,2,1,2,1,2,1,2,3,2,124,8,2,1,3,1,3,1,3,3,3,129,8,3,1,4,1,4,1,4,1,
    4,3,4,135,8,4,3,4,137,8,4,1,5,1,5,1,5,3,5,142,8,5,1,6,1,6,1,7,1,7,1,7,
    1,8,1,8,1,9,1,9,3,9,153,8,9,1,10,1,10,3,10,157,8,10,1,11,1,11,1,12,1,
    12,5,12,163,8,12,10,12,12,12,166,9,12,1,13,1,13,1,13,1,13,1,13,3,13,173,
    8,13,1,14,1,14,1,14,1,14,4,14,179,8,14,11,14,12,14,180,1,14,1,14,1,14,
    1,14,5,14,187,8,14,10,14,12,14,190,9,14,1,14,1,14,1,14,1,14,3,14,196,
    8,14,1,14,1,14,5,14,200,8,14,10,14,12,14,203,9,14,1,14,1,14,1,14,1,14,
    1,14,1,14,5,14,211,8,14,10,14,12,14,214,9,14,1,14,1,14,1,14,3,14,219,
    8,14,3,14,221,8,14,1,14,1,14,3,14,225,8,14,3,14,227,8,14,1,15,1,15,1,
    15,1,15,1,15,1,15,1,15,1,15,3,15,237,8,15,1,16,1,16,1,16,3,16,242,8,16,
    1,17,1,17,1,17,1,17,1,17,1,17,3,17,250,8,17,1,18,1,18,1,18,1,18,1,18,
    1,18,1,18,1,18,1,18,1,18,1,18,1,18,1,18,1,18,1,18,5,18,267,8,18,10,18,
    12,18,270,9,18,1,18,1,18,1,18,1,18,1,18,5,18,277,8,18,10,18,12,18,280,
    9,18,3,18,282,8,18,1,19,1,19,5,19,286,8,19,10,19,12,19,289,9,19,1,19,
    1,19,1,19,1,19,3,19,295,8,19,1,19,1,19,1,19,1,19,1,19,1,19,1,19,1,19,
    1,19,1,19,1,19,1,19,1,19,1,19,1,19,1,19,1,19,1,19,1,19,1,19,1,19,1,19,
    1,19,1,19,1,19,1,19,1,19,1,19,3,19,325,8,19,1,20,1,20,5,20,329,8,20,10,
    20,12,20,332,9,20,1,21,1,21,1,21,1,21,5,21,338,8,21,10,21,12,21,341,9,
    21,1,22,1,22,1,22,1,22,1,22,1,22,1,22,3,22,350,8,22,1,23,1,23,1,23,1,
    23,1,23,1,23,1,23,1,23,5,23,360,8,23,10,23,12,23,363,9,23,1,23,1,23,1,
    23,1,23,4,23,369,8,23,11,23,12,23,370,1,23,1,23,1,23,1,23,1,23,1,23,3,
    23,379,8,23,3,23,381,8,23,1,24,1,24,1,24,1,25,1,25,1,25,1,26,1,26,1,26,
    1,27,1,27,3,27,394,8,27,1,27,4,27,397,8,27,11,27,12,27,398,3,27,401,8,
    27,1,28,1,28,1,29,1,29,1,29,4,29,408,8,29,11,29,12,29,409,1,29,1,29,1,
    29,1,29,1,29,3,29,417,8,29,1,29,1,29,3,29,421,8,29,1,30,1,30,1,30,1,31,
    1,31,1,31,1,31,3,31,430,8,31,1,32,1,32,1,33,1,33,1,33,1,33,1,33,1,33,
    1,33,1,33,1,33,1,33,3,33,444,8,33,1,33,1,33,1,33,1,33,1,33,1,33,1,33,
    1,33,1,33,1,33,3,33,456,8,33,3,33,458,8,33,1,34,1,34,1,34,1,34,1,34,1,
    34,1,34,3,34,467,8,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,1,34,3,
    34,478,8,34,1,34,3,34,481,8,34,1,35,1,35,1,35,1,35,1,35,1,35,3,35,489,
    8,35,1,36,1,36,1,36,1,37,1,37,1,37,1,38,1,38,1,38,1,38,1,38,4,38,502,
    8,38,11,38,12,38,503,3,38,506,8,38,1,39,1,39,1,39,1,39,0,0,40,0,2,4,6,
    8,10,12,14,16,18,20,22,24,26,28,30,32,34,36,38,40,42,44,46,48,50,52,54,
    56,58,60,62,64,66,68,70,72,74,76,78,0,14,1,0,7,8,2,0,12,12,20,23,2,0,
    20,20,34,37,3,0,20,20,34,36,38,40,2,0,109,109,118,118,1,0,63,66,1,0,68,
    69,2,0,46,46,70,71,1,0,75,76,2,0,37,37,77,77,1,0,86,87,3,0,20,20,34,34,
    91,94,18,0,2,6,9,9,11,11,13,16,18,19,26,29,31,33,44,45,47,48,50,52,54,
    58,60,62,68,69,73,76,78,80,82,90,95,100,114,114,1,0,105,106,584,0,80,
    1,0,0,0,2,109,1,0,0,0,4,111,1,0,0,0,6,128,1,0,0,0,8,130,1,0,0,0,10,138,
    1,0,0,0,12,143,1,0,0,0,14,145,1,0,0,0,16,148,1,0,0,0,18,150,1,0,0,0,20,
    154,1,0,0,0,22,158,1,0,0,0,24,160,1,0,0,0,26,172,1,0,0,0,28,226,1,0,0,
    0,30,236,1,0,0,0,32,241,1,0,0,0,34,249,1,0,0,0,36,251,1,0,0,0,38,283,
    1,0,0,0,40,326,1,0,0,0,42,333,1,0,0,0,44,349,1,0,0,0,46,351,1,0,0,0,48,
    382,1,0,0,0,50,385,1,0,0,0,52,388,1,0,0,0,54,391,1,0,0,0,56,402,1,0,0,
    0,58,404,1,0,0,0,60,422,1,0,0,0,62,429,1,0,0,0,64,431,1,0,0,0,66,433,
    1,0,0,0,68,459,1,0,0,0,70,482,1,0,0,0,72,490,1,0,0,0,74,493,1,0,0,0,76,
    505,1,0,0,0,78,507,1,0,0,0,80,81,5,1,0,0,81,82,3,2,1,0,82,83,5,0,0,1,
    83,1,1,0,0,0,84,110,3,4,2,0,85,110,3,8,4,0,86,110,3,10,5,0,87,110,3,12,
    6,0,88,110,3,14,7,0,89,110,3,16,8,0,90,110,3,18,9,0,91,110,3,20,10,0,
    92,110,3,22,11,0,93,110,3,24,12,0,94,110,3,28,14,0,95,110,3,36,18,0,96,
    110,3,38,19,0,97,110,3,40,20,0,98,110,3,42,21,0,99,110,3,46,23,0,100,
    110,3,48,24,0,101,110,3,50,25,0,102,110,3,52,26,0,103,110,3,54,27,0,104,
    110,3,56,28,0,105,110,3,58,29,0,106,110,3,66,33,0,107,110,3,68,34,0,108,
    110,3,70,35,0,109,84,1,0,0,0,109,85,1,0,0,0,109,86,1,0,0,0,109,87,1,0,
    0,0,109,88,1,0,0,0,109,89,1,0,0,0,109,90,1,0,0,0,109,91,1,0,0,0,109,92,
    1,0,0,0,109,93,1,0,0,0,109,94,1,0,0,0,109,95,1,0,0,0,109,96,1,0,0,0,109,
    97,1,0,0,0,109,98,1,0,0,0,109,99,1,0,0,0,109,100,1,0,0,0,109,101,1,0,
    0,0,109,102,1,0,0,0,109,103,1,0,0,0,109,104,1,0,0,0,109,105,1,0,0,0,109,
    106,1,0,0,0,109,107,1,0,0,0,109,108,1,0,0,0,110,3,1,0,0,0,111,123,5,2,
    0,0,112,116,5,3,0,0,113,115,3,6,3,0,114,113,1,0,0,0,115,118,1,0,0,0,116,
    114,1,0,0,0,116,117,1,0,0,0,117,124,1,0,0,0,118,116,1,0,0,0,119,120,5,
    4,0,0,120,124,3,62,31,0,121,124,5,5,0,0,122,124,5,6,0,0,123,112,1,0,0,
    0,123,119,1,0,0,0,123,121,1,0,0,0,123,122,1,0,0,0,124,5,1,0,0,0,125,126,
    7,0,0,0,126,129,3,62,31,0,127,129,3,74,37,0,128,125,1,0,0,0,128,127,1,
    0,0,0,129,7,1,0,0,0,130,136,5,9,0,0,131,134,5,109,0,0,132,133,5,10,0,
    0,133,135,5,109,0,0,134,132,1,0,0,0,134,135,1,0,0,0,135,137,1,0,0,0,136,
    131,1,0,0,0,136,137,1,0,0,0,137,9,1,0,0,0,138,141,5,11,0,0,139,140,5,
    12,0,0,140,142,3,62,31,0,141,139,1,0,0,0,141,142,1,0,0,0,142,11,1,0,0,
    0,143,144,5,13,0,0,144,13,1,0,0,0,145,146,5,14,0,0,146,147,3,62,31,0,
    147,15,1,0,0,0,148,149,5,5,0,0,149,17,1,0,0,0,150,152,5,15,0,0,151,153,
    3,62,31,0,152,151,1,0,0,0,152,153,1,0,0,0,153,19,1,0,0,0,154,156,5,16,
    0,0,155,157,5,17,0,0,156,155,1,0,0,0,156,157,1,0,0,0,157,21,1,0,0,0,158,
    159,5,18,0,0,159,23,1,0,0,0,160,164,5,19,0,0,161,163,3,26,13,0,162,161,
    1,0,0,0,163,166,1,0,0,0,164,162,1,0,0,0,164,165,1,0,0,0,165,25,1,0,0,
    0,166,164,1,0,0,0,167,168,7,1,0,0,168,173,3,62,31,0,169,170,5,24,0,0,
    170,173,5,109,0,0,171,173,5,25,0,0,172,167,1,0,0,0,172,169,1,0,0,0,172,
    171,1,0,0,0,173,27,1,0,0,0,174,220,5,26,0,0,175,176,5,27,0,0,176,178,
    3,62,31,0,177,179,3,30,15,0,178,177,1,0,0,0,179,180,1,0,0,0,180,178,1,
    0,0,0,180,181,1,0,0,0,181,221,1,0,0,0,182,183,5,28,0,0,183,188,3,62,31,
    0,184,187,3,32,16,0,185,187,3,78,39,0,186,184,1,0,0,0,186,185,1,0,0,0,
    187,190,1,0,0,0,188,186,1,0,0,0,188,189,1,0,0,0,189,221,1,0,0,0,190,188,
    1,0,0,0,191,192,5,29,0,0,192,195,3,62,31,0,193,194,5,30,0,0,194,196,3,
    62,31,0,195,193,1,0,0,0,195,196,1,0,0,0,196,201,1,0,0,0,197,200,3,34,
    17,0,198,200,3,78,39,0,199,197,1,0,0,0,199,198,1,0,0,0,200,203,1,0,0,
    0,201,199,1,0,0,0,201,202,1,0,0,0,202,221,1,0,0,0,203,201,1,0,0,0,204,
    205,5,31,0,0,205,206,3,62,31,0,206,207,5,30,0,0,207,212,3,62,31,0,208,
    211,3,32,16,0,209,211,3,78,39,0,210,208,1,0,0,0,210,209,1,0,0,0,211,214,
    1,0,0,0,212,210,1,0,0,0,212,213,1,0,0,0,213,221,1,0,0,0,214,212,1,0,0,
    0,215,216,5,32,0,0,216,218,5,109,0,0,217,219,3,78,39,0,218,217,1,0,0,
    0,218,219,1,0,0,0,219,221,1,0,0,0,220,175,1,0,0,0,220,182,1,0,0,0,220,
    191,1,0,0,0,220,204,1,0,0,0,220,215,1,0,0,0,221,227,1,0,0,0,222,224,5,
    33,0,0,223,225,3,78,39,0,224,223,1,0,0,0,224,225,1,0,0,0,225,227,1,0,
    0,0,226,174,1,0,0,0,226,222,1,0,0,0,227,29,1,0,0,0,228,229,7,2,0,0,229,
    237,3,62,31,0,230,231,5,116,0,0,231,237,3,62,31,0,232,237,5,117,0,0,233,
    237,5,112,0,0,234,237,3,76,38,0,235,237,3,78,39,0,236,228,1,0,0,0,236,
    230,1,0,0,0,236,232,1,0,0,0,236,233,1,0,0,0,236,234,1,0,0,0,236,235,1,
    0,0,0,237,31,1,0,0,0,238,239,7,3,0,0,239,242,3,62,31,0,240,242,3,76,38,
    0,241,238,1,0,0,0,241,240,1,0,0,0,242,33,1,0,0,0,243,250,3,32,16,0,244,
    245,5,41,0,0,245,250,7,4,0,0,246,250,5,42,0,0,247,250,5,43,0,0,248,250,
    3,72,36,0,249,243,1,0,0,0,249,244,1,0,0,0,249,246,1,0,0,0,249,247,1,0,
    0,0,249,248,1,0,0,0,250,35,1,0,0,0,251,281,5,44,0,0,252,253,5,45,0,0,
    253,254,3,62,31,0,254,255,5,46,0,0,255,256,3,62,31,0,256,282,1,0,0,0,
    257,258,5,47,0,0,258,282,3,62,31,0,259,260,5,48,0,0,260,282,3,62,31,0,
    261,262,5,31,0,0,262,263,3,62,31,0,263,264,5,49,0,0,264,268,3,62,31,0,
    265,267,3,32,16,0,266,265,1,0,0,0,267,270,1,0,0,0,268,266,1,0,0,0,268,
    269,1,0,0,0,269,282,1,0,0,0,270,268,1,0,0,0,271,272,5,50,0,0,272,273,
    3,62,31,0,273,274,5,49,0,0,274,278,3,62,31,0,275,277,3,34,17,0,276,275,
    1,0,0,0,277,280,1,0,0,0,278,276,1,0,0,0,278,279,1,0,0,0,279,282,1,0,0,
    0,280,278,1,0,0,0,281,252,1,0,0,0,281,257,1,0,0,0,281,259,1,0,0,0,281,
    261,1,0,0,0,281,271,1,0,0,0,282,37,1,0,0,0,283,324,5,51,0,0,284,286,3,
    44,22,0,285,284,1,0,0,0,286,289,1,0,0,0,287,285,1,0,0,0,287,288,1,0,0,
    0,288,325,1,0,0,0,289,287,1,0,0,0,290,291,5,52,0,0,291,294,3,62,31,0,
    292,293,5,24,0,0,293,295,5,109,0,0,294,292,1,0,0,0,294,295,1,0,0,0,295,
    325,1,0,0,0,296,297,5,48,0,0,297,298,3,62,31,0,298,299,5,53,0,0,299,300,
    5,109,0,0,300,325,1,0,0,0,301,302,5,114,0,0,302,303,3,62,31,0,303,304,
    5,53,0,0,304,305,5,109,0,0,305,325,1,0,0,0,306,307,5,54,0,0,307,325,3,
    62,31,0,308,309,5,55,0,0,309,325,3,62,31,0,310,311,5,56,0,0,311,325,3,
    62,31,0,312,313,5,57,0,0,313,314,3,62,31,0,314,315,5,46,0,0,315,316,3,
    62,31,0,316,325,1,0,0,0,317,318,5,58,0,0,318,319,3,62,31,0,319,320,5,
    59,0,0,320,321,5,60,0,0,321,322,5,37,0,0,322,323,3,62,31,0,323,325,1,
    0,0,0,324,287,1,0,0,0,324,290,1,0,0,0,324,296,1,0,0,0,324,301,1,0,0,0,
    324,306,1,0,0,0,324,308,1,0,0,0,324,310,1,0,0,0,324,312,1,0,0,0,324,317,
    1,0,0,0,325,39,1,0,0,0,326,330,5,61,0,0,327,329,3,44,22,0,328,327,1,0,
    0,0,329,332,1,0,0,0,330,328,1,0,0,0,330,331,1,0,0,0,331,41,1,0,0,0,332,
    330,1,0,0,0,333,334,5,62,0,0,334,339,3,62,31,0,335,338,3,44,22,0,336,
    338,5,113,0,0,337,335,1,0,0,0,337,336,1,0,0,0,338,341,1,0,0,0,339,337,
    1,0,0,0,339,340,1,0,0,0,340,43,1,0,0,0,341,339,1,0,0,0,342,343,7,5,0,
    0,343,350,5,109,0,0,344,345,5,67,0,0,345,350,7,6,0,0,346,347,7,7,0,0,
    347,350,3,62,31,0,348,350,5,72,0,0,349,342,1,0,0,0,349,344,1,0,0,0,349,
    346,1,0,0,0,349,348,1,0,0,0,350,45,1,0,0,0,351,380,5,73,0,0,352,353,5,
    45,0,0,353,381,3,62,31,0,354,381,5,74,0,0,355,356,7,8,0,0,356,361,3,62,
    31,0,357,358,7,9,0,0,358,360,3,62,31,0,359,357,1,0,0,0,360,363,1,0,0,
    0,361,359,1,0,0,0,361,362,1,0,0,0,362,381,1,0,0,0,363,361,1,0,0,0,364,
    378,5,78,0,0,365,368,5,79,0,0,366,369,3,62,31,0,367,369,5,111,0,0,368,
    366,1,0,0,0,368,367,1,0,0,0,369,370,1,0,0,0,370,368,1,0,0,0,370,371,1,
    0,0,0,371,379,1,0,0,0,372,379,5,56,0,0,373,374,5,80,0,0,374,375,3,62,
    31,0,375,376,5,81,0,0,376,377,3,62,31,0,377,379,1,0,0,0,378,365,1,0,0,
    0,378,372,1,0,0,0,378,373,1,0,0,0,378,379,1,0,0,0,379,381,1,0,0,0,380,
    352,1,0,0,0,380,354,1,0,0,0,380,355,1,0,0,0,380,364,1,0,0,0,381,47,1,
    0,0,0,382,383,5,82,0,0,383,384,3,62,31,0,384,49,1,0,0,0,385,386,5,83,
    0,0,386,387,3,62,31,0,387,51,1,0,0,0,388,389,5,84,0,0,389,390,3,62,31,
    0,390,53,1,0,0,0,391,400,5,85,0,0,392,394,5,1,0,0,393,392,1,0,0,0,393,
    394,1,0,0,0,394,396,1,0,0,0,395,397,3,62,31,0,396,395,1,0,0,0,397,398,
    1,0,0,0,398,396,1,0,0,0,398,399,1,0,0,0,399,401,1,0,0,0,400,393,1,0,0,
    0,400,401,1,0,0,0,401,55,1,0,0,0,402,403,7,10,0,0,403,57,1,0,0,0,404,
    420,5,88,0,0,405,407,5,89,0,0,406,408,3,60,30,0,407,406,1,0,0,0,408,409,
    1,0,0,0,409,407,1,0,0,0,409,410,1,0,0,0,410,421,1,0,0,0,411,412,5,90,
    0,0,412,413,5,91,0,0,413,416,3,62,31,0,414,415,5,30,0,0,415,417,3,62,
    31,0,416,414,1,0,0,0,416,417,1,0,0,0,417,421,1,0,0,0,418,419,5,32,0,0,
    419,421,3,62,31,0,420,405,1,0,0,0,420,411,1,0,0,0,420,418,1,0,0,0,421,
    59,1,0,0,0,422,423,7,11,0,0,423,424,3,62,31,0,424,61,1,0,0,0,425,430,
    5,110,0,0,426,430,5,118,0,0,427,430,5,109,0,0,428,430,3,64,32,0,429,425,
    1,0,0,0,429,426,1,0,0,0,429,427,1,0,0,0,429,428,1,0,0,0,430,63,1,0,0,
    0,431,432,7,12,0,0,432,65,1,0,0,0,433,457,5,95,0,0,434,458,5,96,0,0,435,
    436,5,97,0,0,436,437,3,62,31,0,437,438,5,23,0,0,438,443,3,62,31,0,439,
    440,5,63,0,0,440,444,5,109,0,0,441,442,5,77,0,0,442,444,3,62,31,0,443,
    439,1,0,0,0,443,441,1,0,0,0,444,458,1,0,0,0,445,446,5,98,0,0,446,447,
    3,62,31,0,447,448,5,23,0,0,448,449,3,62,31,0,449,450,5,63,0,0,450,451,
    5,109,0,0,451,452,5,101,0,0,452,455,3,62,31,0,453,454,5,24,0,0,454,456,
    5,109,0,0,455,453,1,0,0,0,455,456,1,0,0,0,456,458,1,0,0,0,457,434,1,0,
    0,0,457,435,1,0,0,0,457,445,1,0,0,0,458,67,1,0,0,0,459,460,5,26,0,0,460,
    477,5,99,0,0,461,462,3,62,31,0,462,463,5,23,0,0,463,466,3,62,31,0,464,
    465,5,20,0,0,465,467,3,62,31,0,466,464,1,0,0,0,466,467,1,0,0,0,467,478,
    1,0,0,0,468,469,5,20,0,0,469,470,3,62,31,0,470,471,5,102,0,0,471,472,
    3,62,31,0,472,473,5,103,0,0,473,474,3,62,31,0,474,475,5,23,0,0,475,476,
    3,62,31,0,476,478,1,0,0,0,477,461,1,0,0,0,477,468,1,0,0,0,478,480,1,0,
    0,0,479,481,3,78,39,0,480,479,1,0,0,0,480,481,1,0,0,0,481,69,1,0,0,0,
    482,483,5,26,0,0,483,484,5,100,0,0,484,485,5,109,0,0,485,486,5,30,0,0,
    486,488,3,62,31,0,487,489,3,78,39,0,488,487,1,0,0,0,488,489,1,0,0,0,489,
    71,1,0,0,0,490,491,5,104,0,0,491,492,3,62,31,0,492,73,1,0,0,0,493,494,
    7,13,0,0,494,495,5,109,0,0,495,75,1,0,0,0,496,497,5,107,0,0,497,506,3,
    62,31,0,498,501,5,108,0,0,499,502,3,62,31,0,500,502,5,111,0,0,501,499,
    1,0,0,0,501,500,1,0,0,0,502,503,1,0,0,0,503,501,1,0,0,0,503,504,1,0,0,
    0,504,506,1,0,0,0,505,496,1,0,0,0,505,498,1,0,0,0,506,77,1,0,0,0,507,
    508,5,115,0,0,508,509,3,62,31,0,509,79,1,0,0,0,58,109,116,123,128,134,
    136,141,152,156,164,172,180,186,188,195,199,201,210,212,218,220,224,226,
    236,241,249,268,278,281,287,294,324,330,337,339,349,361,368,370,378,380,
    393,398,400,409,416,420,429,443,455,457,466,477,480,488,501,503,505
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
    setState(80);
    match(TracerCommandsParser::T__0);
    setState(81);
    command();
    setState(82);
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
    setState(109);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 0, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(84);
      server();
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(85);
      attach();
      break;
    }

    case 3: {
      enterOuterAlt(_localctx, 3);
      setState(86);
      processes();
      break;
    }

    case 4: {
      enterOuterAlt(_localctx, 4);
      setState(87);
      sessions();
      break;
    }

    case 5: {
      enterOuterAlt(_localctx, 5);
      setState(88);
      use();
      break;
    }

    case 6: {
      enterOuterAlt(_localctx, 6);
      setState(89);
      status();
      break;
    }

    case 7: {
      enterOuterAlt(_localctx, 7);
      setState(90);
      detach();
      break;
    }

    case 8: {
      enterOuterAlt(_localctx, 8);
      setState(91);
      modules();
      break;
    }

    case 9: {
      enterOuterAlt(_localctx, 9);
      setState(92);
      threads();
      break;
    }

    case 10: {
      enterOuterAlt(_localctx, 10);
      setState(93);
      symbols();
      break;
    }

    case 11: {
      enterOuterAlt(_localctx, 11);
      setState(94);
      probe();
      break;
    }

    case 12: {
      enterOuterAlt(_localctx, 12);
      setState(95);
      interceptor();
      break;
    }

    case 13: {
      enterOuterAlt(_localctx, 13);
      setState(96);
      events();
      break;
    }

    case 14: {
      enterOuterAlt(_localctx, 14);
      setState(97);
      collect();
      break;
    }

    case 15: {
      enterOuterAlt(_localctx, 15);
      setState(98);
      record();
      break;
    }

    case 16: {
      enterOuterAlt(_localctx, 16);
      setState(99);
      cpp();
      break;
    }

    case 17: {
      enterOuterAlt(_localctx, 17);
      setState(100);
      shell();
      break;
    }

    case 18: {
      enterOuterAlt(_localctx, 18);
      setState(101);
      complete();
      break;
    }

    case 19: {
      enterOuterAlt(_localctx, 19);
      setState(102);
      type();
      break;
    }

    case 20: {
      enterOuterAlt(_localctx, 20);
      setState(103);
      help();
      break;
    }

    case 21: {
      enterOuterAlt(_localctx, 21);
      setState(104);
      quit();
      break;
    }

    case 22: {
      enterOuterAlt(_localctx, 22);
      setState(105);
      bulkTrace();
      break;
    }

    case 23: {
      enterOuterAlt(_localctx, 23);
      setState(106);
      memory();
      break;
    }

    case 24: {
      enterOuterAlt(_localctx, 24);
      setState(107);
      probeEntry();
      break;
    }

    case 25: {
      enterOuterAlt(_localctx, 25);
      setState(108);
      probeReload();
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
    setState(111);
    match(TracerCommandsParser::T__1);
    setState(123);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__2: {
        setState(112);
        match(TracerCommandsParser::T__2);
        setState(116);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__6

        || _la == TracerCommandsParser::T__7 || _la == TracerCommandsParser::T__104

        || _la == TracerCommandsParser::T__105) {
          setState(113);
          serverOption();
          setState(118);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__3: {
        setState(119);
        match(TracerCommandsParser::T__3);
        setState(120);
        value();
        break;
      }

      case TracerCommandsParser::T__4: {
        setState(121);
        match(TracerCommandsParser::T__4);
        break;
      }

      case TracerCommandsParser::T__5: {
        setState(122);
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
    setState(128);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__6:
      case TracerCommandsParser::T__7: {
        enterOuterAlt(_localctx, 1);
        setState(125);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__6

        || _la == TracerCommandsParser::T__7)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(126);
        value();
        break;
      }

      case TracerCommandsParser::T__104:
      case TracerCommandsParser::T__105: {
        enterOuterAlt(_localctx, 2);
        setState(127);
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
    setState(130);
    match(TracerCommandsParser::T__8);
    setState(136);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::UINT) {
      setState(131);
      match(TracerCommandsParser::UINT);
      setState(134);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == TracerCommandsParser::T__9) {
        setState(132);
        match(TracerCommandsParser::T__9);
        setState(133);
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
    setState(138);
    match(TracerCommandsParser::T__10);
    setState(141);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::T__11) {
      setState(139);
      match(TracerCommandsParser::T__11);
      setState(140);
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
    setState(143);
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
    setState(145);
    match(TracerCommandsParser::T__13);
    setState(146);
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
    setState(148);
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
    setState(150);
    match(TracerCommandsParser::T__14);
    setState(152);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 8637253190452898428) != 0) || ((((_la - 68) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 68)) & 1202874184883683) != 0)) {
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
    setState(154);
    match(TracerCommandsParser::T__15);
    setState(156);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::T__16) {
      setState(155);
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
    setState(158);
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
    setState(160);
    match(TracerCommandsParser::T__18);
    setState(164);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 66064384) != 0)) {
      setState(161);
      symbolOption();
      setState(166);
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
    setState(172);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__11:
      case TracerCommandsParser::T__19:
      case TracerCommandsParser::T__20:
      case TracerCommandsParser::T__21:
      case TracerCommandsParser::T__22: {
        enterOuterAlt(_localctx, 1);
        setState(167);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 15732736) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(168);
        value();
        break;
      }

      case TracerCommandsParser::T__23: {
        enterOuterAlt(_localctx, 2);
        setState(169);
        match(TracerCommandsParser::T__23);
        setState(170);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__24: {
        enterOuterAlt(_localctx, 3);
        setState(171);
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
        setState(174);
        match(TracerCommandsParser::T__25);
        setState(220);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__26: {
            setState(175);
            match(TracerCommandsParser::T__26);
            setState(176);
            value();
            setState(178);
            _errHandler->sync(this);
            _la = _input->LA(1);
            do {
              setState(177);
              prepareOption();
              setState(180);
              _errHandler->sync(this);
              _la = _input->LA(1);
            } while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 257699086336) != 0) || ((((_la - 107) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 107)) & 1827) != 0));
            break;
          }

          case TracerCommandsParser::T__27: {
            setState(182);
            match(TracerCommandsParser::T__27);
            setState(183);
            value();
            setState(188);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 2044405481472) != 0) || ((((_la - 107) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 107)) & 259) != 0)) {
              setState(186);
              _errHandler->sync(this);
              switch (_input->LA(1)) {
                case TracerCommandsParser::T__19:
                case TracerCommandsParser::T__33:
                case TracerCommandsParser::T__34:
                case TracerCommandsParser::T__35:
                case TracerCommandsParser::T__37:
                case TracerCommandsParser::T__38:
                case TracerCommandsParser::T__39:
                case TracerCommandsParser::T__106:
                case TracerCommandsParser::T__107: {
                  setState(184);
                  resolveOption();
                  break;
                }

                case TracerCommandsParser::CWD: {
                  setState(185);
                  cwdOption();
                  break;
                }

              default:
                throw NoViableAltException(this);
              }
              setState(190);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__28: {
            setState(191);
            match(TracerCommandsParser::T__28);
            setState(192);
            value();
            setState(195);
            _errHandler->sync(this);

            _la = _input->LA(1);
            if (_la == TracerCommandsParser::T__29) {
              setState(193);
              match(TracerCommandsParser::T__29);
              setState(194);
              value();
            }
            setState(201);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 17437568270336) != 0) || ((((_la - 104) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 104)) & 2073) != 0)) {
              setState(199);
              _errHandler->sync(this);
              switch (_input->LA(1)) {
                case TracerCommandsParser::T__19:
                case TracerCommandsParser::T__33:
                case TracerCommandsParser::T__34:
                case TracerCommandsParser::T__35:
                case TracerCommandsParser::T__37:
                case TracerCommandsParser::T__38:
                case TracerCommandsParser::T__39:
                case TracerCommandsParser::T__40:
                case TracerCommandsParser::T__41:
                case TracerCommandsParser::T__42:
                case TracerCommandsParser::T__103:
                case TracerCommandsParser::T__106:
                case TracerCommandsParser::T__107: {
                  setState(197);
                  deployOption();
                  break;
                }

                case TracerCommandsParser::CWD: {
                  setState(198);
                  cwdOption();
                  break;
                }

              default:
                throw NoViableAltException(this);
              }
              setState(203);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__30: {
            setState(204);
            match(TracerCommandsParser::T__30);
            setState(205);
            value();
            setState(206);
            match(TracerCommandsParser::T__29);
            setState(207);
            value();
            setState(212);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 2044405481472) != 0) || ((((_la - 107) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 107)) & 259) != 0)) {
              setState(210);
              _errHandler->sync(this);
              switch (_input->LA(1)) {
                case TracerCommandsParser::T__19:
                case TracerCommandsParser::T__33:
                case TracerCommandsParser::T__34:
                case TracerCommandsParser::T__35:
                case TracerCommandsParser::T__37:
                case TracerCommandsParser::T__38:
                case TracerCommandsParser::T__39:
                case TracerCommandsParser::T__106:
                case TracerCommandsParser::T__107: {
                  setState(208);
                  resolveOption();
                  break;
                }

                case TracerCommandsParser::CWD: {
                  setState(209);
                  cwdOption();
                  break;
                }

              default:
                throw NoViableAltException(this);
              }
              setState(214);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__31: {
            setState(215);
            match(TracerCommandsParser::T__31);
            setState(216);
            match(TracerCommandsParser::UINT);
            setState(218);
            _errHandler->sync(this);

            _la = _input->LA(1);
            if (_la == TracerCommandsParser::CWD) {
              setState(217);
              cwdOption();
            }
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
        setState(224);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::CWD) {
          setState(223);
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
    setState(236);
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

      case TracerCommandsParser::SIGNATURE: {
        enterOuterAlt(_localctx, 2);
        setState(230);
        match(TracerCommandsParser::SIGNATURE);
        setState(231);
        value();
        break;
      }

      case TracerCommandsParser::MEMBER: {
        enterOuterAlt(_localctx, 3);
        setState(232);
        match(TracerCommandsParser::MEMBER);
        break;
      }

      case TracerCommandsParser::METADATA_ONLY: {
        enterOuterAlt(_localctx, 4);
        setState(233);
        match(TracerCommandsParser::METADATA_ONLY);
        break;
      }

      case TracerCommandsParser::T__106:
      case TracerCommandsParser::T__107: {
        enterOuterAlt(_localctx, 5);
        setState(234);
        extraCompileOption();
        break;
      }

      case TracerCommandsParser::CWD: {
        enterOuterAlt(_localctx, 6);
        setState(235);
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
    setState(241);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__19:
      case TracerCommandsParser::T__33:
      case TracerCommandsParser::T__34:
      case TracerCommandsParser::T__35:
      case TracerCommandsParser::T__37:
      case TracerCommandsParser::T__38:
      case TracerCommandsParser::T__39: {
        enterOuterAlt(_localctx, 1);
        setState(238);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 2044405481472) != 0))) {
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

      case TracerCommandsParser::T__106:
      case TracerCommandsParser::T__107: {
        enterOuterAlt(_localctx, 2);
        setState(240);
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
  enterRule(_localctx, 34, TracerCommandsParser::RuleDeployOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(249);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__19:
      case TracerCommandsParser::T__33:
      case TracerCommandsParser::T__34:
      case TracerCommandsParser::T__35:
      case TracerCommandsParser::T__37:
      case TracerCommandsParser::T__38:
      case TracerCommandsParser::T__39:
      case TracerCommandsParser::T__106:
      case TracerCommandsParser::T__107: {
        enterOuterAlt(_localctx, 1);
        setState(243);
        resolveOption();
        break;
      }

      case TracerCommandsParser::T__40: {
        enterOuterAlt(_localctx, 2);
        setState(244);
        match(TracerCommandsParser::T__40);
        setState(245);
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

      case TracerCommandsParser::T__41: {
        enterOuterAlt(_localctx, 3);
        setState(246);
        match(TracerCommandsParser::T__41);
        break;
      }

      case TracerCommandsParser::T__42: {
        enterOuterAlt(_localctx, 4);
        setState(247);
        match(TracerCommandsParser::T__42);
        break;
      }

      case TracerCommandsParser::T__103: {
        enterOuterAlt(_localctx, 5);
        setState(248);
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
  enterRule(_localctx, 36, TracerCommandsParser::RuleInterceptor);
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
    setState(251);
    match(TracerCommandsParser::T__43);
    setState(281);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__44: {
        setState(252);
        match(TracerCommandsParser::T__44);
        setState(253);
        value();
        setState(254);
        match(TracerCommandsParser::T__45);
        setState(255);
        value();
        break;
      }

      case TracerCommandsParser::T__46: {
        setState(257);
        match(TracerCommandsParser::T__46);
        setState(258);
        value();
        break;
      }

      case TracerCommandsParser::T__47: {
        setState(259);
        match(TracerCommandsParser::T__47);
        setState(260);
        value();
        break;
      }

      case TracerCommandsParser::T__30: {
        setState(261);
        match(TracerCommandsParser::T__30);
        setState(262);
        value();
        setState(263);
        match(TracerCommandsParser::T__48);
        setState(264);
        value();
        setState(268);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 2044405481472) != 0) || _la == TracerCommandsParser::T__106

        || _la == TracerCommandsParser::T__107) {
          setState(265);
          resolveOption();
          setState(270);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__49: {
        setState(271);
        match(TracerCommandsParser::T__49);
        setState(272);
        value();
        setState(273);
        match(TracerCommandsParser::T__48);
        setState(274);
        value();
        setState(278);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 17437568270336) != 0) || ((((_la - 104) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 104)) & 25) != 0)) {
          setState(275);
          deployOption();
          setState(280);
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
  enterRule(_localctx, 38, TracerCommandsParser::RuleEvents);
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
    setState(283);
    match(TracerCommandsParser::T__50);
    setState(324);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::EOF:
      case TracerCommandsParser::T__45:
      case TracerCommandsParser::T__62:
      case TracerCommandsParser::T__63:
      case TracerCommandsParser::T__64:
      case TracerCommandsParser::T__65:
      case TracerCommandsParser::T__66:
      case TracerCommandsParser::T__69:
      case TracerCommandsParser::T__70:
      case TracerCommandsParser::T__71: {
        setState(287);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (((((_la - 46) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 46)) & 121503745) != 0)) {
          setState(284);
          streamOption();
          setState(289);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__51: {
        setState(290);
        match(TracerCommandsParser::T__51);
        setState(291);
        value();
        setState(294);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__23) {
          setState(292);
          match(TracerCommandsParser::T__23);
          setState(293);
          match(TracerCommandsParser::UINT);
        }
        break;
      }

      case TracerCommandsParser::T__47: {
        setState(296);
        match(TracerCommandsParser::T__47);
        setState(297);
        value();
        setState(298);
        match(TracerCommandsParser::T__52);
        setState(299);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::STACK: {
        setState(301);
        match(TracerCommandsParser::STACK);
        setState(302);
        value();
        setState(303);
        match(TracerCommandsParser::T__52);
        setState(304);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__53: {
        setState(306);
        match(TracerCommandsParser::T__53);
        setState(307);
        value();
        break;
      }

      case TracerCommandsParser::T__54: {
        setState(308);
        match(TracerCommandsParser::T__54);
        setState(309);
        value();
        break;
      }

      case TracerCommandsParser::T__55: {
        setState(310);
        match(TracerCommandsParser::T__55);
        setState(311);
        value();
        break;
      }

      case TracerCommandsParser::T__56: {
        setState(312);
        match(TracerCommandsParser::T__56);
        setState(313);
        value();
        setState(314);
        match(TracerCommandsParser::T__45);
        setState(315);
        value();
        break;
      }

      case TracerCommandsParser::T__57: {
        setState(317);
        match(TracerCommandsParser::T__57);
        setState(318);
        value();
        setState(319);
        match(TracerCommandsParser::T__58);
        setState(320);
        match(TracerCommandsParser::T__59);
        setState(321);
        match(TracerCommandsParser::T__36);
        setState(322);
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
  enterRule(_localctx, 40, TracerCommandsParser::RuleCollect);
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
    setState(326);
    match(TracerCommandsParser::T__60);
    setState(330);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (((((_la - 46) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 46)) & 121503745) != 0)) {
      setState(327);
      streamOption();
      setState(332);
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
  enterRule(_localctx, 42, TracerCommandsParser::RuleRecord);
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
    setState(333);
    match(TracerCommandsParser::T__61);
    setState(334);
    value();
    setState(339);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == TracerCommandsParser::T__45

    || _la == TracerCommandsParser::T__62 || ((((_la - 64) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 64)) & 562949953421775) != 0)) {
      setState(337);
      _errHandler->sync(this);
      switch (_input->LA(1)) {
        case TracerCommandsParser::T__45:
        case TracerCommandsParser::T__62:
        case TracerCommandsParser::T__63:
        case TracerCommandsParser::T__64:
        case TracerCommandsParser::T__65:
        case TracerCommandsParser::T__66:
        case TracerCommandsParser::T__69:
        case TracerCommandsParser::T__70:
        case TracerCommandsParser::T__71: {
          setState(335);
          streamOption();
          break;
        }

        case TracerCommandsParser::SQLITE: {
          setState(336);
          match(TracerCommandsParser::SQLITE);
          break;
        }

      default:
        throw NoViableAltException(this);
      }
      setState(341);
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
  enterRule(_localctx, 44, TracerCommandsParser::RuleStreamOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(349);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__62:
      case TracerCommandsParser::T__63:
      case TracerCommandsParser::T__64:
      case TracerCommandsParser::T__65: {
        enterOuterAlt(_localctx, 1);
        setState(342);
        _la = _input->LA(1);
        if (!(((((_la - 63) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 63)) & 15) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(343);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__66: {
        enterOuterAlt(_localctx, 2);
        setState(344);
        match(TracerCommandsParser::T__66);
        setState(345);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__67

        || _la == TracerCommandsParser::T__68)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        break;
      }

      case TracerCommandsParser::T__45:
      case TracerCommandsParser::T__69:
      case TracerCommandsParser::T__70: {
        enterOuterAlt(_localctx, 3);
        setState(346);
        _la = _input->LA(1);
        if (!(((((_la - 46) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 46)) & 50331649) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(347);
        value();
        break;
      }

      case TracerCommandsParser::T__71: {
        enterOuterAlt(_localctx, 4);
        setState(348);
        match(TracerCommandsParser::T__71);
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
  enterRule(_localctx, 46, TracerCommandsParser::RuleCpp);
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
    setState(351);
    match(TracerCommandsParser::T__72);
    setState(380);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__44: {
        setState(352);
        match(TracerCommandsParser::T__44);
        setState(353);
        value();
        break;
      }

      case TracerCommandsParser::T__73: {
        setState(354);
        match(TracerCommandsParser::T__73);
        break;
      }

      case TracerCommandsParser::T__74:
      case TracerCommandsParser::T__75: {
        setState(355);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__74

        || _la == TracerCommandsParser::T__75)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(356);
        value();
        setState(361);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__36

        || _la == TracerCommandsParser::T__76) {
          setState(357);
          _la = _input->LA(1);
          if (!(_la == TracerCommandsParser::T__36

          || _la == TracerCommandsParser::T__76)) {
          _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(358);
          value();
          setState(363);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__77: {
        setState(364);
        match(TracerCommandsParser::T__77);
        setState(378);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__78: {
            setState(365);
            match(TracerCommandsParser::T__78);
            setState(368);
            _errHandler->sync(this);
            _la = _input->LA(1);
            do {
              setState(368);
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
                case TracerCommandsParser::T__43:
                case TracerCommandsParser::T__44:
                case TracerCommandsParser::T__46:
                case TracerCommandsParser::T__47:
                case TracerCommandsParser::T__49:
                case TracerCommandsParser::T__50:
                case TracerCommandsParser::T__51:
                case TracerCommandsParser::T__53:
                case TracerCommandsParser::T__54:
                case TracerCommandsParser::T__55:
                case TracerCommandsParser::T__56:
                case TracerCommandsParser::T__57:
                case TracerCommandsParser::T__59:
                case TracerCommandsParser::T__60:
                case TracerCommandsParser::T__61:
                case TracerCommandsParser::T__67:
                case TracerCommandsParser::T__68:
                case TracerCommandsParser::T__72:
                case TracerCommandsParser::T__73:
                case TracerCommandsParser::T__74:
                case TracerCommandsParser::T__75:
                case TracerCommandsParser::T__77:
                case TracerCommandsParser::T__78:
                case TracerCommandsParser::T__79:
                case TracerCommandsParser::T__81:
                case TracerCommandsParser::T__82:
                case TracerCommandsParser::T__83:
                case TracerCommandsParser::T__84:
                case TracerCommandsParser::T__85:
                case TracerCommandsParser::T__86:
                case TracerCommandsParser::T__87:
                case TracerCommandsParser::T__88:
                case TracerCommandsParser::T__89:
                case TracerCommandsParser::T__94:
                case TracerCommandsParser::T__95:
                case TracerCommandsParser::T__96:
                case TracerCommandsParser::T__97:
                case TracerCommandsParser::T__98:
                case TracerCommandsParser::T__99:
                case TracerCommandsParser::UINT:
                case TracerCommandsParser::STRING:
                case TracerCommandsParser::STACK:
                case TracerCommandsParser::WORD: {
                  setState(366);
                  value();
                  break;
                }

                case TracerCommandsParser::SHELL: {
                  setState(367);
                  match(TracerCommandsParser::SHELL);
                  break;
                }

              default:
                throw NoViableAltException(this);
              }
              setState(370);
              _errHandler->sync(this);
              _la = _input->LA(1);
            } while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 8637253190452898428) != 0) || ((((_la - 68) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 68)) & 1211670277905891) != 0));
            break;
          }

          case TracerCommandsParser::T__55: {
            setState(372);
            match(TracerCommandsParser::T__55);
            break;
          }

          case TracerCommandsParser::T__79: {
            setState(373);
            match(TracerCommandsParser::T__79);
            setState(374);
            value();
            setState(375);
            match(TracerCommandsParser::T__80);
            setState(376);
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
  enterRule(_localctx, 48, TracerCommandsParser::RuleShell);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(382);
    match(TracerCommandsParser::T__81);
    setState(383);
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
  enterRule(_localctx, 50, TracerCommandsParser::RuleComplete);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(385);
    match(TracerCommandsParser::T__82);
    setState(386);
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
  enterRule(_localctx, 52, TracerCommandsParser::RuleType);

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
    match(TracerCommandsParser::T__83);
    setState(389);
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
  enterRule(_localctx, 54, TracerCommandsParser::RuleHelp);
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
    match(TracerCommandsParser::T__84);
    setState(400);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 8637253190452898430) != 0) || ((((_la - 68) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 68)) & 1202874184883683) != 0)) {
      setState(393);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == TracerCommandsParser::T__0) {
        setState(392);
        match(TracerCommandsParser::T__0);
      }
      setState(396);
      _errHandler->sync(this);
      _la = _input->LA(1);
      do {
        setState(395);
        value();
        setState(398);
        _errHandler->sync(this);
        _la = _input->LA(1);
      } while ((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & 8637253190452898428) != 0) || ((((_la - 68) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 68)) & 1202874184883683) != 0));
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
  enterRule(_localctx, 56, TracerCommandsParser::RuleQuit);
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
    setState(402);
    _la = _input->LA(1);
    if (!(_la == TracerCommandsParser::T__85

    || _la == TracerCommandsParser::T__86)) {
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
  enterRule(_localctx, 58, TracerCommandsParser::RuleBulkTrace);
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
    setState(404);
    match(TracerCommandsParser::T__87);
    setState(420);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__88: {
        setState(405);
        match(TracerCommandsParser::T__88);
        setState(407);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(406);
          bulkTraceOption();
          setState(409);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while (_la == TracerCommandsParser::T__19

        || _la == TracerCommandsParser::T__33 || ((((_la - 91) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 91)) & 15) != 0));
        break;
      }

      case TracerCommandsParser::T__89: {
        setState(411);
        match(TracerCommandsParser::T__89);
        setState(412);
        match(TracerCommandsParser::T__90);
        setState(413);
        value();
        setState(416);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__29) {
          setState(414);
          match(TracerCommandsParser::T__29);
          setState(415);
          value();
        }
        break;
      }

      case TracerCommandsParser::T__31: {
        setState(418);
        match(TracerCommandsParser::T__31);
        setState(419);
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
  enterRule(_localctx, 60, TracerCommandsParser::RuleBulkTraceOption);
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
    _la = _input->LA(1);
    if (!(_la == TracerCommandsParser::T__19

    || _la == TracerCommandsParser::T__33 || ((((_la - 91) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 91)) & 15) != 0))) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(423);
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
  enterRule(_localctx, 62, TracerCommandsParser::RuleValue);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(429);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::STRING: {
        enterOuterAlt(_localctx, 1);
        setState(425);
        match(TracerCommandsParser::STRING);
        break;
      }

      case TracerCommandsParser::WORD: {
        enterOuterAlt(_localctx, 2);
        setState(426);
        match(TracerCommandsParser::WORD);
        break;
      }

      case TracerCommandsParser::UINT: {
        enterOuterAlt(_localctx, 3);
        setState(427);
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
      case TracerCommandsParser::T__43:
      case TracerCommandsParser::T__44:
      case TracerCommandsParser::T__46:
      case TracerCommandsParser::T__47:
      case TracerCommandsParser::T__49:
      case TracerCommandsParser::T__50:
      case TracerCommandsParser::T__51:
      case TracerCommandsParser::T__53:
      case TracerCommandsParser::T__54:
      case TracerCommandsParser::T__55:
      case TracerCommandsParser::T__56:
      case TracerCommandsParser::T__57:
      case TracerCommandsParser::T__59:
      case TracerCommandsParser::T__60:
      case TracerCommandsParser::T__61:
      case TracerCommandsParser::T__67:
      case TracerCommandsParser::T__68:
      case TracerCommandsParser::T__72:
      case TracerCommandsParser::T__73:
      case TracerCommandsParser::T__74:
      case TracerCommandsParser::T__75:
      case TracerCommandsParser::T__77:
      case TracerCommandsParser::T__78:
      case TracerCommandsParser::T__79:
      case TracerCommandsParser::T__81:
      case TracerCommandsParser::T__82:
      case TracerCommandsParser::T__83:
      case TracerCommandsParser::T__84:
      case TracerCommandsParser::T__85:
      case TracerCommandsParser::T__86:
      case TracerCommandsParser::T__87:
      case TracerCommandsParser::T__88:
      case TracerCommandsParser::T__89:
      case TracerCommandsParser::T__94:
      case TracerCommandsParser::T__95:
      case TracerCommandsParser::T__96:
      case TracerCommandsParser::T__97:
      case TracerCommandsParser::T__98:
      case TracerCommandsParser::T__99:
      case TracerCommandsParser::STACK: {
        enterOuterAlt(_localctx, 4);
        setState(428);
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
  enterRule(_localctx, 64, TracerCommandsParser::RuleKeyword);
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
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 8637253190452898428) != 0) || ((((_la - 68) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 68)) & 70377208274403) != 0))) {
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
  enterRule(_localctx, 66, TracerCommandsParser::RuleMemory);
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
    match(TracerCommandsParser::T__94);
    setState(457);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__95: {
        setState(434);
        match(TracerCommandsParser::T__95);
        break;
      }

      case TracerCommandsParser::T__96: {
        setState(435);
        match(TracerCommandsParser::T__96);
        setState(436);
        value();
        setState(437);
        match(TracerCommandsParser::T__22);
        setState(438);
        value();
        setState(443);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__62: {
            setState(439);
            match(TracerCommandsParser::T__62);
            setState(440);
            match(TracerCommandsParser::UINT);
            break;
          }

          case TracerCommandsParser::T__76: {
            setState(441);
            match(TracerCommandsParser::T__76);
            setState(442);
            value();
            break;
          }

        default:
          throw NoViableAltException(this);
        }
        break;
      }

      case TracerCommandsParser::T__97: {
        setState(445);
        match(TracerCommandsParser::T__97);
        setState(446);
        value();
        setState(447);
        match(TracerCommandsParser::T__22);
        setState(448);
        value();
        setState(449);
        match(TracerCommandsParser::T__62);
        setState(450);
        match(TracerCommandsParser::UINT);
        setState(451);
        match(TracerCommandsParser::T__100);
        setState(452);
        value();
        setState(455);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__23) {
          setState(453);
          match(TracerCommandsParser::T__23);
          setState(454);
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
  enterRule(_localctx, 68, TracerCommandsParser::RuleProbeEntry);
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
    setState(459);
    match(TracerCommandsParser::T__25);
    setState(460);
    match(TracerCommandsParser::T__98);
    setState(477);
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
      case TracerCommandsParser::T__43:
      case TracerCommandsParser::T__44:
      case TracerCommandsParser::T__46:
      case TracerCommandsParser::T__47:
      case TracerCommandsParser::T__49:
      case TracerCommandsParser::T__50:
      case TracerCommandsParser::T__51:
      case TracerCommandsParser::T__53:
      case TracerCommandsParser::T__54:
      case TracerCommandsParser::T__55:
      case TracerCommandsParser::T__56:
      case TracerCommandsParser::T__57:
      case TracerCommandsParser::T__59:
      case TracerCommandsParser::T__60:
      case TracerCommandsParser::T__61:
      case TracerCommandsParser::T__67:
      case TracerCommandsParser::T__68:
      case TracerCommandsParser::T__72:
      case TracerCommandsParser::T__73:
      case TracerCommandsParser::T__74:
      case TracerCommandsParser::T__75:
      case TracerCommandsParser::T__77:
      case TracerCommandsParser::T__78:
      case TracerCommandsParser::T__79:
      case TracerCommandsParser::T__81:
      case TracerCommandsParser::T__82:
      case TracerCommandsParser::T__83:
      case TracerCommandsParser::T__84:
      case TracerCommandsParser::T__85:
      case TracerCommandsParser::T__86:
      case TracerCommandsParser::T__87:
      case TracerCommandsParser::T__88:
      case TracerCommandsParser::T__89:
      case TracerCommandsParser::T__94:
      case TracerCommandsParser::T__95:
      case TracerCommandsParser::T__96:
      case TracerCommandsParser::T__97:
      case TracerCommandsParser::T__98:
      case TracerCommandsParser::T__99:
      case TracerCommandsParser::UINT:
      case TracerCommandsParser::STRING:
      case TracerCommandsParser::STACK:
      case TracerCommandsParser::WORD: {
        setState(461);
        value();
        setState(462);
        match(TracerCommandsParser::T__22);
        setState(463);
        value();
        setState(466);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__19) {
          setState(464);
          match(TracerCommandsParser::T__19);
          setState(465);
          value();
        }
        break;
      }

      case TracerCommandsParser::T__19: {
        setState(468);
        match(TracerCommandsParser::T__19);
        setState(469);
        value();
        setState(470);
        match(TracerCommandsParser::T__101);
        setState(471);
        value();
        setState(472);
        match(TracerCommandsParser::T__102);
        setState(473);
        value();
        setState(474);
        match(TracerCommandsParser::T__22);
        setState(475);
        value();
        break;
      }

    default:
      throw NoViableAltException(this);
    }
    setState(480);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::CWD) {
      setState(479);
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
  enterRule(_localctx, 70, TracerCommandsParser::RuleProbeReload);
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
    match(TracerCommandsParser::T__25);
    setState(483);
    match(TracerCommandsParser::T__99);
    setState(484);
    match(TracerCommandsParser::UINT);
    setState(485);
    match(TracerCommandsParser::T__29);
    setState(486);
    value();
    setState(488);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::CWD) {
      setState(487);
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
  enterRule(_localctx, 72, TracerCommandsParser::RuleStackFilter);

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
    match(TracerCommandsParser::T__103);
    setState(491);
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
  enterRule(_localctx, 74, TracerCommandsParser::RuleContextEventBytes);
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
    setState(494);
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
  enterRule(_localctx, 76, TracerCommandsParser::RuleExtraCompileOption);
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
      case TracerCommandsParser::T__106: {
        enterOuterAlt(_localctx, 1);
        setState(496);
        match(TracerCommandsParser::T__106);
        setState(497);
        value();
        break;
      }

      case TracerCommandsParser::T__107: {
        enterOuterAlt(_localctx, 2);
        setState(498);
        match(TracerCommandsParser::T__107);
        setState(501);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(501);
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
            case TracerCommandsParser::T__43:
            case TracerCommandsParser::T__44:
            case TracerCommandsParser::T__46:
            case TracerCommandsParser::T__47:
            case TracerCommandsParser::T__49:
            case TracerCommandsParser::T__50:
            case TracerCommandsParser::T__51:
            case TracerCommandsParser::T__53:
            case TracerCommandsParser::T__54:
            case TracerCommandsParser::T__55:
            case TracerCommandsParser::T__56:
            case TracerCommandsParser::T__57:
            case TracerCommandsParser::T__59:
            case TracerCommandsParser::T__60:
            case TracerCommandsParser::T__61:
            case TracerCommandsParser::T__67:
            case TracerCommandsParser::T__68:
            case TracerCommandsParser::T__72:
            case TracerCommandsParser::T__73:
            case TracerCommandsParser::T__74:
            case TracerCommandsParser::T__75:
            case TracerCommandsParser::T__77:
            case TracerCommandsParser::T__78:
            case TracerCommandsParser::T__79:
            case TracerCommandsParser::T__81:
            case TracerCommandsParser::T__82:
            case TracerCommandsParser::T__83:
            case TracerCommandsParser::T__84:
            case TracerCommandsParser::T__85:
            case TracerCommandsParser::T__86:
            case TracerCommandsParser::T__87:
            case TracerCommandsParser::T__88:
            case TracerCommandsParser::T__89:
            case TracerCommandsParser::T__94:
            case TracerCommandsParser::T__95:
            case TracerCommandsParser::T__96:
            case TracerCommandsParser::T__97:
            case TracerCommandsParser::T__98:
            case TracerCommandsParser::T__99:
            case TracerCommandsParser::UINT:
            case TracerCommandsParser::STRING:
            case TracerCommandsParser::STACK:
            case TracerCommandsParser::WORD: {
              setState(499);
              value();
              break;
            }

            case TracerCommandsParser::SHELL: {
              setState(500);
              match(TracerCommandsParser::SHELL);
              break;
            }

          default:
            throw NoViableAltException(this);
          }
          setState(503);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 8637253190452898428) != 0) || ((((_la - 68) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 68)) & 1211670277905891) != 0));
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
  enterRule(_localctx, 78, TracerCommandsParser::RuleCwdOption);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(507);
    match(TracerCommandsParser::CWD);
    setState(508);
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
