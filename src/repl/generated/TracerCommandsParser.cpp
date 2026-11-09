
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
      "validateOption", "deployOption", "interceptor", "events", "collect",
      "record", "streamOption", "cpp", "shell", "complete", "type", "help",
      "quit", "bulkTrace", "bulkTraceOption", "value", "keyword", "memory",
      "probeEntry", "probeReload", "targetLaunch", "launchOption", "childList",
      "targetCall", "localPerf", "targetLibrary", "instructions", "instructionOption",
      "watchpoint", "stackFilter", "contextEventBytes", "extraCompileOption",
      "cwdOption"
    },
    std::vector<std::string>{
      "", "'%'", "'server'", "'start'", "'connect'", "'status'", "'stop'",
      "'--executable'", "'--agent'", "'attach'", "'--start-time'", "'processes'",
      "'--filter'", "'sessions'", "'use'", "'detach'", "'modules'", "'--details'",
      "'--elf'", "'--watch'", "'--timeout'", "'--count'", "'module'", "'index'",
      "'types'", "'symbols'", "'show'", "'emit'", "'--file'", "'--module'",
      "'--mapping'", "'--output'", "'--debug-file'", "'--max-entries'",
      "'--max-depth'", "'--index'", "'--page'", "'--limit'", "'--format'",
      "'json'", "'--exported'", "'--type'", "'--symbol'", "'--id'", "'threads'",
      "'--address'", "'--addresses'", "'probe'", "'prepare'", "'resolve'",
      "'inject'", "'--callback'", "'validate'", "'remove'", "'probes'",
      "'--include'", "'--compdb'", "'--translation-unit'", "'--module-index'",
      "'--generated'", "'--compile-option'", "'--prepared'", "'--prepared-mode'",
      "'--allow-arg-overrides'", "'--allow-result-override'", "'--allow-replacement'",
      "'--reclaimable'", "'--allow-call-original'", "'--trace-exceptions'",
      "'--stack-depth'", "'--print-arguments'", "'--print-result'", "'interceptor'",
      "'load'", "'--as'", "'edit'", "'--target'", "'upload'", "'events'",
      "'list'", "'calls'", "'stats'", "'clear'", "'open'", "'export'", "'collect'",
      "'record'", "'--after'", "'--probe'", "'--phase'", "'PRE'", "'POST'",
      "'EXCEPTION'", "'--name'", "'--session'", "'--all'", "'cpp'", "'reset'",
      "'import'", "'generate'", "'options'", "'add'", "'compdb'", "'shell'",
      "'complete'", "'type'", "'help'", "'quit'", "'exit'", "'trace'", "'preview'",
      "'apply'", "'--manifest'", "'--exclude'", "'--exact'", "'--save'",
      "'memory'", "'maps'", "'read'", "'scan'", "'patch'", "'restore'",
      "'import-hook'", "'monitor-start'", "'monitor-stop'", "'entry'", "'reload'",
      "'launch'", "'resume'", "'terminate'", "'call'", "'call-result'",
      "'syscalls'", "'sample'", "'cpu'", "'instructions'", "'watchpoint'",
      "'library'", "'allocate'", "'free'", "'write'", "'replay'", "'children'",
      "'faults'", "'allocations'", "'--pattern'", "'--elf-address'", "'--expect'",
      "'--replace'", "'--tid'", "'--size'", "'--original'", "'--abi'", "'--arg'",
      "'--env'", "'--stdin'", "'--stdout'", "'--stderr'", "'--preload'",
      "'--supervise'", "'--gate-children'", "'--memory-size'", "'--registers'",
      "'--discover'", "'--callback-library'", "'--memory-address'", "'--access'",
      "'--stack-application-only'", "'--context-event-bytes'", "'--context-transport-memory-bytes'",
      "'--extra-arg'", "'--extra-args'", "", "", "", "'--metadata-only'",
      "'--sqlite'", "'stack'", "'--cwd'", "'--signature'", "'--member'"
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
      "", "", "UINT", "STRING", "SHELL", "METADATA_ONLY", "SQLITE", "STACK",
      "CWD", "SIGNATURE", "MEMBER", "WORD", "WS"
    }
  );
  static const int32_t serializedATNSegment[] = {
    4,1,182,786,2,0,7,0,2,1,7,1,2,2,7,2,2,3,7,3,2,4,7,4,2,5,7,5,2,6,7,6,2,
    7,7,7,2,8,7,8,2,9,7,9,2,10,7,10,2,11,7,11,2,12,7,12,2,13,7,13,2,14,7,
    14,2,15,7,15,2,16,7,16,2,17,7,17,2,18,7,18,2,19,7,19,2,20,7,20,2,21,7,
    21,2,22,7,22,2,23,7,23,2,24,7,24,2,25,7,25,2,26,7,26,2,27,7,27,2,28,7,
    28,2,29,7,29,2,30,7,30,2,31,7,31,2,32,7,32,2,33,7,33,2,34,7,34,2,35,7,
    35,2,36,7,36,2,37,7,37,2,38,7,38,2,39,7,39,2,40,7,40,2,41,7,41,2,42,7,
    42,2,43,7,43,2,44,7,44,2,45,7,45,2,46,7,46,2,47,7,47,2,48,7,48,2,49,7,
    49,2,50,7,50,2,51,7,51,2,52,7,52,2,53,7,53,2,54,7,54,1,0,1,0,1,0,1,0,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,3,1,148,
    8,1,1,2,1,2,1,2,5,2,153,8,2,10,2,12,2,156,9,2,1,2,1,2,1,2,1,2,3,2,162,
    8,2,1,3,1,3,1,3,3,3,167,8,3,1,4,1,4,1,4,1,4,3,4,173,8,4,3,4,175,8,4,1,
    5,1,5,1,5,3,5,180,8,5,1,6,1,6,1,7,1,7,1,7,1,8,1,8,1,9,1,9,3,9,191,8,9,
    1,10,1,10,1,10,1,10,5,10,197,8,10,10,10,12,10,200,9,10,1,10,1,10,1,10,
    1,10,1,10,5,10,207,8,10,10,10,12,10,210,9,10,3,10,212,8,10,1,11,1,11,
    1,11,4,11,217,8,11,11,11,12,11,218,1,11,1,11,1,11,1,11,5,11,225,8,11,
    10,11,12,11,228,9,11,1,11,1,11,4,11,232,8,11,11,11,12,11,233,1,11,1,11,
    4,11,238,8,11,11,11,12,11,239,3,11,242,8,11,1,12,1,12,1,12,1,12,3,12,
    248,8,12,1,13,1,13,1,13,1,13,1,13,1,13,1,13,3,13,257,8,13,1,14,1,14,1,
    14,1,15,1,15,1,15,3,15,265,8,15,1,16,1,16,1,17,1,17,5,17,271,8,17,10,
    17,12,17,274,9,17,1,18,1,18,1,18,1,18,1,18,3,18,281,8,18,1,19,1,19,1,
    19,1,19,4,19,287,8,19,11,19,12,19,288,1,19,1,19,1,19,1,19,5,19,295,8,
    19,10,19,12,19,298,9,19,1,19,1,19,1,19,1,19,3,19,304,8,19,1,19,1,19,5,
    19,308,8,19,10,19,12,19,311,9,19,1,19,1,19,1,19,1,19,1,19,1,19,5,19,319,
    8,19,10,19,12,19,322,9,19,1,19,1,19,1,19,3,19,327,8,19,3,19,329,8,19,
    1,19,1,19,3,19,333,8,19,3,19,335,8,19,1,20,1,20,1,20,1,20,1,20,1,20,1,
    20,1,20,3,20,345,8,20,1,21,1,21,1,21,3,21,350,8,21,1,22,1,22,1,22,1,22,
    1,22,1,22,1,22,3,22,359,8,22,1,23,1,23,1,23,1,23,1,23,1,23,3,23,367,8,
    23,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,
    24,1,24,5,24,384,8,24,10,24,12,24,387,9,24,1,24,1,24,1,24,1,24,1,24,5,
    24,394,8,24,10,24,12,24,397,9,24,3,24,399,8,24,1,25,1,25,5,25,403,8,25,
    10,25,12,25,406,9,25,1,25,1,25,1,25,1,25,3,25,412,8,25,1,25,1,25,1,25,
    1,25,1,25,1,25,1,25,1,25,1,25,1,25,1,25,1,25,1,25,1,25,1,25,1,25,1,25,
    1,25,1,25,1,25,1,25,1,25,1,25,1,25,1,25,1,25,1,25,1,25,3,25,442,8,25,
    1,26,1,26,5,26,446,8,26,10,26,12,26,449,9,26,1,27,1,27,1,27,1,27,5,27,
    455,8,27,10,27,12,27,458,9,27,1,28,1,28,1,28,1,28,1,28,1,28,1,28,3,28,
    467,8,28,1,29,1,29,1,29,1,29,1,29,1,29,1,29,1,29,5,29,477,8,29,10,29,
    12,29,480,9,29,1,29,1,29,1,29,1,29,4,29,486,8,29,11,29,12,29,487,1,29,
    1,29,1,29,1,29,1,29,1,29,3,29,496,8,29,3,29,498,8,29,1,30,1,30,1,30,1,
    31,1,31,1,31,1,32,1,32,1,32,1,33,1,33,3,33,511,8,33,1,33,4,33,514,8,33,
    11,33,12,33,515,3,33,518,8,33,1,34,1,34,1,35,1,35,1,35,4,35,525,8,35,
    11,35,12,35,526,1,35,1,35,1,35,1,35,1,35,3,35,534,8,35,1,35,1,35,3,35,
    538,8,35,1,36,1,36,1,36,1,37,1,37,1,37,1,37,3,37,547,8,37,1,38,1,38,1,
    39,1,39,1,39,1,39,1,39,1,39,1,39,1,39,1,39,1,39,3,39,561,8,39,1,39,1,
    39,1,39,1,39,1,39,1,39,1,39,1,39,1,39,1,39,3,39,573,8,39,1,39,1,39,1,
    39,1,39,1,39,1,39,1,39,1,39,1,39,1,39,1,39,1,39,1,39,1,39,1,39,1,39,1,
    39,1,39,1,39,1,39,1,39,1,39,1,39,1,39,1,39,1,39,1,39,1,39,1,39,1,39,1,
    39,1,39,1,39,1,39,3,39,609,8,39,1,39,1,39,1,39,1,39,1,39,1,39,1,39,1,
    39,1,39,1,39,1,39,1,39,1,39,1,39,1,39,3,39,626,8,39,1,40,1,40,1,40,1,
    40,1,40,1,40,1,40,3,40,635,8,40,1,40,1,40,1,40,1,40,1,40,1,40,1,40,1,
    40,1,40,3,40,646,8,40,1,40,3,40,649,8,40,1,41,1,41,1,41,1,41,1,41,1,41,
    1,41,1,41,1,41,5,41,660,8,41,10,41,12,41,663,9,41,5,41,665,8,41,10,41,
    12,41,668,9,41,1,42,1,42,1,42,5,42,673,8,42,10,42,12,42,676,9,42,1,42,
    1,42,3,42,680,8,42,3,42,682,8,42,1,43,1,43,1,43,1,43,1,43,1,43,3,43,690,
    8,43,1,44,1,44,1,45,1,45,1,45,1,45,1,45,1,45,5,45,700,8,45,10,45,12,45,
    703,9,45,1,45,1,45,1,45,3,45,708,8,45,3,45,710,8,45,1,46,1,46,1,46,1,
    46,3,46,716,8,46,1,46,1,46,5,46,720,8,46,10,46,12,46,723,9,46,1,47,1,
    47,1,47,1,47,1,48,1,48,1,48,5,48,732,8,48,10,48,12,48,735,9,48,1,48,1,
    48,1,48,3,48,740,8,48,1,48,1,48,3,48,744,8,48,1,49,1,49,1,49,1,49,1,49,
    1,49,1,49,1,49,3,49,754,8,49,1,50,1,50,1,50,1,50,1,50,5,50,761,8,50,10,
    50,12,50,764,9,50,1,51,1,51,1,51,1,52,1,52,1,52,1,53,1,53,1,53,1,53,1,
    53,4,53,777,8,53,11,53,12,53,778,3,53,781,8,53,1,54,1,54,1,54,1,54,0,
    0,55,0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,34,36,38,40,42,44,
    46,48,50,52,54,56,58,60,62,64,66,68,70,72,74,76,78,80,82,84,86,88,90,
    92,94,96,98,100,102,104,106,108,0,27,1,0,7,8,1,0,20,21,1,0,24,25,1,0,
    28,32,1,0,33,34,2,0,12,12,35,36,2,0,35,35,41,43,4,0,12,12,29,30,36,36,
    45,45,4,0,29,29,31,31,43,43,55,59,3,0,29,29,55,57,60,62,2,0,172,172,181,
    181,2,0,20,21,87,88,1,0,90,92,2,0,74,74,93,94,1,0,98,99,2,0,31,31,41,
    41,1,0,107,108,3,0,29,29,55,55,112,115,16,0,2,6,9,9,11,11,13,16,22,27,
    39,39,44,44,47,50,52,54,72,73,75,75,77,86,90,92,96,111,116,144,177,177,
    2,0,121,121,138,139,1,0,128,129,2,0,153,158,178,178,2,0,134,134,144,144,
    2,0,20,21,149,150,3,0,30,30,45,45,164,165,3,0,30,30,45,45,166,166,1,0,
    168,169,907,0,110,1,0,0,0,2,147,1,0,0,0,4,149,1,0,0,0,6,166,1,0,0,0,8,
    168,1,0,0,0,10,176,1,0,0,0,12,181,1,0,0,0,14,183,1,0,0,0,16,186,1,0,0,
    0,18,188,1,0,0,0,20,192,1,0,0,0,22,213,1,0,0,0,24,247,1,0,0,0,26,256,
    1,0,0,0,28,258,1,0,0,0,30,264,1,0,0,0,32,266,1,0,0,0,34,268,1,0,0,0,36,
    280,1,0,0,0,38,334,1,0,0,0,40,344,1,0,0,0,42,349,1,0,0,0,44,358,1,0,0,
    0,46,366,1,0,0,0,48,368,1,0,0,0,50,400,1,0,0,0,52,443,1,0,0,0,54,450,
    1,0,0,0,56,466,1,0,0,0,58,468,1,0,0,0,60,499,1,0,0,0,62,502,1,0,0,0,64,
    505,1,0,0,0,66,508,1,0,0,0,68,519,1,0,0,0,70,521,1,0,0,0,72,539,1,0,0,
    0,74,546,1,0,0,0,76,548,1,0,0,0,78,550,1,0,0,0,80,627,1,0,0,0,82,650,
    1,0,0,0,84,681,1,0,0,0,86,689,1,0,0,0,88,691,1,0,0,0,90,709,1,0,0,0,92,
    715,1,0,0,0,94,724,1,0,0,0,96,728,1,0,0,0,98,753,1,0,0,0,100,755,1,0,
    0,0,102,765,1,0,0,0,104,768,1,0,0,0,106,780,1,0,0,0,108,782,1,0,0,0,110,
    111,5,1,0,0,111,112,3,2,1,0,112,113,5,0,0,1,113,1,1,0,0,0,114,148,3,4,
    2,0,115,148,3,8,4,0,116,148,3,10,5,0,117,148,3,12,6,0,118,148,3,14,7,
    0,119,148,3,16,8,0,120,148,3,18,9,0,121,148,3,20,10,0,122,148,3,22,11,
    0,123,148,3,32,16,0,124,148,3,34,17,0,125,148,3,38,19,0,126,148,3,48,
    24,0,127,148,3,50,25,0,128,148,3,52,26,0,129,148,3,54,27,0,130,148,3,
    58,29,0,131,148,3,60,30,0,132,148,3,62,31,0,133,148,3,64,32,0,134,148,
    3,66,33,0,135,148,3,68,34,0,136,148,3,70,35,0,137,148,3,78,39,0,138,148,
    3,80,40,0,139,148,3,82,41,0,140,148,3,84,42,0,141,148,3,90,45,0,142,148,
    3,92,46,0,143,148,3,96,48,0,144,148,3,100,50,0,145,148,3,94,47,0,146,
    148,3,88,44,0,147,114,1,0,0,0,147,115,1,0,0,0,147,116,1,0,0,0,147,117,
    1,0,0,0,147,118,1,0,0,0,147,119,1,0,0,0,147,120,1,0,0,0,147,121,1,0,0,
    0,147,122,1,0,0,0,147,123,1,0,0,0,147,124,1,0,0,0,147,125,1,0,0,0,147,
    126,1,0,0,0,147,127,1,0,0,0,147,128,1,0,0,0,147,129,1,0,0,0,147,130,1,
    0,0,0,147,131,1,0,0,0,147,132,1,0,0,0,147,133,1,0,0,0,147,134,1,0,0,0,
    147,135,1,0,0,0,147,136,1,0,0,0,147,137,1,0,0,0,147,138,1,0,0,0,147,139,
    1,0,0,0,147,140,1,0,0,0,147,141,1,0,0,0,147,142,1,0,0,0,147,143,1,0,0,
    0,147,144,1,0,0,0,147,145,1,0,0,0,147,146,1,0,0,0,148,3,1,0,0,0,149,161,
    5,2,0,0,150,154,5,3,0,0,151,153,3,6,3,0,152,151,1,0,0,0,153,156,1,0,0,
    0,154,152,1,0,0,0,154,155,1,0,0,0,155,162,1,0,0,0,156,154,1,0,0,0,157,
    158,5,4,0,0,158,162,3,74,37,0,159,162,5,5,0,0,160,162,5,6,0,0,161,150,
    1,0,0,0,161,157,1,0,0,0,161,159,1,0,0,0,161,160,1,0,0,0,162,5,1,0,0,0,
    163,164,7,0,0,0,164,167,3,74,37,0,165,167,3,104,52,0,166,163,1,0,0,0,
    166,165,1,0,0,0,167,7,1,0,0,0,168,174,5,9,0,0,169,172,5,172,0,0,170,171,
    5,10,0,0,171,173,5,172,0,0,172,170,1,0,0,0,172,173,1,0,0,0,173,175,1,
    0,0,0,174,169,1,0,0,0,174,175,1,0,0,0,175,9,1,0,0,0,176,179,5,11,0,0,
    177,178,5,12,0,0,178,180,3,74,37,0,179,177,1,0,0,0,179,180,1,0,0,0,180,
    11,1,0,0,0,181,182,5,13,0,0,182,13,1,0,0,0,183,184,5,14,0,0,184,185,3,
    74,37,0,185,15,1,0,0,0,186,187,5,5,0,0,187,17,1,0,0,0,188,190,5,15,0,
    0,189,191,3,74,37,0,190,189,1,0,0,0,190,191,1,0,0,0,191,19,1,0,0,0,192,
    211,5,16,0,0,193,197,5,17,0,0,194,195,5,12,0,0,195,197,3,74,37,0,196,
    193,1,0,0,0,196,194,1,0,0,0,197,200,1,0,0,0,198,196,1,0,0,0,198,199,1,
    0,0,0,199,212,1,0,0,0,200,198,1,0,0,0,201,202,5,18,0,0,202,212,3,74,37,
    0,203,208,5,19,0,0,204,205,7,1,0,0,205,207,5,172,0,0,206,204,1,0,0,0,
    207,210,1,0,0,0,208,206,1,0,0,0,208,209,1,0,0,0,209,212,1,0,0,0,210,208,
    1,0,0,0,211,198,1,0,0,0,211,201,1,0,0,0,211,203,1,0,0,0,212,21,1,0,0,
    0,213,241,5,22,0,0,214,216,5,23,0,0,215,217,3,24,12,0,216,215,1,0,0,0,
    217,218,1,0,0,0,218,216,1,0,0,0,218,219,1,0,0,0,219,242,1,0,0,0,220,221,
    5,14,0,0,221,242,3,74,37,0,222,226,7,2,0,0,223,225,3,26,13,0,224,223,
    1,0,0,0,225,228,1,0,0,0,226,224,1,0,0,0,226,227,1,0,0,0,227,242,1,0,0,
    0,228,226,1,0,0,0,229,231,5,26,0,0,230,232,3,28,14,0,231,230,1,0,0,0,
    232,233,1,0,0,0,233,231,1,0,0,0,233,234,1,0,0,0,234,242,1,0,0,0,235,237,
    5,27,0,0,236,238,3,30,15,0,237,236,1,0,0,0,238,239,1,0,0,0,239,237,1,
    0,0,0,239,240,1,0,0,0,240,242,1,0,0,0,241,214,1,0,0,0,241,220,1,0,0,0,
    241,222,1,0,0,0,241,229,1,0,0,0,241,235,1,0,0,0,242,23,1,0,0,0,243,244,
    7,3,0,0,244,248,3,74,37,0,245,246,7,4,0,0,246,248,5,172,0,0,247,243,1,
    0,0,0,247,245,1,0,0,0,248,25,1,0,0,0,249,250,7,5,0,0,250,257,3,74,37,
    0,251,252,5,37,0,0,252,257,5,172,0,0,253,254,5,38,0,0,254,257,5,39,0,
    0,255,257,5,40,0,0,256,249,1,0,0,0,256,251,1,0,0,0,256,253,1,0,0,0,256,
    255,1,0,0,0,257,27,1,0,0,0,258,259,7,6,0,0,259,260,3,74,37,0,260,29,1,
    0,0,0,261,265,3,28,14,0,262,263,5,31,0,0,263,265,3,74,37,0,264,261,1,
    0,0,0,264,262,1,0,0,0,265,31,1,0,0,0,266,267,5,44,0,0,267,33,1,0,0,0,
    268,272,5,25,0,0,269,271,3,36,18,0,270,269,1,0,0,0,271,274,1,0,0,0,272,
    270,1,0,0,0,272,273,1,0,0,0,273,35,1,0,0,0,274,272,1,0,0,0,275,276,7,
    7,0,0,276,281,3,74,37,0,277,278,5,37,0,0,278,281,5,172,0,0,279,281,5,
    46,0,0,280,275,1,0,0,0,280,277,1,0,0,0,280,279,1,0,0,0,281,37,1,0,0,0,
    282,328,5,47,0,0,283,284,5,48,0,0,284,286,3,74,37,0,285,287,3,40,20,0,
    286,285,1,0,0,0,287,288,1,0,0,0,288,286,1,0,0,0,288,289,1,0,0,0,289,329,
    1,0,0,0,290,291,5,49,0,0,291,296,3,74,37,0,292,295,3,42,21,0,293,295,
    3,108,54,0,294,292,1,0,0,0,294,293,1,0,0,0,295,298,1,0,0,0,296,294,1,
    0,0,0,296,297,1,0,0,0,297,329,1,0,0,0,298,296,1,0,0,0,299,300,5,50,0,
    0,300,303,3,74,37,0,301,302,5,51,0,0,302,304,3,74,37,0,303,301,1,0,0,
    0,303,304,1,0,0,0,304,309,1,0,0,0,305,308,3,46,23,0,306,308,3,108,54,
    0,307,305,1,0,0,0,307,306,1,0,0,0,308,311,1,0,0,0,309,307,1,0,0,0,309,
    310,1,0,0,0,310,329,1,0,0,0,311,309,1,0,0,0,312,313,5,52,0,0,313,314,
    3,74,37,0,314,315,5,51,0,0,315,320,3,74,37,0,316,319,3,44,22,0,317,319,
    3,108,54,0,318,316,1,0,0,0,318,317,1,0,0,0,319,322,1,0,0,0,320,318,1,
    0,0,0,320,321,1,0,0,0,321,329,1,0,0,0,322,320,1,0,0,0,323,324,5,53,0,
    0,324,326,5,172,0,0,325,327,3,108,54,0,326,325,1,0,0,0,326,327,1,0,0,
    0,327,329,1,0,0,0,328,283,1,0,0,0,328,290,1,0,0,0,328,299,1,0,0,0,328,
    312,1,0,0,0,328,323,1,0,0,0,329,335,1,0,0,0,330,332,5,54,0,0,331,333,
    3,108,54,0,332,331,1,0,0,0,332,333,1,0,0,0,333,335,1,0,0,0,334,282,1,
    0,0,0,334,330,1,0,0,0,335,39,1,0,0,0,336,337,7,8,0,0,337,345,3,74,37,
    0,338,339,5,179,0,0,339,345,3,74,37,0,340,345,5,180,0,0,341,345,5,175,
    0,0,342,345,3,106,53,0,343,345,3,108,54,0,344,336,1,0,0,0,344,338,1,0,
    0,0,344,340,1,0,0,0,344,341,1,0,0,0,344,342,1,0,0,0,344,343,1,0,0,0,345,
    41,1,0,0,0,346,347,7,9,0,0,347,350,3,74,37,0,348,350,3,106,53,0,349,346,
    1,0,0,0,349,348,1,0,0,0,350,43,1,0,0,0,351,359,3,42,21,0,352,359,5,63,
    0,0,353,359,5,64,0,0,354,359,5,65,0,0,355,359,5,66,0,0,356,359,5,67,0,
    0,357,359,5,68,0,0,358,351,1,0,0,0,358,352,1,0,0,0,358,353,1,0,0,0,358,
    354,1,0,0,0,358,355,1,0,0,0,358,356,1,0,0,0,358,357,1,0,0,0,359,45,1,
    0,0,0,360,367,3,44,22,0,361,362,5,69,0,0,362,367,7,10,0,0,363,367,5,70,
    0,0,364,367,5,71,0,0,365,367,3,102,51,0,366,360,1,0,0,0,366,361,1,0,0,
    0,366,363,1,0,0,0,366,364,1,0,0,0,366,365,1,0,0,0,367,47,1,0,0,0,368,
    398,5,72,0,0,369,370,5,73,0,0,370,371,3,74,37,0,371,372,5,74,0,0,372,
    373,3,74,37,0,373,399,1,0,0,0,374,375,5,75,0,0,375,399,3,74,37,0,376,
    377,5,26,0,0,377,399,3,74,37,0,378,379,5,52,0,0,379,380,3,74,37,0,380,
    381,5,76,0,0,381,385,3,74,37,0,382,384,3,44,22,0,383,382,1,0,0,0,384,
    387,1,0,0,0,385,383,1,0,0,0,385,386,1,0,0,0,386,399,1,0,0,0,387,385,1,
    0,0,0,388,389,5,77,0,0,389,390,3,74,37,0,390,391,5,76,0,0,391,395,3,74,
    37,0,392,394,3,46,23,0,393,392,1,0,0,0,394,397,1,0,0,0,395,393,1,0,0,
    0,395,396,1,0,0,0,396,399,1,0,0,0,397,395,1,0,0,0,398,369,1,0,0,0,398,
    374,1,0,0,0,398,376,1,0,0,0,398,378,1,0,0,0,398,388,1,0,0,0,399,49,1,
    0,0,0,400,441,5,78,0,0,401,403,3,56,28,0,402,401,1,0,0,0,403,406,1,0,
    0,0,404,402,1,0,0,0,404,405,1,0,0,0,405,442,1,0,0,0,406,404,1,0,0,0,407,
    408,5,79,0,0,408,411,3,74,37,0,409,410,5,37,0,0,410,412,5,172,0,0,411,
    409,1,0,0,0,411,412,1,0,0,0,412,442,1,0,0,0,413,414,5,26,0,0,414,415,
    3,74,37,0,415,416,5,35,0,0,416,417,5,172,0,0,417,442,1,0,0,0,418,419,
    5,177,0,0,419,420,3,74,37,0,420,421,5,35,0,0,421,422,5,172,0,0,422,442,
    1,0,0,0,423,424,5,80,0,0,424,442,3,74,37,0,425,426,5,81,0,0,426,442,3,
    74,37,0,427,428,5,82,0,0,428,442,3,74,37,0,429,430,5,83,0,0,430,431,3,
    74,37,0,431,432,5,74,0,0,432,433,3,74,37,0,433,442,1,0,0,0,434,435,5,
    84,0,0,435,436,3,74,37,0,436,437,5,38,0,0,437,438,5,39,0,0,438,439,5,
    31,0,0,439,440,3,74,37,0,440,442,1,0,0,0,441,404,1,0,0,0,441,407,1,0,
    0,0,441,413,1,0,0,0,441,418,1,0,0,0,441,423,1,0,0,0,441,425,1,0,0,0,441,
    427,1,0,0,0,441,429,1,0,0,0,441,434,1,0,0,0,442,51,1,0,0,0,443,447,5,
    85,0,0,444,446,3,56,28,0,445,444,1,0,0,0,446,449,1,0,0,0,447,445,1,0,
    0,0,447,448,1,0,0,0,448,53,1,0,0,0,449,447,1,0,0,0,450,451,5,86,0,0,451,
    456,3,74,37,0,452,455,3,56,28,0,453,455,5,176,0,0,454,452,1,0,0,0,454,
    453,1,0,0,0,455,458,1,0,0,0,456,454,1,0,0,0,456,457,1,0,0,0,457,55,1,
    0,0,0,458,456,1,0,0,0,459,460,7,11,0,0,460,467,5,172,0,0,461,462,5,89,
    0,0,462,467,7,12,0,0,463,464,7,13,0,0,464,467,3,74,37,0,465,467,5,95,
    0,0,466,459,1,0,0,0,466,461,1,0,0,0,466,463,1,0,0,0,466,465,1,0,0,0,467,
    57,1,0,0,0,468,497,5,96,0,0,469,470,5,73,0,0,470,498,3,74,37,0,471,498,
    5,97,0,0,472,473,7,14,0,0,473,478,3,74,37,0,474,475,7,15,0,0,475,477,
    3,74,37,0,476,474,1,0,0,0,477,480,1,0,0,0,478,476,1,0,0,0,478,479,1,0,
    0,0,479,498,1,0,0,0,480,478,1,0,0,0,481,495,5,100,0,0,482,485,5,101,0,
    0,483,486,3,74,37,0,484,486,5,174,0,0,485,483,1,0,0,0,485,484,1,0,0,0,
    486,487,1,0,0,0,487,485,1,0,0,0,487,488,1,0,0,0,488,496,1,0,0,0,489,496,
    5,82,0,0,490,491,5,102,0,0,491,492,3,74,37,0,492,493,5,28,0,0,493,494,
    3,74,37,0,494,496,1,0,0,0,495,482,1,0,0,0,495,489,1,0,0,0,495,490,1,0,
    0,0,495,496,1,0,0,0,496,498,1,0,0,0,497,469,1,0,0,0,497,471,1,0,0,0,497,
    472,1,0,0,0,497,481,1,0,0,0,498,59,1,0,0,0,499,500,5,103,0,0,500,501,
    3,74,37,0,501,61,1,0,0,0,502,503,5,104,0,0,503,504,3,74,37,0,504,63,1,
    0,0,0,505,506,5,105,0,0,506,507,3,74,37,0,507,65,1,0,0,0,508,517,5,106,
    0,0,509,511,5,1,0,0,510,509,1,0,0,0,510,511,1,0,0,0,511,513,1,0,0,0,512,
    514,3,74,37,0,513,512,1,0,0,0,514,515,1,0,0,0,515,513,1,0,0,0,515,516,
    1,0,0,0,516,518,1,0,0,0,517,510,1,0,0,0,517,518,1,0,0,0,518,67,1,0,0,
    0,519,520,7,16,0,0,520,69,1,0,0,0,521,537,5,109,0,0,522,524,5,110,0,0,
    523,525,3,72,36,0,524,523,1,0,0,0,525,526,1,0,0,0,526,524,1,0,0,0,526,
    527,1,0,0,0,527,538,1,0,0,0,528,529,5,111,0,0,529,530,5,112,0,0,530,533,
    3,74,37,0,531,532,5,51,0,0,532,534,3,74,37,0,533,531,1,0,0,0,533,534,
    1,0,0,0,534,538,1,0,0,0,535,536,5,53,0,0,536,538,3,74,37,0,537,522,1,
    0,0,0,537,528,1,0,0,0,537,535,1,0,0,0,538,71,1,0,0,0,539,540,7,17,0,0,
    540,541,3,74,37,0,541,73,1,0,0,0,542,547,5,173,0,0,543,547,5,181,0,0,
    544,547,5,172,0,0,545,547,3,76,38,0,546,542,1,0,0,0,546,543,1,0,0,0,546,
    544,1,0,0,0,546,545,1,0,0,0,547,75,1,0,0,0,548,549,7,18,0,0,549,77,1,
    0,0,0,550,625,5,116,0,0,551,626,5,117,0,0,552,553,5,118,0,0,553,554,3,
    74,37,0,554,555,5,30,0,0,555,560,3,74,37,0,556,557,5,21,0,0,557,561,5,
    172,0,0,558,559,5,41,0,0,559,561,3,74,37,0,560,556,1,0,0,0,560,558,1,
    0,0,0,561,626,1,0,0,0,562,563,5,119,0,0,563,564,3,74,37,0,564,565,5,30,
    0,0,565,566,3,74,37,0,566,567,5,21,0,0,567,568,5,172,0,0,568,569,5,145,
    0,0,569,572,3,74,37,0,570,571,5,37,0,0,571,573,5,172,0,0,572,570,1,0,
    0,0,572,573,1,0,0,0,573,626,1,0,0,0,574,575,5,120,0,0,575,576,5,29,0,
    0,576,577,3,74,37,0,577,578,5,146,0,0,578,579,3,74,37,0,579,580,5,30,
    0,0,580,581,3,74,37,0,581,582,5,147,0,0,582,583,3,74,37,0,583,584,5,148,
    0,0,584,585,3,74,37,0,585,626,1,0,0,0,586,587,5,140,0,0,587,588,3,74,
    37,0,588,589,5,30,0,0,589,590,3,74,37,0,590,591,5,147,0,0,591,592,3,74,
    37,0,592,593,5,148,0,0,593,594,3,74,37,0,594,626,1,0,0,0,595,596,7,19,
    0,0,596,626,5,172,0,0,597,598,5,123,0,0,598,599,5,149,0,0,599,600,5,172,
    0,0,600,601,5,45,0,0,601,602,3,74,37,0,602,603,5,150,0,0,603,604,5,172,
    0,0,604,605,5,30,0,0,605,608,3,74,37,0,606,607,5,20,0,0,607,609,5,172,
    0,0,608,606,1,0,0,0,608,609,1,0,0,0,609,626,1,0,0,0,610,626,5,124,0,0,
    611,612,5,122,0,0,612,613,5,29,0,0,613,614,3,74,37,0,614,615,5,42,0,0,
    615,616,3,74,37,0,616,617,5,151,0,0,617,618,3,74,37,0,618,619,5,45,0,
    0,619,620,3,74,37,0,620,621,5,30,0,0,621,622,3,74,37,0,622,623,5,152,
    0,0,623,624,3,74,37,0,624,626,1,0,0,0,625,551,1,0,0,0,625,552,1,0,0,0,
    625,562,1,0,0,0,625,574,1,0,0,0,625,586,1,0,0,0,625,595,1,0,0,0,625,597,
    1,0,0,0,625,610,1,0,0,0,625,611,1,0,0,0,626,79,1,0,0,0,627,628,5,47,0,
    0,628,645,5,125,0,0,629,630,3,74,37,0,630,631,5,30,0,0,631,634,3,74,37,
    0,632,633,5,29,0,0,633,635,3,74,37,0,634,632,1,0,0,0,634,635,1,0,0,0,
    635,646,1,0,0,0,636,637,5,29,0,0,637,638,3,74,37,0,638,639,5,146,0,0,
    639,640,3,74,37,0,640,641,5,152,0,0,641,642,3,74,37,0,642,643,5,30,0,
    0,643,644,3,74,37,0,644,646,1,0,0,0,645,629,1,0,0,0,645,636,1,0,0,0,646,
    648,1,0,0,0,647,649,3,108,54,0,648,647,1,0,0,0,648,649,1,0,0,0,649,81,
    1,0,0,0,650,651,5,47,0,0,651,652,5,126,0,0,652,653,5,172,0,0,653,654,
    5,51,0,0,654,666,3,74,37,0,655,665,3,108,54,0,656,661,5,19,0,0,657,658,
    7,1,0,0,658,660,5,172,0,0,659,657,1,0,0,0,660,663,1,0,0,0,661,659,1,0,
    0,0,661,662,1,0,0,0,662,665,1,0,0,0,663,661,1,0,0,0,664,655,1,0,0,0,664,
    656,1,0,0,0,665,668,1,0,0,0,666,664,1,0,0,0,666,667,1,0,0,0,667,83,1,
    0,0,0,668,666,1,0,0,0,669,670,5,127,0,0,670,674,3,74,37,0,671,673,3,86,
    43,0,672,671,1,0,0,0,673,676,1,0,0,0,674,672,1,0,0,0,674,675,1,0,0,0,
    675,682,1,0,0,0,676,674,1,0,0,0,677,679,7,20,0,0,678,680,5,172,0,0,679,
    678,1,0,0,0,679,680,1,0,0,0,680,682,1,0,0,0,681,669,1,0,0,0,681,677,1,
    0,0,0,682,85,1,0,0,0,683,684,7,21,0,0,684,690,3,74,37,0,685,686,5,20,
    0,0,686,690,5,172,0,0,687,690,5,159,0,0,688,690,5,160,0,0,689,683,1,0,
    0,0,689,685,1,0,0,0,689,687,1,0,0,0,689,688,1,0,0,0,690,87,1,0,0,0,691,
    692,5,142,0,0,692,89,1,0,0,0,693,694,5,130,0,0,694,701,3,74,37,0,695,
    696,5,153,0,0,696,700,3,74,37,0,697,698,5,20,0,0,698,700,5,172,0,0,699,
    695,1,0,0,0,699,697,1,0,0,0,700,703,1,0,0,0,701,699,1,0,0,0,701,702,1,
    0,0,0,702,710,1,0,0,0,703,701,1,0,0,0,704,707,5,131,0,0,705,706,5,20,
    0,0,706,708,5,172,0,0,707,705,1,0,0,0,707,708,1,0,0,0,708,710,1,0,0,0,
    709,693,1,0,0,0,709,704,1,0,0,0,710,91,1,0,0,0,711,716,5,132,0,0,712,
    716,5,143,0,0,713,714,5,133,0,0,714,716,7,22,0,0,715,711,1,0,0,0,715,
    712,1,0,0,0,715,713,1,0,0,0,716,721,1,0,0,0,717,718,7,1,0,0,718,720,5,
    172,0,0,719,717,1,0,0,0,720,723,1,0,0,0,721,719,1,0,0,0,721,722,1,0,0,
    0,722,93,1,0,0,0,723,721,1,0,0,0,724,725,5,137,0,0,725,726,5,73,0,0,726,
    727,3,74,37,0,727,95,1,0,0,0,728,743,5,135,0,0,729,733,5,3,0,0,730,732,
    3,98,49,0,731,730,1,0,0,0,732,735,1,0,0,0,733,731,1,0,0,0,733,734,1,0,
    0,0,734,744,1,0,0,0,735,733,1,0,0,0,736,739,5,6,0,0,737,738,5,115,0,0,
    738,740,3,74,37,0,739,737,1,0,0,0,739,740,1,0,0,0,740,744,1,0,0,0,741,
    742,5,141,0,0,742,744,3,74,37,0,743,729,1,0,0,0,743,736,1,0,0,0,743,741,
    1,0,0,0,744,97,1,0,0,0,745,746,7,23,0,0,746,754,5,172,0,0,747,748,5,161,
    0,0,748,754,5,172,0,0,749,754,5,162,0,0,750,754,5,163,0,0,751,752,7,24,
    0,0,752,754,3,74,37,0,753,745,1,0,0,0,753,747,1,0,0,0,753,749,1,0,0,0,
    753,750,1,0,0,0,753,751,1,0,0,0,754,99,1,0,0,0,755,762,5,136,0,0,756,
    757,7,23,0,0,757,761,5,172,0,0,758,759,7,25,0,0,759,761,3,74,37,0,760,
    756,1,0,0,0,760,758,1,0,0,0,761,764,1,0,0,0,762,760,1,0,0,0,762,763,1,
    0,0,0,763,101,1,0,0,0,764,762,1,0,0,0,765,766,5,167,0,0,766,767,3,74,
    37,0,767,103,1,0,0,0,768,769,7,26,0,0,769,770,5,172,0,0,770,105,1,0,0,
    0,771,772,5,170,0,0,772,781,3,74,37,0,773,776,5,171,0,0,774,777,3,74,
    37,0,775,777,5,174,0,0,776,774,1,0,0,0,776,775,1,0,0,0,777,778,1,0,0,
    0,778,776,1,0,0,0,778,779,1,0,0,0,779,781,1,0,0,0,780,771,1,0,0,0,780,
    773,1,0,0,0,781,107,1,0,0,0,782,783,5,178,0,0,783,784,3,74,37,0,784,109,
    1,0,0,0,89,147,154,161,166,172,174,179,190,196,198,208,211,218,226,233,
    239,241,247,256,264,272,280,288,294,296,303,307,309,318,320,326,328,332,
    334,344,349,358,366,385,395,398,404,411,441,447,454,456,466,478,485,487,
    495,497,510,515,517,526,533,537,546,560,572,608,625,634,645,648,661,664,
    666,674,679,681,689,699,701,707,709,715,721,733,739,743,753,760,762,776,
    778,780
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
    setState(110);
    match(TracerCommandsParser::T__0);
    setState(111);
    command();
    setState(112);
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
    setState(147);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 0, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(114);
      server();
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(115);
      attach();
      break;
    }

    case 3: {
      enterOuterAlt(_localctx, 3);
      setState(116);
      processes();
      break;
    }

    case 4: {
      enterOuterAlt(_localctx, 4);
      setState(117);
      sessions();
      break;
    }

    case 5: {
      enterOuterAlt(_localctx, 5);
      setState(118);
      use();
      break;
    }

    case 6: {
      enterOuterAlt(_localctx, 6);
      setState(119);
      status();
      break;
    }

    case 7: {
      enterOuterAlt(_localctx, 7);
      setState(120);
      detach();
      break;
    }

    case 8: {
      enterOuterAlt(_localctx, 8);
      setState(121);
      modules();
      break;
    }

    case 9: {
      enterOuterAlt(_localctx, 9);
      setState(122);
      moduleIndex();
      break;
    }

    case 10: {
      enterOuterAlt(_localctx, 10);
      setState(123);
      threads();
      break;
    }

    case 11: {
      enterOuterAlt(_localctx, 11);
      setState(124);
      symbols();
      break;
    }

    case 12: {
      enterOuterAlt(_localctx, 12);
      setState(125);
      probe();
      break;
    }

    case 13: {
      enterOuterAlt(_localctx, 13);
      setState(126);
      interceptor();
      break;
    }

    case 14: {
      enterOuterAlt(_localctx, 14);
      setState(127);
      events();
      break;
    }

    case 15: {
      enterOuterAlt(_localctx, 15);
      setState(128);
      collect();
      break;
    }

    case 16: {
      enterOuterAlt(_localctx, 16);
      setState(129);
      record();
      break;
    }

    case 17: {
      enterOuterAlt(_localctx, 17);
      setState(130);
      cpp();
      break;
    }

    case 18: {
      enterOuterAlt(_localctx, 18);
      setState(131);
      shell();
      break;
    }

    case 19: {
      enterOuterAlt(_localctx, 19);
      setState(132);
      complete();
      break;
    }

    case 20: {
      enterOuterAlt(_localctx, 20);
      setState(133);
      type();
      break;
    }

    case 21: {
      enterOuterAlt(_localctx, 21);
      setState(134);
      help();
      break;
    }

    case 22: {
      enterOuterAlt(_localctx, 22);
      setState(135);
      quit();
      break;
    }

    case 23: {
      enterOuterAlt(_localctx, 23);
      setState(136);
      bulkTrace();
      break;
    }

    case 24: {
      enterOuterAlt(_localctx, 24);
      setState(137);
      memory();
      break;
    }

    case 25: {
      enterOuterAlt(_localctx, 25);
      setState(138);
      probeEntry();
      break;
    }

    case 26: {
      enterOuterAlt(_localctx, 26);
      setState(139);
      probeReload();
      break;
    }

    case 27: {
      enterOuterAlt(_localctx, 27);
      setState(140);
      targetLaunch();
      break;
    }

    case 28: {
      enterOuterAlt(_localctx, 28);
      setState(141);
      targetCall();
      break;
    }

    case 29: {
      enterOuterAlt(_localctx, 29);
      setState(142);
      localPerf();
      break;
    }

    case 30: {
      enterOuterAlt(_localctx, 30);
      setState(143);
      instructions();
      break;
    }

    case 31: {
      enterOuterAlt(_localctx, 31);
      setState(144);
      watchpoint();
      break;
    }

    case 32: {
      enterOuterAlt(_localctx, 32);
      setState(145);
      targetLibrary();
      break;
    }

    case 33: {
      enterOuterAlt(_localctx, 33);
      setState(146);
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
    setState(149);
    match(TracerCommandsParser::T__1);
    setState(161);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__2: {
        setState(150);
        match(TracerCommandsParser::T__2);
        setState(154);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__6

        || _la == TracerCommandsParser::T__7 || _la == TracerCommandsParser::T__167

        || _la == TracerCommandsParser::T__168) {
          setState(151);
          serverOption();
          setState(156);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__3: {
        setState(157);
        match(TracerCommandsParser::T__3);
        setState(158);
        value();
        break;
      }

      case TracerCommandsParser::T__4: {
        setState(159);
        match(TracerCommandsParser::T__4);
        break;
      }

      case TracerCommandsParser::T__5: {
        setState(160);
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
    setState(166);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__6:
      case TracerCommandsParser::T__7: {
        enterOuterAlt(_localctx, 1);
        setState(163);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__6

        || _la == TracerCommandsParser::T__7)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(164);
        value();
        break;
      }

      case TracerCommandsParser::T__167:
      case TracerCommandsParser::T__168: {
        enterOuterAlt(_localctx, 2);
        setState(165);
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
    setState(168);
    match(TracerCommandsParser::T__8);
    setState(174);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::UINT) {
      setState(169);
      match(TracerCommandsParser::UINT);
      setState(172);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == TracerCommandsParser::T__9) {
        setState(170);
        match(TracerCommandsParser::T__9);
        setState(171);
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
    setState(176);
    match(TracerCommandsParser::T__10);
    setState(179);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::T__11) {
      setState(177);
      match(TracerCommandsParser::T__11);
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
    setState(181);
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
    setState(183);
    match(TracerCommandsParser::T__13);
    setState(184);
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
    setState(186);
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
    setState(188);
    match(TracerCommandsParser::T__14);
    setState(190);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 33654401923148412) != 0) || ((((_la - 72) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 72)) & -16492689326101) != 0) || ((((_la - 136) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 136)) & 37589553775103) != 0)) {
      setState(189);
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

std::vector<TracerCommandsParser::ValueContext *> TracerCommandsParser::ModulesContext::value() {
  return getRuleContexts<TracerCommandsParser::ValueContext>();
}

TracerCommandsParser::ValueContext* TracerCommandsParser::ModulesContext::value(size_t i) {
  return getRuleContext<TracerCommandsParser::ValueContext>(i);
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
    setState(192);
    match(TracerCommandsParser::T__15);
    setState(211);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::EOF:
      case TracerCommandsParser::T__11:
      case TracerCommandsParser::T__16: {
        setState(198);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__11

        || _la == TracerCommandsParser::T__16) {
          setState(196);
          _errHandler->sync(this);
          switch (_input->LA(1)) {
            case TracerCommandsParser::T__16: {
              setState(193);
              match(TracerCommandsParser::T__16);
              break;
            }

            case TracerCommandsParser::T__11: {
              setState(194);
              match(TracerCommandsParser::T__11);
              setState(195);
              value();
              break;
            }

          default:
            throw NoViableAltException(this);
          }
          setState(200);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__17: {
        setState(201);
        match(TracerCommandsParser::T__17);
        setState(202);
        value();
        break;
      }

      case TracerCommandsParser::T__18: {
        setState(203);
        match(TracerCommandsParser::T__18);
        setState(208);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__19

        || _la == TracerCommandsParser::T__20) {
          setState(204);
          _la = _input->LA(1);
          if (!(_la == TracerCommandsParser::T__19

          || _la == TracerCommandsParser::T__20)) {
          _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(205);
          match(TracerCommandsParser::UINT);
          setState(210);
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
    setState(213);
    match(TracerCommandsParser::T__21);
    setState(241);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__22: {
        setState(214);
        match(TracerCommandsParser::T__22);
        setState(216);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(215);
          moduleBuildOption();
          setState(218);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 34091302912) != 0));
        break;
      }

      case TracerCommandsParser::T__13: {
        setState(220);
        match(TracerCommandsParser::T__13);
        setState(221);
        value();
        break;
      }

      case TracerCommandsParser::T__23:
      case TracerCommandsParser::T__24: {
        setState(222);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__23

        || _la == TracerCommandsParser::T__24)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(226);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 1614907707392) != 0)) {
          setState(223);
          moduleQueryOption();
          setState(228);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__25: {
        setState(229);
        match(TracerCommandsParser::T__25);
        setState(231);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(230);
          moduleShowOption();
          setState(233);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 15427522527232) != 0));
        break;
      }

      case TracerCommandsParser::T__26: {
        setState(235);
        match(TracerCommandsParser::T__26);
        setState(237);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(236);
          moduleEmitOption();
          setState(239);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 15429670010880) != 0));
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
    setState(247);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__27:
      case TracerCommandsParser::T__28:
      case TracerCommandsParser::T__29:
      case TracerCommandsParser::T__30:
      case TracerCommandsParser::T__31: {
        enterOuterAlt(_localctx, 1);
        setState(243);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 8321499136) != 0))) {
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

      case TracerCommandsParser::T__32:
      case TracerCommandsParser::T__33: {
        enterOuterAlt(_localctx, 2);
        setState(245);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__32

        || _la == TracerCommandsParser::T__33)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(246);
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
    setState(256);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__11:
      case TracerCommandsParser::T__34:
      case TracerCommandsParser::T__35: {
        enterOuterAlt(_localctx, 1);
        setState(249);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 103079219200) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(250);
        value();
        break;
      }

      case TracerCommandsParser::T__36: {
        enterOuterAlt(_localctx, 2);
        setState(251);
        match(TracerCommandsParser::T__36);
        setState(252);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__37: {
        enterOuterAlt(_localctx, 3);
        setState(253);
        match(TracerCommandsParser::T__37);
        setState(254);
        match(TracerCommandsParser::T__38);
        break;
      }

      case TracerCommandsParser::T__39: {
        enterOuterAlt(_localctx, 4);
        setState(255);
        match(TracerCommandsParser::T__39);
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
    setState(258);
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 15427522527232) != 0))) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(259);
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
    setState(264);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__34:
      case TracerCommandsParser::T__40:
      case TracerCommandsParser::T__41:
      case TracerCommandsParser::T__42: {
        enterOuterAlt(_localctx, 1);
        setState(261);
        moduleShowOption();
        break;
      }

      case TracerCommandsParser::T__30: {
        enterOuterAlt(_localctx, 2);
        setState(262);
        match(TracerCommandsParser::T__30);
        setState(263);
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
    setState(266);
    match(TracerCommandsParser::T__43);

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
    setState(268);
    match(TracerCommandsParser::T__24);
    setState(272);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 105760885313536) != 0)) {
      setState(269);
      symbolOption();
      setState(274);
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
    setState(280);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__11:
      case TracerCommandsParser::T__28:
      case TracerCommandsParser::T__29:
      case TracerCommandsParser::T__35:
      case TracerCommandsParser::T__44: {
        enterOuterAlt(_localctx, 1);
        setState(275);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 35254702182400) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(276);
        value();
        break;
      }

      case TracerCommandsParser::T__36: {
        enterOuterAlt(_localctx, 2);
        setState(277);
        match(TracerCommandsParser::T__36);
        setState(278);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__45: {
        enterOuterAlt(_localctx, 3);
        setState(279);
        match(TracerCommandsParser::T__45);
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
    setState(334);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__46: {
        enterOuterAlt(_localctx, 1);
        setState(282);
        match(TracerCommandsParser::T__46);
        setState(328);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__47: {
            setState(283);
            match(TracerCommandsParser::T__47);
            setState(284);
            value();
            setState(286);
            _errHandler->sync(this);
            _la = _input->LA(1);
            do {
              setState(285);
              prepareOption();
              setState(288);
              _errHandler->sync(this);
              _la = _input->LA(1);
            } while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 1116901506365259776) != 0) || ((((_la - 170) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 170)) & 1827) != 0));
            break;
          }

          case TracerCommandsParser::T__48: {
            setState(290);
            match(TracerCommandsParser::T__48);
            setState(291);
            value();
            setState(296);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 8322652111917547520) != 0) || ((((_la - 170) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 170)) & 259) != 0)) {
              setState(294);
              _errHandler->sync(this);
              switch (_input->LA(1)) {
                case TracerCommandsParser::T__28:
                case TracerCommandsParser::T__54:
                case TracerCommandsParser::T__55:
                case TracerCommandsParser::T__56:
                case TracerCommandsParser::T__59:
                case TracerCommandsParser::T__60:
                case TracerCommandsParser::T__61:
                case TracerCommandsParser::T__169:
                case TracerCommandsParser::T__170: {
                  setState(292);
                  resolveOption();
                  break;
                }

                case TracerCommandsParser::CWD: {
                  setState(293);
                  cwdOption();
                  break;
                }

              default:
                throw NoViableAltException(this);
              }
              setState(298);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__49: {
            setState(299);
            match(TracerCommandsParser::T__49);
            setState(300);
            value();
            setState(303);
            _errHandler->sync(this);

            _la = _input->LA(1);
            if (_la == TracerCommandsParser::T__50) {
              setState(301);
              match(TracerCommandsParser::T__50);
              setState(302);
              value();
            }
            setState(309);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while (((((_la - 29) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 29)) & 8794415300609) != 0) || ((((_la - 167) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 167)) & 2073) != 0)) {
              setState(307);
              _errHandler->sync(this);
              switch (_input->LA(1)) {
                case TracerCommandsParser::T__28:
                case TracerCommandsParser::T__54:
                case TracerCommandsParser::T__55:
                case TracerCommandsParser::T__56:
                case TracerCommandsParser::T__59:
                case TracerCommandsParser::T__60:
                case TracerCommandsParser::T__61:
                case TracerCommandsParser::T__62:
                case TracerCommandsParser::T__63:
                case TracerCommandsParser::T__64:
                case TracerCommandsParser::T__65:
                case TracerCommandsParser::T__66:
                case TracerCommandsParser::T__67:
                case TracerCommandsParser::T__68:
                case TracerCommandsParser::T__69:
                case TracerCommandsParser::T__70:
                case TracerCommandsParser::T__166:
                case TracerCommandsParser::T__169:
                case TracerCommandsParser::T__170: {
                  setState(305);
                  deployOption();
                  break;
                }

                case TracerCommandsParser::CWD: {
                  setState(306);
                  cwdOption();
                  break;
                }

              default:
                throw NoViableAltException(this);
              }
              setState(311);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__51: {
            setState(312);
            match(TracerCommandsParser::T__51);
            setState(313);
            value();
            setState(314);
            match(TracerCommandsParser::T__50);
            setState(315);
            value();
            setState(320);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while (((((_la - 29) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 29)) & 1097833906177) != 0) || ((((_la - 170) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 170)) & 259) != 0)) {
              setState(318);
              _errHandler->sync(this);
              switch (_input->LA(1)) {
                case TracerCommandsParser::T__28:
                case TracerCommandsParser::T__54:
                case TracerCommandsParser::T__55:
                case TracerCommandsParser::T__56:
                case TracerCommandsParser::T__59:
                case TracerCommandsParser::T__60:
                case TracerCommandsParser::T__61:
                case TracerCommandsParser::T__62:
                case TracerCommandsParser::T__63:
                case TracerCommandsParser::T__64:
                case TracerCommandsParser::T__65:
                case TracerCommandsParser::T__66:
                case TracerCommandsParser::T__67:
                case TracerCommandsParser::T__169:
                case TracerCommandsParser::T__170: {
                  setState(316);
                  validateOption();
                  break;
                }

                case TracerCommandsParser::CWD: {
                  setState(317);
                  cwdOption();
                  break;
                }

              default:
                throw NoViableAltException(this);
              }
              setState(322);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__52: {
            setState(323);
            match(TracerCommandsParser::T__52);
            setState(324);
            match(TracerCommandsParser::UINT);
            setState(326);
            _errHandler->sync(this);

            _la = _input->LA(1);
            if (_la == TracerCommandsParser::CWD) {
              setState(325);
              cwdOption();
            }
            break;
          }

        default:
          throw NoViableAltException(this);
        }
        break;
      }

      case TracerCommandsParser::T__53: {
        enterOuterAlt(_localctx, 2);
        setState(330);
        match(TracerCommandsParser::T__53);
        setState(332);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::CWD) {
          setState(331);
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
    setState(344);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__28:
      case TracerCommandsParser::T__30:
      case TracerCommandsParser::T__42:
      case TracerCommandsParser::T__54:
      case TracerCommandsParser::T__55:
      case TracerCommandsParser::T__56:
      case TracerCommandsParser::T__57:
      case TracerCommandsParser::T__58: {
        enterOuterAlt(_localctx, 1);
        setState(336);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 1116901506365259776) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(337);
        value();
        break;
      }

      case TracerCommandsParser::SIGNATURE: {
        enterOuterAlt(_localctx, 2);
        setState(338);
        match(TracerCommandsParser::SIGNATURE);
        setState(339);
        value();
        break;
      }

      case TracerCommandsParser::MEMBER: {
        enterOuterAlt(_localctx, 3);
        setState(340);
        match(TracerCommandsParser::MEMBER);
        break;
      }

      case TracerCommandsParser::METADATA_ONLY: {
        enterOuterAlt(_localctx, 4);
        setState(341);
        match(TracerCommandsParser::METADATA_ONLY);
        break;
      }

      case TracerCommandsParser::T__169:
      case TracerCommandsParser::T__170: {
        enterOuterAlt(_localctx, 5);
        setState(342);
        extraCompileOption();
        break;
      }

      case TracerCommandsParser::CWD: {
        enterOuterAlt(_localctx, 6);
        setState(343);
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
    setState(349);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__28:
      case TracerCommandsParser::T__54:
      case TracerCommandsParser::T__55:
      case TracerCommandsParser::T__56:
      case TracerCommandsParser::T__59:
      case TracerCommandsParser::T__60:
      case TracerCommandsParser::T__61: {
        enterOuterAlt(_localctx, 1);
        setState(346);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 8322652111917547520) != 0))) {
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

      case TracerCommandsParser::T__169:
      case TracerCommandsParser::T__170: {
        enterOuterAlt(_localctx, 2);
        setState(348);
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
  enterRule(_localctx, 44, TracerCommandsParser::RuleValidateOption);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(358);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__28:
      case TracerCommandsParser::T__54:
      case TracerCommandsParser::T__55:
      case TracerCommandsParser::T__56:
      case TracerCommandsParser::T__59:
      case TracerCommandsParser::T__60:
      case TracerCommandsParser::T__61:
      case TracerCommandsParser::T__169:
      case TracerCommandsParser::T__170: {
        enterOuterAlt(_localctx, 1);
        setState(351);
        resolveOption();
        break;
      }

      case TracerCommandsParser::T__62: {
        enterOuterAlt(_localctx, 2);
        setState(352);
        match(TracerCommandsParser::T__62);
        break;
      }

      case TracerCommandsParser::T__63: {
        enterOuterAlt(_localctx, 3);
        setState(353);
        match(TracerCommandsParser::T__63);
        break;
      }

      case TracerCommandsParser::T__64: {
        enterOuterAlt(_localctx, 4);
        setState(354);
        match(TracerCommandsParser::T__64);
        break;
      }

      case TracerCommandsParser::T__65: {
        enterOuterAlt(_localctx, 5);
        setState(355);
        match(TracerCommandsParser::T__65);
        break;
      }

      case TracerCommandsParser::T__66: {
        enterOuterAlt(_localctx, 6);
        setState(356);
        match(TracerCommandsParser::T__66);
        break;
      }

      case TracerCommandsParser::T__67: {
        enterOuterAlt(_localctx, 7);
        setState(357);
        match(TracerCommandsParser::T__67);
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
  enterRule(_localctx, 46, TracerCommandsParser::RuleDeployOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(366);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__28:
      case TracerCommandsParser::T__54:
      case TracerCommandsParser::T__55:
      case TracerCommandsParser::T__56:
      case TracerCommandsParser::T__59:
      case TracerCommandsParser::T__60:
      case TracerCommandsParser::T__61:
      case TracerCommandsParser::T__62:
      case TracerCommandsParser::T__63:
      case TracerCommandsParser::T__64:
      case TracerCommandsParser::T__65:
      case TracerCommandsParser::T__66:
      case TracerCommandsParser::T__67:
      case TracerCommandsParser::T__169:
      case TracerCommandsParser::T__170: {
        enterOuterAlt(_localctx, 1);
        setState(360);
        validateOption();
        break;
      }

      case TracerCommandsParser::T__68: {
        enterOuterAlt(_localctx, 2);
        setState(361);
        match(TracerCommandsParser::T__68);
        setState(362);
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

      case TracerCommandsParser::T__69: {
        enterOuterAlt(_localctx, 3);
        setState(363);
        match(TracerCommandsParser::T__69);
        break;
      }

      case TracerCommandsParser::T__70: {
        enterOuterAlt(_localctx, 4);
        setState(364);
        match(TracerCommandsParser::T__70);
        break;
      }

      case TracerCommandsParser::T__166: {
        enterOuterAlt(_localctx, 5);
        setState(365);
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
  enterRule(_localctx, 48, TracerCommandsParser::RuleInterceptor);
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
    setState(368);
    match(TracerCommandsParser::T__71);
    setState(398);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__72: {
        setState(369);
        match(TracerCommandsParser::T__72);
        setState(370);
        value();
        setState(371);
        match(TracerCommandsParser::T__73);
        setState(372);
        value();
        break;
      }

      case TracerCommandsParser::T__74: {
        setState(374);
        match(TracerCommandsParser::T__74);
        setState(375);
        value();
        break;
      }

      case TracerCommandsParser::T__25: {
        setState(376);
        match(TracerCommandsParser::T__25);
        setState(377);
        value();
        break;
      }

      case TracerCommandsParser::T__51: {
        setState(378);
        match(TracerCommandsParser::T__51);
        setState(379);
        value();
        setState(380);
        match(TracerCommandsParser::T__75);
        setState(381);
        value();
        setState(385);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (((((_la - 29) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 29)) & 1097833906177) != 0) || _la == TracerCommandsParser::T__169

        || _la == TracerCommandsParser::T__170) {
          setState(382);
          validateOption();
          setState(387);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__76: {
        setState(388);
        match(TracerCommandsParser::T__76);
        setState(389);
        value();
        setState(390);
        match(TracerCommandsParser::T__75);
        setState(391);
        value();
        setState(395);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (((((_la - 29) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 29)) & 8794415300609) != 0) || ((((_la - 167) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 167)) & 25) != 0)) {
          setState(392);
          deployOption();
          setState(397);
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
  enterRule(_localctx, 50, TracerCommandsParser::RuleEvents);
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
    setState(400);
    match(TracerCommandsParser::T__77);
    setState(441);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::EOF:
      case TracerCommandsParser::T__19:
      case TracerCommandsParser::T__20:
      case TracerCommandsParser::T__73:
      case TracerCommandsParser::T__86:
      case TracerCommandsParser::T__87:
      case TracerCommandsParser::T__88:
      case TracerCommandsParser::T__92:
      case TracerCommandsParser::T__93:
      case TracerCommandsParser::T__94: {
        setState(404);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__19

        || _la == TracerCommandsParser::T__20 || ((((_la - 74) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 74)) & 3727361) != 0)) {
          setState(401);
          streamOption();
          setState(406);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__78: {
        setState(407);
        match(TracerCommandsParser::T__78);
        setState(408);
        value();
        setState(411);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__36) {
          setState(409);
          match(TracerCommandsParser::T__36);
          setState(410);
          match(TracerCommandsParser::UINT);
        }
        break;
      }

      case TracerCommandsParser::T__25: {
        setState(413);
        match(TracerCommandsParser::T__25);
        setState(414);
        value();
        setState(415);
        match(TracerCommandsParser::T__34);
        setState(416);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::STACK: {
        setState(418);
        match(TracerCommandsParser::STACK);
        setState(419);
        value();
        setState(420);
        match(TracerCommandsParser::T__34);
        setState(421);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__79: {
        setState(423);
        match(TracerCommandsParser::T__79);
        setState(424);
        value();
        break;
      }

      case TracerCommandsParser::T__80: {
        setState(425);
        match(TracerCommandsParser::T__80);
        setState(426);
        value();
        break;
      }

      case TracerCommandsParser::T__81: {
        setState(427);
        match(TracerCommandsParser::T__81);
        setState(428);
        value();
        break;
      }

      case TracerCommandsParser::T__82: {
        setState(429);
        match(TracerCommandsParser::T__82);
        setState(430);
        value();
        setState(431);
        match(TracerCommandsParser::T__73);
        setState(432);
        value();
        break;
      }

      case TracerCommandsParser::T__83: {
        setState(434);
        match(TracerCommandsParser::T__83);
        setState(435);
        value();
        setState(436);
        match(TracerCommandsParser::T__37);
        setState(437);
        match(TracerCommandsParser::T__38);
        setState(438);
        match(TracerCommandsParser::T__30);
        setState(439);
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
  enterRule(_localctx, 52, TracerCommandsParser::RuleCollect);
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
    match(TracerCommandsParser::T__84);
    setState(447);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == TracerCommandsParser::T__19

    || _la == TracerCommandsParser::T__20 || ((((_la - 74) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 74)) & 3727361) != 0)) {
      setState(444);
      streamOption();
      setState(449);
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
  enterRule(_localctx, 54, TracerCommandsParser::RuleRecord);
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
    setState(450);
    match(TracerCommandsParser::T__85);
    setState(451);
    value();
    setState(456);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == TracerCommandsParser::T__19

    || _la == TracerCommandsParser::T__20 || ((((_la - 74) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 74)) & 3727361) != 0) || _la == TracerCommandsParser::SQLITE) {
      setState(454);
      _errHandler->sync(this);
      switch (_input->LA(1)) {
        case TracerCommandsParser::T__19:
        case TracerCommandsParser::T__20:
        case TracerCommandsParser::T__73:
        case TracerCommandsParser::T__86:
        case TracerCommandsParser::T__87:
        case TracerCommandsParser::T__88:
        case TracerCommandsParser::T__92:
        case TracerCommandsParser::T__93:
        case TracerCommandsParser::T__94: {
          setState(452);
          streamOption();
          break;
        }

        case TracerCommandsParser::SQLITE: {
          setState(453);
          match(TracerCommandsParser::SQLITE);
          break;
        }

      default:
        throw NoViableAltException(this);
      }
      setState(458);
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
  enterRule(_localctx, 56, TracerCommandsParser::RuleStreamOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(466);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__19:
      case TracerCommandsParser::T__20:
      case TracerCommandsParser::T__86:
      case TracerCommandsParser::T__87: {
        enterOuterAlt(_localctx, 1);
        setState(459);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__19

        || _la == TracerCommandsParser::T__20 || _la == TracerCommandsParser::T__86

        || _la == TracerCommandsParser::T__87)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(460);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__88: {
        enterOuterAlt(_localctx, 2);
        setState(461);
        match(TracerCommandsParser::T__88);
        setState(462);
        _la = _input->LA(1);
        if (!(((((_la - 90) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 90)) & 7) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        break;
      }

      case TracerCommandsParser::T__73:
      case TracerCommandsParser::T__92:
      case TracerCommandsParser::T__93: {
        enterOuterAlt(_localctx, 3);
        setState(463);
        _la = _input->LA(1);
        if (!(((((_la - 74) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 74)) & 1572865) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(464);
        value();
        break;
      }

      case TracerCommandsParser::T__94: {
        enterOuterAlt(_localctx, 4);
        setState(465);
        match(TracerCommandsParser::T__94);
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
  enterRule(_localctx, 58, TracerCommandsParser::RuleCpp);
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
    setState(468);
    match(TracerCommandsParser::T__95);
    setState(497);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__72: {
        setState(469);
        match(TracerCommandsParser::T__72);
        setState(470);
        value();
        break;
      }

      case TracerCommandsParser::T__96: {
        setState(471);
        match(TracerCommandsParser::T__96);
        break;
      }

      case TracerCommandsParser::T__97:
      case TracerCommandsParser::T__98: {
        setState(472);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__97

        || _la == TracerCommandsParser::T__98)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(473);
        value();
        setState(478);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__30

        || _la == TracerCommandsParser::T__40) {
          setState(474);
          _la = _input->LA(1);
          if (!(_la == TracerCommandsParser::T__30

          || _la == TracerCommandsParser::T__40)) {
          _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(475);
          value();
          setState(480);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__99: {
        setState(481);
        match(TracerCommandsParser::T__99);
        setState(495);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__100: {
            setState(482);
            match(TracerCommandsParser::T__100);
            setState(485);
            _errHandler->sync(this);
            _la = _input->LA(1);
            do {
              setState(485);
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
                case TracerCommandsParser::T__23:
                case TracerCommandsParser::T__24:
                case TracerCommandsParser::T__25:
                case TracerCommandsParser::T__26:
                case TracerCommandsParser::T__38:
                case TracerCommandsParser::T__43:
                case TracerCommandsParser::T__46:
                case TracerCommandsParser::T__47:
                case TracerCommandsParser::T__48:
                case TracerCommandsParser::T__49:
                case TracerCommandsParser::T__51:
                case TracerCommandsParser::T__52:
                case TracerCommandsParser::T__53:
                case TracerCommandsParser::T__71:
                case TracerCommandsParser::T__72:
                case TracerCommandsParser::T__74:
                case TracerCommandsParser::T__76:
                case TracerCommandsParser::T__77:
                case TracerCommandsParser::T__78:
                case TracerCommandsParser::T__79:
                case TracerCommandsParser::T__80:
                case TracerCommandsParser::T__81:
                case TracerCommandsParser::T__82:
                case TracerCommandsParser::T__83:
                case TracerCommandsParser::T__84:
                case TracerCommandsParser::T__85:
                case TracerCommandsParser::T__89:
                case TracerCommandsParser::T__90:
                case TracerCommandsParser::T__91:
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
                case TracerCommandsParser::T__109:
                case TracerCommandsParser::T__110:
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
                case TracerCommandsParser::T__142:
                case TracerCommandsParser::T__143:
                case TracerCommandsParser::UINT:
                case TracerCommandsParser::STRING:
                case TracerCommandsParser::STACK:
                case TracerCommandsParser::WORD: {
                  setState(483);
                  value();
                  break;
                }

                case TracerCommandsParser::SHELL: {
                  setState(484);
                  match(TracerCommandsParser::SHELL);
                  break;
                }

              default:
                throw NoViableAltException(this);
              }
              setState(487);
              _errHandler->sync(this);
              _la = _input->LA(1);
            } while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 33654401923148412) != 0) || ((((_la - 72) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 72)) & -16492689326101) != 0) || ((((_la - 136) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 136)) & 37864431682047) != 0));
            break;
          }

          case TracerCommandsParser::T__81: {
            setState(489);
            match(TracerCommandsParser::T__81);
            break;
          }

          case TracerCommandsParser::T__101: {
            setState(490);
            match(TracerCommandsParser::T__101);
            setState(491);
            value();
            setState(492);
            match(TracerCommandsParser::T__27);
            setState(493);
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
  enterRule(_localctx, 60, TracerCommandsParser::RuleShell);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(499);
    match(TracerCommandsParser::T__102);
    setState(500);
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
  enterRule(_localctx, 62, TracerCommandsParser::RuleComplete);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(502);
    match(TracerCommandsParser::T__103);
    setState(503);
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
  enterRule(_localctx, 64, TracerCommandsParser::RuleType);

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
    match(TracerCommandsParser::T__104);
    setState(506);
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
  enterRule(_localctx, 66, TracerCommandsParser::RuleHelp);
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
    match(TracerCommandsParser::T__105);
    setState(517);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 33654401923148414) != 0) || ((((_la - 72) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 72)) & -16492689326101) != 0) || ((((_la - 136) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 136)) & 37589553775103) != 0)) {
      setState(510);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == TracerCommandsParser::T__0) {
        setState(509);
        match(TracerCommandsParser::T__0);
      }
      setState(513);
      _errHandler->sync(this);
      _la = _input->LA(1);
      do {
        setState(512);
        value();
        setState(515);
        _errHandler->sync(this);
        _la = _input->LA(1);
      } while ((((_la & ~ 0x3fULL) == 0) &&
        ((1ULL << _la) & 33654401923148412) != 0) || ((((_la - 72) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 72)) & -16492689326101) != 0) || ((((_la - 136) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 136)) & 37589553775103) != 0));
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
  enterRule(_localctx, 68, TracerCommandsParser::RuleQuit);
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
    _la = _input->LA(1);
    if (!(_la == TracerCommandsParser::T__106

    || _la == TracerCommandsParser::T__107)) {
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
  enterRule(_localctx, 70, TracerCommandsParser::RuleBulkTrace);
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
    setState(521);
    match(TracerCommandsParser::T__108);
    setState(537);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__109: {
        setState(522);
        match(TracerCommandsParser::T__109);
        setState(524);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(523);
          bulkTraceOption();
          setState(526);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while (_la == TracerCommandsParser::T__28

        || _la == TracerCommandsParser::T__54 || ((((_la - 112) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 112)) & 15) != 0));
        break;
      }

      case TracerCommandsParser::T__110: {
        setState(528);
        match(TracerCommandsParser::T__110);
        setState(529);
        match(TracerCommandsParser::T__111);
        setState(530);
        value();
        setState(533);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__50) {
          setState(531);
          match(TracerCommandsParser::T__50);
          setState(532);
          value();
        }
        break;
      }

      case TracerCommandsParser::T__52: {
        setState(535);
        match(TracerCommandsParser::T__52);
        setState(536);
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
  enterRule(_localctx, 72, TracerCommandsParser::RuleBulkTraceOption);
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
    _la = _input->LA(1);
    if (!(_la == TracerCommandsParser::T__28

    || _la == TracerCommandsParser::T__54 || ((((_la - 112) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 112)) & 15) != 0))) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(540);
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
  enterRule(_localctx, 74, TracerCommandsParser::RuleValue);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(546);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::STRING: {
        enterOuterAlt(_localctx, 1);
        setState(542);
        match(TracerCommandsParser::STRING);
        break;
      }

      case TracerCommandsParser::WORD: {
        enterOuterAlt(_localctx, 2);
        setState(543);
        match(TracerCommandsParser::WORD);
        break;
      }

      case TracerCommandsParser::UINT: {
        enterOuterAlt(_localctx, 3);
        setState(544);
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
      case TracerCommandsParser::T__23:
      case TracerCommandsParser::T__24:
      case TracerCommandsParser::T__25:
      case TracerCommandsParser::T__26:
      case TracerCommandsParser::T__38:
      case TracerCommandsParser::T__43:
      case TracerCommandsParser::T__46:
      case TracerCommandsParser::T__47:
      case TracerCommandsParser::T__48:
      case TracerCommandsParser::T__49:
      case TracerCommandsParser::T__51:
      case TracerCommandsParser::T__52:
      case TracerCommandsParser::T__53:
      case TracerCommandsParser::T__71:
      case TracerCommandsParser::T__72:
      case TracerCommandsParser::T__74:
      case TracerCommandsParser::T__76:
      case TracerCommandsParser::T__77:
      case TracerCommandsParser::T__78:
      case TracerCommandsParser::T__79:
      case TracerCommandsParser::T__80:
      case TracerCommandsParser::T__81:
      case TracerCommandsParser::T__82:
      case TracerCommandsParser::T__83:
      case TracerCommandsParser::T__84:
      case TracerCommandsParser::T__85:
      case TracerCommandsParser::T__89:
      case TracerCommandsParser::T__90:
      case TracerCommandsParser::T__91:
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
      case TracerCommandsParser::T__109:
      case TracerCommandsParser::T__110:
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
      case TracerCommandsParser::T__142:
      case TracerCommandsParser::T__143:
      case TracerCommandsParser::STACK: {
        enterOuterAlt(_localctx, 4);
        setState(545);
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
  enterRule(_localctx, 76, TracerCommandsParser::RuleKeyword);
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
    setState(548);
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 33654401923148412) != 0) || ((((_la - 72) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 72)) & -16492689326101) != 0) || ((((_la - 136) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 136)) & 2199023256063) != 0))) {
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
  enterRule(_localctx, 78, TracerCommandsParser::RuleMemory);
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
    setState(550);
    match(TracerCommandsParser::T__115);
    setState(625);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__116: {
        setState(551);
        match(TracerCommandsParser::T__116);
        break;
      }

      case TracerCommandsParser::T__117: {
        setState(552);
        match(TracerCommandsParser::T__117);
        setState(553);
        value();
        setState(554);
        match(TracerCommandsParser::T__29);
        setState(555);
        value();
        setState(560);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__20: {
            setState(556);
            match(TracerCommandsParser::T__20);
            setState(557);
            match(TracerCommandsParser::UINT);
            break;
          }

          case TracerCommandsParser::T__40: {
            setState(558);
            match(TracerCommandsParser::T__40);
            setState(559);
            value();
            break;
          }

        default:
          throw NoViableAltException(this);
        }
        break;
      }

      case TracerCommandsParser::T__118: {
        setState(562);
        match(TracerCommandsParser::T__118);
        setState(563);
        value();
        setState(564);
        match(TracerCommandsParser::T__29);
        setState(565);
        value();
        setState(566);
        match(TracerCommandsParser::T__20);
        setState(567);
        match(TracerCommandsParser::UINT);
        setState(568);
        match(TracerCommandsParser::T__144);
        setState(569);
        value();
        setState(572);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__36) {
          setState(570);
          match(TracerCommandsParser::T__36);
          setState(571);
          match(TracerCommandsParser::UINT);
        }
        break;
      }

      case TracerCommandsParser::T__119: {
        setState(574);
        match(TracerCommandsParser::T__119);
        setState(575);
        match(TracerCommandsParser::T__28);
        setState(576);
        value();
        setState(577);
        match(TracerCommandsParser::T__145);
        setState(578);
        value();
        setState(579);
        match(TracerCommandsParser::T__29);
        setState(580);
        value();
        setState(581);
        match(TracerCommandsParser::T__146);
        setState(582);
        value();
        setState(583);
        match(TracerCommandsParser::T__147);
        setState(584);
        value();
        break;
      }

      case TracerCommandsParser::T__139: {
        setState(586);
        match(TracerCommandsParser::T__139);
        setState(587);
        value();
        setState(588);
        match(TracerCommandsParser::T__29);
        setState(589);
        value();
        setState(590);
        match(TracerCommandsParser::T__146);
        setState(591);
        value();
        setState(592);
        match(TracerCommandsParser::T__147);
        setState(593);
        value();
        break;
      }

      case TracerCommandsParser::T__120:
      case TracerCommandsParser::T__137:
      case TracerCommandsParser::T__138: {
        setState(595);
        _la = _input->LA(1);
        if (!(((((_la - 121) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 121)) & 393217) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(596);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__122: {
        setState(597);
        match(TracerCommandsParser::T__122);
        setState(598);
        match(TracerCommandsParser::T__148);
        setState(599);
        match(TracerCommandsParser::UINT);
        setState(600);
        match(TracerCommandsParser::T__44);
        setState(601);
        value();
        setState(602);
        match(TracerCommandsParser::T__149);
        setState(603);
        match(TracerCommandsParser::UINT);
        setState(604);
        match(TracerCommandsParser::T__29);
        setState(605);
        value();
        setState(608);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__19) {
          setState(606);
          match(TracerCommandsParser::T__19);
          setState(607);
          match(TracerCommandsParser::UINT);
        }
        break;
      }

      case TracerCommandsParser::T__123: {
        setState(610);
        match(TracerCommandsParser::T__123);
        break;
      }

      case TracerCommandsParser::T__121: {
        setState(611);
        match(TracerCommandsParser::T__121);
        setState(612);
        match(TracerCommandsParser::T__28);
        setState(613);
        value();
        setState(614);
        match(TracerCommandsParser::T__41);
        setState(615);
        value();
        setState(616);
        match(TracerCommandsParser::T__150);
        setState(617);
        value();
        setState(618);
        match(TracerCommandsParser::T__44);
        setState(619);
        value();
        setState(620);
        match(TracerCommandsParser::T__29);
        setState(621);
        value();
        setState(622);
        match(TracerCommandsParser::T__151);
        setState(623);
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
  enterRule(_localctx, 80, TracerCommandsParser::RuleProbeEntry);
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
    match(TracerCommandsParser::T__46);
    setState(628);
    match(TracerCommandsParser::T__124);
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
      case TracerCommandsParser::T__21:
      case TracerCommandsParser::T__22:
      case TracerCommandsParser::T__23:
      case TracerCommandsParser::T__24:
      case TracerCommandsParser::T__25:
      case TracerCommandsParser::T__26:
      case TracerCommandsParser::T__38:
      case TracerCommandsParser::T__43:
      case TracerCommandsParser::T__46:
      case TracerCommandsParser::T__47:
      case TracerCommandsParser::T__48:
      case TracerCommandsParser::T__49:
      case TracerCommandsParser::T__51:
      case TracerCommandsParser::T__52:
      case TracerCommandsParser::T__53:
      case TracerCommandsParser::T__71:
      case TracerCommandsParser::T__72:
      case TracerCommandsParser::T__74:
      case TracerCommandsParser::T__76:
      case TracerCommandsParser::T__77:
      case TracerCommandsParser::T__78:
      case TracerCommandsParser::T__79:
      case TracerCommandsParser::T__80:
      case TracerCommandsParser::T__81:
      case TracerCommandsParser::T__82:
      case TracerCommandsParser::T__83:
      case TracerCommandsParser::T__84:
      case TracerCommandsParser::T__85:
      case TracerCommandsParser::T__89:
      case TracerCommandsParser::T__90:
      case TracerCommandsParser::T__91:
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
      case TracerCommandsParser::T__109:
      case TracerCommandsParser::T__110:
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
      case TracerCommandsParser::T__142:
      case TracerCommandsParser::T__143:
      case TracerCommandsParser::UINT:
      case TracerCommandsParser::STRING:
      case TracerCommandsParser::STACK:
      case TracerCommandsParser::WORD: {
        setState(629);
        value();
        setState(630);
        match(TracerCommandsParser::T__29);
        setState(631);
        value();
        setState(634);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__28) {
          setState(632);
          match(TracerCommandsParser::T__28);
          setState(633);
          value();
        }
        break;
      }

      case TracerCommandsParser::T__28: {
        setState(636);
        match(TracerCommandsParser::T__28);
        setState(637);
        value();
        setState(638);
        match(TracerCommandsParser::T__145);
        setState(639);
        value();
        setState(640);
        match(TracerCommandsParser::T__151);
        setState(641);
        value();
        setState(642);
        match(TracerCommandsParser::T__29);
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
  enterRule(_localctx, 82, TracerCommandsParser::RuleProbeReload);
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
    match(TracerCommandsParser::T__46);
    setState(651);
    match(TracerCommandsParser::T__125);
    setState(652);
    match(TracerCommandsParser::UINT);
    setState(653);
    match(TracerCommandsParser::T__50);
    setState(654);
    value();
    setState(666);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == TracerCommandsParser::T__18 || _la == TracerCommandsParser::CWD) {
      setState(664);
      _errHandler->sync(this);
      switch (_input->LA(1)) {
        case TracerCommandsParser::CWD: {
          setState(655);
          cwdOption();
          break;
        }

        case TracerCommandsParser::T__18: {
          setState(656);
          match(TracerCommandsParser::T__18);
          setState(661);
          _errHandler->sync(this);
          _la = _input->LA(1);
          while (_la == TracerCommandsParser::T__19

          || _la == TracerCommandsParser::T__20) {
            setState(657);
            _la = _input->LA(1);
            if (!(_la == TracerCommandsParser::T__19

            || _la == TracerCommandsParser::T__20)) {
            _errHandler->recoverInline(this);
            }
            else {
              _errHandler->reportMatch(this);
              consume();
            }
            setState(658);
            match(TracerCommandsParser::UINT);
            setState(663);
            _errHandler->sync(this);
            _la = _input->LA(1);
          }
          break;
        }

      default:
        throw NoViableAltException(this);
      }
      setState(668);
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
  enterRule(_localctx, 84, TracerCommandsParser::RuleTargetLaunch);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(681);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__126: {
        enterOuterAlt(_localctx, 1);
        setState(669);
        match(TracerCommandsParser::T__126);
        setState(670);
        value();
        setState(674);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__19 || ((((_la - 153) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 153)) & 33554687) != 0)) {
          setState(671);
          launchOption();
          setState(676);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__127:
      case TracerCommandsParser::T__128: {
        enterOuterAlt(_localctx, 2);
        setState(677);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__127

        || _la == TracerCommandsParser::T__128)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(679);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::UINT) {
          setState(678);
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
  enterRule(_localctx, 86, TracerCommandsParser::RuleLaunchOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(689);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__152:
      case TracerCommandsParser::T__153:
      case TracerCommandsParser::T__154:
      case TracerCommandsParser::T__155:
      case TracerCommandsParser::T__156:
      case TracerCommandsParser::T__157:
      case TracerCommandsParser::CWD: {
        enterOuterAlt(_localctx, 1);
        setState(683);
        _la = _input->LA(1);
        if (!(((((_la - 153) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 153)) & 33554495) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(684);
        value();
        break;
      }

      case TracerCommandsParser::T__19: {
        enterOuterAlt(_localctx, 2);
        setState(685);
        match(TracerCommandsParser::T__19);
        setState(686);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__158: {
        enterOuterAlt(_localctx, 3);
        setState(687);
        match(TracerCommandsParser::T__158);
        break;
      }

      case TracerCommandsParser::T__159: {
        enterOuterAlt(_localctx, 4);
        setState(688);
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
  enterRule(_localctx, 88, TracerCommandsParser::RuleChildList);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(691);
    match(TracerCommandsParser::T__141);

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
  enterRule(_localctx, 90, TracerCommandsParser::RuleTargetCall);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(709);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__129: {
        enterOuterAlt(_localctx, 1);
        setState(693);
        match(TracerCommandsParser::T__129);
        setState(694);
        value();
        setState(701);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__19 || _la == TracerCommandsParser::T__152) {
          setState(699);
          _errHandler->sync(this);
          switch (_input->LA(1)) {
            case TracerCommandsParser::T__152: {
              setState(695);
              match(TracerCommandsParser::T__152);
              setState(696);
              value();
              break;
            }

            case TracerCommandsParser::T__19: {
              setState(697);
              match(TracerCommandsParser::T__19);
              setState(698);
              match(TracerCommandsParser::UINT);
              break;
            }

          default:
            throw NoViableAltException(this);
          }
          setState(703);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__130: {
        enterOuterAlt(_localctx, 2);
        setState(704);
        match(TracerCommandsParser::T__130);
        setState(707);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__19) {
          setState(705);
          match(TracerCommandsParser::T__19);
          setState(706);
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
  enterRule(_localctx, 92, TracerCommandsParser::RuleLocalPerf);
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
    setState(715);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__131: {
        setState(711);
        match(TracerCommandsParser::T__131);
        break;
      }

      case TracerCommandsParser::T__142: {
        setState(712);
        match(TracerCommandsParser::T__142);
        break;
      }

      case TracerCommandsParser::T__132: {
        setState(713);
        match(TracerCommandsParser::T__132);
        setState(714);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__133

        || _la == TracerCommandsParser::T__143)) {
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
    setState(721);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (_la == TracerCommandsParser::T__19

    || _la == TracerCommandsParser::T__20) {
      setState(717);
      _la = _input->LA(1);
      if (!(_la == TracerCommandsParser::T__19

      || _la == TracerCommandsParser::T__20)) {
      _errHandler->recoverInline(this);
      }
      else {
        _errHandler->reportMatch(this);
        consume();
      }
      setState(718);
      match(TracerCommandsParser::UINT);
      setState(723);
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
  enterRule(_localctx, 94, TracerCommandsParser::RuleTargetLibrary);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(724);
    match(TracerCommandsParser::T__136);
    setState(725);
    match(TracerCommandsParser::T__72);
    setState(726);
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
  enterRule(_localctx, 96, TracerCommandsParser::RuleInstructions);
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
    setState(728);
    match(TracerCommandsParser::T__134);
    setState(743);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__2: {
        setState(729);
        match(TracerCommandsParser::T__2);
        setState(733);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 35185448976384) != 0) || ((((_la - 149) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 149)) & 126979) != 0)) {
          setState(730);
          instructionOption();
          setState(735);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__5: {
        setState(736);
        match(TracerCommandsParser::T__5);
        setState(739);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__114) {
          setState(737);
          match(TracerCommandsParser::T__114);
          setState(738);
          value();
        }
        break;
      }

      case TracerCommandsParser::T__140: {
        setState(741);
        match(TracerCommandsParser::T__140);
        setState(742);
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
  enterRule(_localctx, 98, TracerCommandsParser::RuleInstructionOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(753);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__19:
      case TracerCommandsParser::T__20:
      case TracerCommandsParser::T__148:
      case TracerCommandsParser::T__149: {
        enterOuterAlt(_localctx, 1);
        setState(745);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__19

        || _la == TracerCommandsParser::T__20 || _la == TracerCommandsParser::T__148

        || _la == TracerCommandsParser::T__149)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(746);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__160: {
        enterOuterAlt(_localctx, 2);
        setState(747);
        match(TracerCommandsParser::T__160);
        setState(748);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__161: {
        enterOuterAlt(_localctx, 3);
        setState(749);
        match(TracerCommandsParser::T__161);
        break;
      }

      case TracerCommandsParser::T__162: {
        enterOuterAlt(_localctx, 4);
        setState(750);
        match(TracerCommandsParser::T__162);
        break;
      }

      case TracerCommandsParser::T__29:
      case TracerCommandsParser::T__44:
      case TracerCommandsParser::T__163:
      case TracerCommandsParser::T__164: {
        enterOuterAlt(_localctx, 5);
        setState(751);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__29

        || _la == TracerCommandsParser::T__44 || _la == TracerCommandsParser::T__163

        || _la == TracerCommandsParser::T__164)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(752);
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
  enterRule(_localctx, 100, TracerCommandsParser::RuleWatchpoint);
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
    setState(755);
    match(TracerCommandsParser::T__135);
    setState(762);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 35185448976384) != 0) || ((((_la - 149) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 149)) & 131075) != 0)) {
      setState(760);
      _errHandler->sync(this);
      switch (_input->LA(1)) {
        case TracerCommandsParser::T__19:
        case TracerCommandsParser::T__20:
        case TracerCommandsParser::T__148:
        case TracerCommandsParser::T__149: {
          setState(756);
          _la = _input->LA(1);
          if (!(_la == TracerCommandsParser::T__19

          || _la == TracerCommandsParser::T__20 || _la == TracerCommandsParser::T__148

          || _la == TracerCommandsParser::T__149)) {
          _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(757);
          match(TracerCommandsParser::UINT);
          break;
        }

        case TracerCommandsParser::T__29:
        case TracerCommandsParser::T__44:
        case TracerCommandsParser::T__165: {
          setState(758);
          _la = _input->LA(1);
          if (!(_la == TracerCommandsParser::T__29

          || _la == TracerCommandsParser::T__44 || _la == TracerCommandsParser::T__165)) {
          _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(759);
          value();
          break;
        }

      default:
        throw NoViableAltException(this);
      }
      setState(764);
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
  enterRule(_localctx, 102, TracerCommandsParser::RuleStackFilter);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(765);
    match(TracerCommandsParser::T__166);
    setState(766);
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
  enterRule(_localctx, 104, TracerCommandsParser::RuleContextEventBytes);
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
    setState(768);
    _la = _input->LA(1);
    if (!(_la == TracerCommandsParser::T__167

    || _la == TracerCommandsParser::T__168)) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(769);
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
  enterRule(_localctx, 106, TracerCommandsParser::RuleExtraCompileOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(780);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__169: {
        enterOuterAlt(_localctx, 1);
        setState(771);
        match(TracerCommandsParser::T__169);
        setState(772);
        value();
        break;
      }

      case TracerCommandsParser::T__170: {
        enterOuterAlt(_localctx, 2);
        setState(773);
        match(TracerCommandsParser::T__170);
        setState(776);
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(776);
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
            case TracerCommandsParser::T__23:
            case TracerCommandsParser::T__24:
            case TracerCommandsParser::T__25:
            case TracerCommandsParser::T__26:
            case TracerCommandsParser::T__38:
            case TracerCommandsParser::T__43:
            case TracerCommandsParser::T__46:
            case TracerCommandsParser::T__47:
            case TracerCommandsParser::T__48:
            case TracerCommandsParser::T__49:
            case TracerCommandsParser::T__51:
            case TracerCommandsParser::T__52:
            case TracerCommandsParser::T__53:
            case TracerCommandsParser::T__71:
            case TracerCommandsParser::T__72:
            case TracerCommandsParser::T__74:
            case TracerCommandsParser::T__76:
            case TracerCommandsParser::T__77:
            case TracerCommandsParser::T__78:
            case TracerCommandsParser::T__79:
            case TracerCommandsParser::T__80:
            case TracerCommandsParser::T__81:
            case TracerCommandsParser::T__82:
            case TracerCommandsParser::T__83:
            case TracerCommandsParser::T__84:
            case TracerCommandsParser::T__85:
            case TracerCommandsParser::T__89:
            case TracerCommandsParser::T__90:
            case TracerCommandsParser::T__91:
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
            case TracerCommandsParser::T__109:
            case TracerCommandsParser::T__110:
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
            case TracerCommandsParser::T__142:
            case TracerCommandsParser::T__143:
            case TracerCommandsParser::UINT:
            case TracerCommandsParser::STRING:
            case TracerCommandsParser::STACK:
            case TracerCommandsParser::WORD: {
              setState(774);
              value();
              break;
            }

            case TracerCommandsParser::SHELL: {
              setState(775);
              match(TracerCommandsParser::SHELL);
              break;
            }

          default:
            throw NoViableAltException(this);
          }
          setState(778);
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 33654401923148412) != 0) || ((((_la - 72) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 72)) & -16492689326101) != 0) || ((((_la - 136) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 136)) & 37864431682047) != 0));
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
  enterRule(_localctx, 108, TracerCommandsParser::RuleCwdOption);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(782);
    match(TracerCommandsParser::CWD);
    setState(783);
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
