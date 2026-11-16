
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
      "value", "keyword", "objectProbe", "objectProbePrepareOption", "objectProbeBindOption", 
      "liveUpdate", "liveScanSelection", "liveOption", "objectWatch", "objectOption", 
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
      "'clear'", "'open'", "'--callback-errors'", "'export'", "'collect'", 
      "'record'", "'--count'", "'--timeout'", "'--after'", "'--probe'", 
      "'--phase'", "'PRE'", "'POST'", "'--name'", "'--session'", "'--all'", 
      "'cpp'", "'reset'", "'import'", "'generate'", "'options'", "'add'", 
      "'compdb'", "'shell'", "'complete'", "'type'", "'help'", "'quit'", 
      "'exit'", "'trace'", "'preview'", "'apply'", "'--manifest'", "'--exclude'", 
      "'--exact'", "'--save'", "'memory'", "'maps'", "'read'", "'scan'", 
      "'entry'", "'reload'", "'object'", "'object-probe'", "'live'", "'update'", 
      "'revert'", "'mock'", "'update-data'", "'scan-heap'", "'watch'", "'--callbacks'", 
      "'--method'", "'--field'", "'--code'", "'--dwarf-writers'", "'--package'", 
      "'--method-module'", "'--union-tag'", "'--union-case'", "'--synchronization'", 
      "'--follow-calls'", "'--no-values'", "'--first-seen'", "'--from-scan'", 
      "'--build'", "'--function'", "'--fixture'", "'--bind'", "'--recursion'", 
      "'--discover-file'", "'--discover-function'", "'--after-file'", "'--after-function'", 
      "'--for-each'", "'--managed'", "'--retention-limit'", "'--base'", 
      "'--owner'", "'--collection'", "'--owner-add'", "'--owner-get'", "'--owner-remove'", 
      "'--owner-snapshot'", "'--snapshot-field'", "'--plan'", "'--mode'", 
      "'--policy'", "'--on'", "'--emit-source'", "'--vector'", "'--element-key'", 
      "'--map'", "'--map-key'", "'--pointee'", "'--mutation'", "'--rendezvous'", 
      "'--overflow'", "'--shared-file'", "'--max-elements'", "'--max-string-bytes'", 
      "'--max-snapshot-bytes'", "'--max-instances'", "'--max-events'", "'--event-buffer'", 
      "'--tid'", "'--async-signal'", "'--serialize-wait-us'", "'--runtime-limit'", 
      "'--writer-entry'", "'--discover-writers'", "'--syscall-output'", 
      "'--stack-cfi'", "'--pattern'", "'--elf-address'", "'--abi'", "'--stack-application-only'", 
      "'--context-event-bytes'", "'--context-transport-memory-bytes'", "'--extra-arg'", 
      "'--extra-args'", "", "", "", "'--metadata-only'", "'--sqlite'", "'stack'", 
      "'--cwd'", "'--signature'", "'--member'", "'--snapshot-return-pointee'", 
      "'recover'", "'--payload-limit'"
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
      "", "", "", "", "", "", "", "", "", "", "", "", "", "UINT", "STRING", 
      "SHELL", "METADATA_ONLY", "SQLITE", "STACK", "CWD", "SIGNATURE", "MEMBER", 
      "SNAPSHOT_RETURN_POINTEE", "RECOVER", "PAYLOAD_LIMIT", "WORD", "WS"
    }
  );
  static const int32_t serializedATNSegment[] = {
  	4,1,213,734,2,0,7,0,2,1,7,1,2,2,7,2,2,3,7,3,2,4,7,4,2,5,7,5,2,6,7,6,2,
  	7,7,7,2,8,7,8,2,9,7,9,2,10,7,10,2,11,7,11,2,12,7,12,2,13,7,13,2,14,7,
  	14,2,15,7,15,2,16,7,16,2,17,7,17,2,18,7,18,2,19,7,19,2,20,7,20,2,21,7,
  	21,2,22,7,22,2,23,7,23,2,24,7,24,2,25,7,25,2,26,7,26,2,27,7,27,2,28,7,
  	28,2,29,7,29,2,30,7,30,2,31,7,31,2,32,7,32,2,33,7,33,2,34,7,34,2,35,7,
  	35,2,36,7,36,2,37,7,37,2,38,7,38,2,39,7,39,2,40,7,40,2,41,7,41,2,42,7,
  	42,2,43,7,43,2,44,7,44,2,45,7,45,2,46,7,46,2,47,7,47,2,48,7,48,2,49,7,
  	49,2,50,7,50,2,51,7,51,2,52,7,52,1,0,1,0,1,0,1,0,1,1,1,1,1,1,1,1,1,1,
  	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  	1,1,1,1,1,1,1,1,1,1,1,1,1,3,1,140,8,1,1,2,1,2,1,2,5,2,145,8,2,10,2,12,
  	2,148,9,2,1,2,1,2,1,2,1,2,3,2,154,8,2,1,3,1,3,1,3,3,3,159,8,3,1,4,1,4,
  	1,4,1,4,3,4,165,8,4,3,4,167,8,4,1,5,1,5,1,5,3,5,172,8,5,1,6,1,6,1,7,1,
  	7,1,7,1,8,1,8,1,9,1,9,3,9,183,8,9,1,10,1,10,3,10,187,8,10,1,11,1,11,1,
  	11,4,11,192,8,11,11,11,12,11,193,1,11,1,11,1,11,1,11,5,11,200,8,11,10,
  	11,12,11,203,9,11,1,11,1,11,4,11,207,8,11,11,11,12,11,208,1,11,1,11,4,
  	11,213,8,11,11,11,12,11,214,3,11,217,8,11,1,12,1,12,1,12,1,12,3,12,223,
  	8,12,1,13,1,13,1,13,1,13,1,13,1,13,1,13,3,13,232,8,13,1,14,1,14,1,14,
  	1,15,1,15,1,15,3,15,240,8,15,1,16,1,16,1,17,1,17,5,17,246,8,17,10,17,
  	12,17,249,9,17,1,18,1,18,1,18,1,18,1,18,3,18,256,8,18,1,19,1,19,1,19,
  	1,19,4,19,262,8,19,11,19,12,19,263,1,19,1,19,1,19,1,19,5,19,270,8,19,
  	10,19,12,19,273,9,19,1,19,1,19,1,19,1,19,3,19,279,8,19,1,19,1,19,5,19,
  	283,8,19,10,19,12,19,286,9,19,1,19,1,19,1,19,1,19,1,19,1,19,5,19,294,
  	8,19,10,19,12,19,297,9,19,1,19,1,19,1,19,3,19,302,8,19,3,19,304,8,19,
  	1,19,1,19,3,19,308,8,19,3,19,310,8,19,1,20,1,20,1,20,1,20,1,20,1,20,1,
  	20,1,20,3,20,320,8,20,1,21,1,21,1,21,3,21,325,8,21,1,22,1,22,1,22,1,22,
  	1,22,1,22,1,22,1,22,1,22,1,22,3,22,337,8,22,1,23,1,23,1,23,1,23,1,23,
  	1,23,1,23,1,23,1,23,1,23,1,23,1,23,1,23,1,23,1,23,5,23,354,8,23,10,23,
  	12,23,357,9,23,1,23,1,23,1,23,1,23,1,23,5,23,364,8,23,10,23,12,23,367,
  	9,23,3,23,369,8,23,1,24,1,24,5,24,373,8,24,10,24,12,24,376,9,24,1,24,
  	1,24,1,24,1,24,3,24,382,8,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,
  	1,24,1,24,1,24,1,24,1,24,5,24,397,8,24,10,24,12,24,400,9,24,1,24,1,24,
  	1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,
  	1,24,3,24,419,8,24,1,24,1,24,1,24,1,24,1,24,1,24,1,24,3,24,428,8,24,1,
  	25,1,25,5,25,432,8,25,10,25,12,25,435,9,25,1,26,1,26,1,26,1,26,5,26,441,
  	8,26,10,26,12,26,444,9,26,1,27,1,27,1,27,1,27,1,27,1,27,1,27,1,27,1,27,
  	3,27,455,8,27,1,28,1,28,1,28,1,28,1,28,1,28,1,28,1,28,5,28,465,8,28,10,
  	28,12,28,468,9,28,1,28,1,28,1,28,1,28,4,28,474,8,28,11,28,12,28,475,1,
  	28,1,28,1,28,1,28,1,28,1,28,3,28,484,8,28,3,28,486,8,28,1,29,1,29,1,29,
  	1,30,1,30,1,30,1,31,1,31,1,31,1,32,1,32,3,32,499,8,32,1,32,4,32,502,8,
  	32,11,32,12,32,503,3,32,506,8,32,1,33,1,33,1,34,1,34,1,34,4,34,513,8,
  	34,11,34,12,34,514,1,34,1,34,1,34,1,34,1,34,3,34,522,8,34,1,34,1,34,3,
  	34,526,8,34,1,35,1,35,1,35,1,36,1,36,1,36,1,36,3,36,535,8,36,1,37,1,37,
  	1,38,1,38,1,38,1,38,4,38,543,8,38,11,38,12,38,544,1,38,1,38,4,38,549,
  	8,38,11,38,12,38,550,1,38,1,38,3,38,555,8,38,3,38,557,8,38,1,39,1,39,
  	1,39,1,39,3,39,563,8,39,1,40,1,40,1,40,1,40,1,40,3,40,570,8,40,1,41,1,
  	41,1,41,1,41,4,41,576,8,41,11,41,12,41,577,1,41,1,41,1,41,1,41,3,41,584,
  	8,41,1,41,1,41,1,41,4,41,589,8,41,11,41,12,41,590,1,41,1,41,1,41,1,41,
  	5,41,597,8,41,10,41,12,41,600,9,41,1,41,1,41,4,41,604,8,41,11,41,12,41,
  	605,3,41,608,8,41,1,42,1,42,4,42,612,8,42,11,42,12,42,613,1,43,1,43,1,
  	43,1,43,3,43,620,8,43,1,44,1,44,1,44,3,44,625,8,44,1,44,4,44,628,8,44,
  	11,44,12,44,629,1,44,1,44,4,44,634,8,44,11,44,12,44,635,1,44,1,44,3,44,
  	640,8,44,3,44,642,8,44,1,45,1,45,1,45,1,45,1,45,1,45,1,45,1,45,1,45,1,
  	45,1,45,3,45,655,8,45,1,46,1,46,1,46,1,46,1,46,1,46,1,46,1,46,1,46,1,
  	46,3,46,667,8,46,1,46,1,46,1,46,1,46,1,46,1,46,1,46,1,46,1,46,1,46,3,
  	46,679,8,46,3,46,681,8,46,1,47,1,47,1,47,1,47,1,47,1,47,1,47,3,47,690,
  	8,47,1,47,1,47,1,47,1,47,1,47,1,47,1,47,1,47,1,47,3,47,701,8,47,1,47,
  	3,47,704,8,47,1,48,1,48,1,48,1,48,1,48,1,48,3,48,712,8,48,1,49,1,49,1,
  	49,1,50,1,50,1,50,1,51,1,51,1,51,1,51,1,51,4,51,725,8,51,11,51,12,51,
  	726,3,51,729,8,51,1,52,1,52,1,52,1,52,0,0,53,0,2,4,6,8,10,12,14,16,18,
  	20,22,24,26,28,30,32,34,36,38,40,42,44,46,48,50,52,54,56,58,60,62,64,
  	66,68,70,72,74,76,78,80,82,84,86,88,90,92,94,96,98,100,102,104,0,29,1,
  	0,7,8,1,0,20,21,1,0,24,28,1,0,29,30,2,0,12,12,31,32,2,0,31,31,37,39,4,
  	0,12,12,25,26,32,32,41,41,4,0,25,25,27,27,39,39,51,55,3,0,25,25,51,53,
  	56,58,2,0,200,200,212,212,1,0,63,64,1,0,85,88,1,0,90,91,2,0,67,67,92,
  	93,1,0,97,98,2,0,27,27,37,37,1,0,106,107,3,0,25,25,51,51,111,114,19,0,
  	2,6,9,9,11,11,13,16,18,23,35,35,40,40,43,46,48,50,65,66,68,68,70,72,77,
  	80,82,84,90,91,95,110,115,129,205,205,210,210,2,0,3,3,109,109,1,0,5,6,
  	3,0,27,27,51,53,130,133,2,0,25,25,135,139,3,0,109,109,124,124,129,129,
  	5,0,24,25,37,37,41,41,52,52,144,154,2,0,155,155,211,211,10,0,7,7,25,25,
  	27,27,37,37,51,53,63,64,76,76,132,133,137,139,156,177,4,0,30,30,59,59,
  	86,86,178,187,1,0,196,197,857,0,106,1,0,0,0,2,139,1,0,0,0,4,141,1,0,0,
  	0,6,158,1,0,0,0,8,160,1,0,0,0,10,168,1,0,0,0,12,173,1,0,0,0,14,175,1,
  	0,0,0,16,178,1,0,0,0,18,180,1,0,0,0,20,184,1,0,0,0,22,188,1,0,0,0,24,
  	222,1,0,0,0,26,231,1,0,0,0,28,233,1,0,0,0,30,239,1,0,0,0,32,241,1,0,0,
  	0,34,243,1,0,0,0,36,255,1,0,0,0,38,309,1,0,0,0,40,319,1,0,0,0,42,324,
  	1,0,0,0,44,336,1,0,0,0,46,338,1,0,0,0,48,370,1,0,0,0,50,429,1,0,0,0,52,
  	436,1,0,0,0,54,454,1,0,0,0,56,456,1,0,0,0,58,487,1,0,0,0,60,490,1,0,0,
  	0,62,493,1,0,0,0,64,496,1,0,0,0,66,507,1,0,0,0,68,509,1,0,0,0,70,527,
  	1,0,0,0,72,534,1,0,0,0,74,536,1,0,0,0,76,538,1,0,0,0,78,562,1,0,0,0,80,
  	569,1,0,0,0,82,571,1,0,0,0,84,609,1,0,0,0,86,619,1,0,0,0,88,621,1,0,0,
  	0,90,654,1,0,0,0,92,656,1,0,0,0,94,682,1,0,0,0,96,705,1,0,0,0,98,713,
  	1,0,0,0,100,716,1,0,0,0,102,728,1,0,0,0,104,730,1,0,0,0,106,107,5,1,0,
  	0,107,108,3,2,1,0,108,109,5,0,0,1,109,1,1,0,0,0,110,140,3,4,2,0,111,140,
  	3,8,4,0,112,140,3,10,5,0,113,140,3,12,6,0,114,140,3,14,7,0,115,140,3,
  	16,8,0,116,140,3,18,9,0,117,140,3,20,10,0,118,140,3,22,11,0,119,140,3,
  	32,16,0,120,140,3,34,17,0,121,140,3,38,19,0,122,140,3,46,23,0,123,140,
  	3,48,24,0,124,140,3,50,25,0,125,140,3,52,26,0,126,140,3,56,28,0,127,140,
  	3,58,29,0,128,140,3,60,30,0,129,140,3,62,31,0,130,140,3,64,32,0,131,140,
  	3,66,33,0,132,140,3,68,34,0,133,140,3,92,46,0,134,140,3,94,47,0,135,140,
  	3,96,48,0,136,140,3,88,44,0,137,140,3,76,38,0,138,140,3,82,41,0,139,110,
  	1,0,0,0,139,111,1,0,0,0,139,112,1,0,0,0,139,113,1,0,0,0,139,114,1,0,0,
  	0,139,115,1,0,0,0,139,116,1,0,0,0,139,117,1,0,0,0,139,118,1,0,0,0,139,
  	119,1,0,0,0,139,120,1,0,0,0,139,121,1,0,0,0,139,122,1,0,0,0,139,123,1,
  	0,0,0,139,124,1,0,0,0,139,125,1,0,0,0,139,126,1,0,0,0,139,127,1,0,0,0,
  	139,128,1,0,0,0,139,129,1,0,0,0,139,130,1,0,0,0,139,131,1,0,0,0,139,132,
  	1,0,0,0,139,133,1,0,0,0,139,134,1,0,0,0,139,135,1,0,0,0,139,136,1,0,0,
  	0,139,137,1,0,0,0,139,138,1,0,0,0,140,3,1,0,0,0,141,153,5,2,0,0,142,146,
  	5,3,0,0,143,145,3,6,3,0,144,143,1,0,0,0,145,148,1,0,0,0,146,144,1,0,0,
  	0,146,147,1,0,0,0,147,154,1,0,0,0,148,146,1,0,0,0,149,150,5,4,0,0,150,
  	154,3,72,36,0,151,154,5,5,0,0,152,154,5,6,0,0,153,142,1,0,0,0,153,149,
  	1,0,0,0,153,151,1,0,0,0,153,152,1,0,0,0,154,5,1,0,0,0,155,156,7,0,0,0,
  	156,159,3,72,36,0,157,159,3,100,50,0,158,155,1,0,0,0,158,157,1,0,0,0,
  	159,7,1,0,0,0,160,166,5,9,0,0,161,164,5,200,0,0,162,163,5,10,0,0,163,
  	165,5,200,0,0,164,162,1,0,0,0,164,165,1,0,0,0,165,167,1,0,0,0,166,161,
  	1,0,0,0,166,167,1,0,0,0,167,9,1,0,0,0,168,171,5,11,0,0,169,170,5,12,0,
  	0,170,172,3,72,36,0,171,169,1,0,0,0,171,172,1,0,0,0,172,11,1,0,0,0,173,
  	174,5,13,0,0,174,13,1,0,0,0,175,176,5,14,0,0,176,177,3,72,36,0,177,15,
  	1,0,0,0,178,179,5,5,0,0,179,17,1,0,0,0,180,182,5,15,0,0,181,183,3,72,
  	36,0,182,181,1,0,0,0,182,183,1,0,0,0,183,19,1,0,0,0,184,186,5,16,0,0,
  	185,187,5,17,0,0,186,185,1,0,0,0,186,187,1,0,0,0,187,21,1,0,0,0,188,216,
  	5,18,0,0,189,191,5,19,0,0,190,192,3,24,12,0,191,190,1,0,0,0,192,193,1,
  	0,0,0,193,191,1,0,0,0,193,194,1,0,0,0,194,217,1,0,0,0,195,196,5,14,0,
  	0,196,217,3,72,36,0,197,201,7,1,0,0,198,200,3,26,13,0,199,198,1,0,0,0,
  	200,203,1,0,0,0,201,199,1,0,0,0,201,202,1,0,0,0,202,217,1,0,0,0,203,201,
  	1,0,0,0,204,206,5,22,0,0,205,207,3,28,14,0,206,205,1,0,0,0,207,208,1,
  	0,0,0,208,206,1,0,0,0,208,209,1,0,0,0,209,217,1,0,0,0,210,212,5,23,0,
  	0,211,213,3,30,15,0,212,211,1,0,0,0,213,214,1,0,0,0,214,212,1,0,0,0,214,
  	215,1,0,0,0,215,217,1,0,0,0,216,189,1,0,0,0,216,195,1,0,0,0,216,197,1,
  	0,0,0,216,204,1,0,0,0,216,210,1,0,0,0,217,23,1,0,0,0,218,219,7,2,0,0,
  	219,223,3,72,36,0,220,221,7,3,0,0,221,223,5,200,0,0,222,218,1,0,0,0,222,
  	220,1,0,0,0,223,25,1,0,0,0,224,225,7,4,0,0,225,232,3,72,36,0,226,227,
  	5,33,0,0,227,232,5,200,0,0,228,229,5,34,0,0,229,232,5,35,0,0,230,232,
  	5,36,0,0,231,224,1,0,0,0,231,226,1,0,0,0,231,228,1,0,0,0,231,230,1,0,
  	0,0,232,27,1,0,0,0,233,234,7,5,0,0,234,235,3,72,36,0,235,29,1,0,0,0,236,
  	240,3,28,14,0,237,238,5,27,0,0,238,240,3,72,36,0,239,236,1,0,0,0,239,
  	237,1,0,0,0,240,31,1,0,0,0,241,242,5,40,0,0,242,33,1,0,0,0,243,247,5,
  	21,0,0,244,246,3,36,18,0,245,244,1,0,0,0,246,249,1,0,0,0,247,245,1,0,
  	0,0,247,248,1,0,0,0,248,35,1,0,0,0,249,247,1,0,0,0,250,251,7,6,0,0,251,
  	256,3,72,36,0,252,253,5,33,0,0,253,256,5,200,0,0,254,256,5,42,0,0,255,
  	250,1,0,0,0,255,252,1,0,0,0,255,254,1,0,0,0,256,37,1,0,0,0,257,303,5,
  	43,0,0,258,259,5,44,0,0,259,261,3,72,36,0,260,262,3,40,20,0,261,260,1,
  	0,0,0,262,263,1,0,0,0,263,261,1,0,0,0,263,264,1,0,0,0,264,304,1,0,0,0,
  	265,266,5,45,0,0,266,271,3,72,36,0,267,270,3,42,21,0,268,270,3,104,52,
  	0,269,267,1,0,0,0,269,268,1,0,0,0,270,273,1,0,0,0,271,269,1,0,0,0,271,
  	272,1,0,0,0,272,304,1,0,0,0,273,271,1,0,0,0,274,275,5,46,0,0,275,278,
  	3,72,36,0,276,277,5,47,0,0,277,279,3,72,36,0,278,276,1,0,0,0,278,279,
  	1,0,0,0,279,284,1,0,0,0,280,283,3,44,22,0,281,283,3,104,52,0,282,280,
  	1,0,0,0,282,281,1,0,0,0,283,286,1,0,0,0,284,282,1,0,0,0,284,285,1,0,0,
  	0,285,304,1,0,0,0,286,284,1,0,0,0,287,288,5,48,0,0,288,289,3,72,36,0,
  	289,290,5,47,0,0,290,295,3,72,36,0,291,294,3,42,21,0,292,294,3,104,52,
  	0,293,291,1,0,0,0,293,292,1,0,0,0,294,297,1,0,0,0,295,293,1,0,0,0,295,
  	296,1,0,0,0,296,304,1,0,0,0,297,295,1,0,0,0,298,299,5,49,0,0,299,301,
  	5,200,0,0,300,302,3,104,52,0,301,300,1,0,0,0,301,302,1,0,0,0,302,304,
  	1,0,0,0,303,258,1,0,0,0,303,265,1,0,0,0,303,274,1,0,0,0,303,287,1,0,0,
  	0,303,298,1,0,0,0,304,310,1,0,0,0,305,307,5,50,0,0,306,308,3,104,52,0,
  	307,306,1,0,0,0,307,308,1,0,0,0,308,310,1,0,0,0,309,257,1,0,0,0,309,305,
  	1,0,0,0,310,39,1,0,0,0,311,312,7,7,0,0,312,320,3,72,36,0,313,314,5,207,
  	0,0,314,320,3,72,36,0,315,320,5,208,0,0,316,320,5,203,0,0,317,320,3,102,
  	51,0,318,320,3,104,52,0,319,311,1,0,0,0,319,313,1,0,0,0,319,315,1,0,0,
  	0,319,316,1,0,0,0,319,317,1,0,0,0,319,318,1,0,0,0,320,41,1,0,0,0,321,
  	322,7,8,0,0,322,325,3,72,36,0,323,325,3,102,51,0,324,321,1,0,0,0,324,
  	323,1,0,0,0,325,43,1,0,0,0,326,337,3,42,21,0,327,328,5,59,0,0,328,337,
  	7,9,0,0,329,337,5,60,0,0,330,337,5,61,0,0,331,337,5,209,0,0,332,337,5,
  	62,0,0,333,337,3,98,49,0,334,335,7,10,0,0,335,337,3,72,36,0,336,326,1,
  	0,0,0,336,327,1,0,0,0,336,329,1,0,0,0,336,330,1,0,0,0,336,331,1,0,0,0,
  	336,332,1,0,0,0,336,333,1,0,0,0,336,334,1,0,0,0,337,45,1,0,0,0,338,368,
  	5,65,0,0,339,340,5,66,0,0,340,341,3,72,36,0,341,342,5,67,0,0,342,343,
  	3,72,36,0,343,369,1,0,0,0,344,345,5,68,0,0,345,369,3,72,36,0,346,347,
  	5,22,0,0,347,369,3,72,36,0,348,349,5,48,0,0,349,350,3,72,36,0,350,351,
  	5,69,0,0,351,355,3,72,36,0,352,354,3,42,21,0,353,352,1,0,0,0,354,357,
  	1,0,0,0,355,353,1,0,0,0,355,356,1,0,0,0,356,369,1,0,0,0,357,355,1,0,0,
  	0,358,359,5,70,0,0,359,360,3,72,36,0,360,361,5,69,0,0,361,365,3,72,36,
  	0,362,364,3,44,22,0,363,362,1,0,0,0,364,367,1,0,0,0,365,363,1,0,0,0,365,
  	366,1,0,0,0,366,369,1,0,0,0,367,365,1,0,0,0,368,339,1,0,0,0,368,344,1,
  	0,0,0,368,346,1,0,0,0,368,348,1,0,0,0,368,358,1,0,0,0,369,47,1,0,0,0,
  	370,427,5,71,0,0,371,373,3,54,27,0,372,371,1,0,0,0,373,376,1,0,0,0,374,
  	372,1,0,0,0,374,375,1,0,0,0,375,428,1,0,0,0,376,374,1,0,0,0,377,378,5,
  	72,0,0,378,381,3,72,36,0,379,380,5,33,0,0,380,382,5,200,0,0,381,379,1,
  	0,0,0,381,382,1,0,0,0,382,428,1,0,0,0,383,384,5,22,0,0,384,385,3,72,36,
  	0,385,386,5,31,0,0,386,387,5,200,0,0,387,428,1,0,0,0,388,389,5,73,0,0,
  	389,390,3,72,36,0,390,391,5,74,0,0,391,392,5,200,0,0,392,393,5,75,0,0,
  	393,398,5,200,0,0,394,395,5,76,0,0,395,397,3,72,36,0,396,394,1,0,0,0,
  	397,400,1,0,0,0,398,396,1,0,0,0,398,399,1,0,0,0,399,428,1,0,0,0,400,398,
  	1,0,0,0,401,402,5,205,0,0,402,403,3,72,36,0,403,404,5,31,0,0,404,405,
  	5,200,0,0,405,428,1,0,0,0,406,407,5,77,0,0,407,428,3,72,36,0,408,409,
  	5,78,0,0,409,428,3,72,36,0,410,411,5,79,0,0,411,428,3,72,36,0,412,413,
  	5,80,0,0,413,414,3,72,36,0,414,415,5,67,0,0,415,418,3,72,36,0,416,417,
  	5,81,0,0,417,419,3,72,36,0,418,416,1,0,0,0,418,419,1,0,0,0,419,428,1,
  	0,0,0,420,421,5,82,0,0,421,422,3,72,36,0,422,423,5,34,0,0,423,424,5,35,
  	0,0,424,425,5,27,0,0,425,426,3,72,36,0,426,428,1,0,0,0,427,374,1,0,0,
  	0,427,377,1,0,0,0,427,383,1,0,0,0,427,388,1,0,0,0,427,401,1,0,0,0,427,
  	406,1,0,0,0,427,408,1,0,0,0,427,410,1,0,0,0,427,412,1,0,0,0,427,420,1,
  	0,0,0,428,49,1,0,0,0,429,433,5,83,0,0,430,432,3,54,27,0,431,430,1,0,0,
  	0,432,435,1,0,0,0,433,431,1,0,0,0,433,434,1,0,0,0,434,51,1,0,0,0,435,
  	433,1,0,0,0,436,437,5,84,0,0,437,442,3,72,36,0,438,441,3,54,27,0,439,
  	441,5,204,0,0,440,438,1,0,0,0,440,439,1,0,0,0,441,444,1,0,0,0,442,440,
  	1,0,0,0,442,443,1,0,0,0,443,53,1,0,0,0,444,442,1,0,0,0,445,446,7,11,0,
  	0,446,455,5,200,0,0,447,448,5,89,0,0,448,455,7,12,0,0,449,450,5,81,0,
  	0,450,455,3,72,36,0,451,452,7,13,0,0,452,455,3,72,36,0,453,455,5,94,0,
  	0,454,445,1,0,0,0,454,447,1,0,0,0,454,449,1,0,0,0,454,451,1,0,0,0,454,
  	453,1,0,0,0,455,55,1,0,0,0,456,485,5,95,0,0,457,458,5,66,0,0,458,486,
  	3,72,36,0,459,486,5,96,0,0,460,461,7,14,0,0,461,466,3,72,36,0,462,463,
  	7,15,0,0,463,465,3,72,36,0,464,462,1,0,0,0,465,468,1,0,0,0,466,464,1,
  	0,0,0,466,467,1,0,0,0,467,486,1,0,0,0,468,466,1,0,0,0,469,483,5,99,0,
  	0,470,473,5,100,0,0,471,474,3,72,36,0,472,474,5,202,0,0,473,471,1,0,0,
  	0,473,472,1,0,0,0,474,475,1,0,0,0,475,473,1,0,0,0,475,476,1,0,0,0,476,
  	484,1,0,0,0,477,484,5,79,0,0,478,479,5,101,0,0,479,480,3,72,36,0,480,
  	481,5,24,0,0,481,482,3,72,36,0,482,484,1,0,0,0,483,470,1,0,0,0,483,477,
  	1,0,0,0,483,478,1,0,0,0,483,484,1,0,0,0,484,486,1,0,0,0,485,457,1,0,0,
  	0,485,459,1,0,0,0,485,460,1,0,0,0,485,469,1,0,0,0,486,57,1,0,0,0,487,
  	488,5,102,0,0,488,489,3,72,36,0,489,59,1,0,0,0,490,491,5,103,0,0,491,
  	492,3,72,36,0,492,61,1,0,0,0,493,494,5,104,0,0,494,495,3,72,36,0,495,
  	63,1,0,0,0,496,505,5,105,0,0,497,499,5,1,0,0,498,497,1,0,0,0,498,499,
  	1,0,0,0,499,501,1,0,0,0,500,502,3,72,36,0,501,500,1,0,0,0,502,503,1,0,
  	0,0,503,501,1,0,0,0,503,504,1,0,0,0,504,506,1,0,0,0,505,498,1,0,0,0,505,
  	506,1,0,0,0,506,65,1,0,0,0,507,508,7,16,0,0,508,67,1,0,0,0,509,525,5,
  	108,0,0,510,512,5,109,0,0,511,513,3,70,35,0,512,511,1,0,0,0,513,514,1,
  	0,0,0,514,512,1,0,0,0,514,515,1,0,0,0,515,526,1,0,0,0,516,517,5,110,0,
  	0,517,518,5,111,0,0,518,521,3,72,36,0,519,520,5,47,0,0,520,522,3,72,36,
  	0,521,519,1,0,0,0,521,522,1,0,0,0,522,526,1,0,0,0,523,524,5,49,0,0,524,
  	526,3,72,36,0,525,510,1,0,0,0,525,516,1,0,0,0,525,523,1,0,0,0,526,69,
  	1,0,0,0,527,528,7,17,0,0,528,529,3,72,36,0,529,71,1,0,0,0,530,535,5,201,
  	0,0,531,535,5,212,0,0,532,535,5,200,0,0,533,535,3,74,37,0,534,530,1,0,
  	0,0,534,531,1,0,0,0,534,532,1,0,0,0,534,533,1,0,0,0,535,73,1,0,0,0,536,
  	537,7,18,0,0,537,75,1,0,0,0,538,556,5,122,0,0,539,540,5,44,0,0,540,542,
  	3,72,36,0,541,543,3,78,39,0,542,541,1,0,0,0,543,544,1,0,0,0,544,542,1,
  	0,0,0,544,545,1,0,0,0,545,557,1,0,0,0,546,548,7,19,0,0,547,549,3,80,40,
  	0,548,547,1,0,0,0,549,550,1,0,0,0,550,548,1,0,0,0,550,551,1,0,0,0,551,
  	557,1,0,0,0,552,554,7,20,0,0,553,555,5,200,0,0,554,553,1,0,0,0,554,555,
  	1,0,0,0,555,557,1,0,0,0,556,539,1,0,0,0,556,546,1,0,0,0,556,552,1,0,0,
  	0,557,77,1,0,0,0,558,559,7,21,0,0,559,563,3,72,36,0,560,563,5,134,0,0,
  	561,563,3,102,51,0,562,558,1,0,0,0,562,560,1,0,0,0,562,561,1,0,0,0,563,
  	79,1,0,0,0,564,565,7,22,0,0,565,570,3,72,36,0,566,570,5,140,0,0,567,570,
  	5,141,0,0,568,570,5,142,0,0,569,564,1,0,0,0,569,566,1,0,0,0,569,567,1,
  	0,0,0,569,568,1,0,0,0,570,81,1,0,0,0,571,607,5,123,0,0,572,575,7,23,0,
  	0,573,576,3,72,36,0,574,576,3,86,43,0,575,573,1,0,0,0,575,574,1,0,0,0,
  	576,577,1,0,0,0,577,575,1,0,0,0,577,578,1,0,0,0,578,608,1,0,0,0,579,608,
  	5,5,0,0,580,608,5,210,0,0,581,583,5,125,0,0,582,584,5,200,0,0,583,582,
  	1,0,0,0,583,584,1,0,0,0,584,608,1,0,0,0,585,586,5,126,0,0,586,588,3,72,
  	36,0,587,589,3,86,43,0,588,587,1,0,0,0,589,590,1,0,0,0,590,588,1,0,0,
  	0,590,591,1,0,0,0,591,608,1,0,0,0,592,593,5,127,0,0,593,598,3,72,36,0,
  	594,597,3,86,43,0,595,597,3,84,42,0,596,594,1,0,0,0,596,595,1,0,0,0,597,
  	600,1,0,0,0,598,596,1,0,0,0,598,599,1,0,0,0,599,608,1,0,0,0,600,598,1,
  	0,0,0,601,603,5,128,0,0,602,604,3,86,43,0,603,602,1,0,0,0,604,605,1,0,
  	0,0,605,603,1,0,0,0,605,606,1,0,0,0,606,608,1,0,0,0,607,572,1,0,0,0,607,
  	579,1,0,0,0,607,580,1,0,0,0,607,581,1,0,0,0,607,585,1,0,0,0,607,592,1,
  	0,0,0,607,601,1,0,0,0,608,83,1,0,0,0,609,611,5,143,0,0,610,612,3,72,36,
  	0,611,610,1,0,0,0,612,613,1,0,0,0,613,611,1,0,0,0,613,614,1,0,0,0,614,
  	85,1,0,0,0,615,616,7,24,0,0,616,620,3,72,36,0,617,618,7,25,0,0,618,620,
  	5,200,0,0,619,615,1,0,0,0,619,617,1,0,0,0,620,87,1,0,0,0,621,641,5,121,
  	0,0,622,624,5,44,0,0,623,625,3,72,36,0,624,623,1,0,0,0,624,625,1,0,0,
  	0,625,627,1,0,0,0,626,628,3,90,45,0,627,626,1,0,0,0,628,629,1,0,0,0,629,
  	627,1,0,0,0,629,630,1,0,0,0,630,642,1,0,0,0,631,633,7,19,0,0,632,634,
  	3,90,45,0,633,632,1,0,0,0,634,635,1,0,0,0,635,633,1,0,0,0,635,636,1,0,
  	0,0,636,642,1,0,0,0,637,639,7,20,0,0,638,640,5,200,0,0,639,638,1,0,0,
  	0,639,640,1,0,0,0,640,642,1,0,0,0,641,622,1,0,0,0,641,631,1,0,0,0,641,
  	637,1,0,0,0,642,89,1,0,0,0,643,644,7,26,0,0,644,655,3,72,36,0,645,646,
  	7,27,0,0,646,655,5,200,0,0,647,655,5,141,0,0,648,655,5,142,0,0,649,655,
  	5,188,0,0,650,655,5,189,0,0,651,655,5,140,0,0,652,655,5,190,0,0,653,655,
  	5,191,0,0,654,643,1,0,0,0,654,645,1,0,0,0,654,647,1,0,0,0,654,648,1,0,
  	0,0,654,649,1,0,0,0,654,650,1,0,0,0,654,651,1,0,0,0,654,652,1,0,0,0,654,
  	653,1,0,0,0,655,91,1,0,0,0,656,680,5,115,0,0,657,681,5,116,0,0,658,659,
  	5,117,0,0,659,660,3,72,36,0,660,661,5,26,0,0,661,666,3,72,36,0,662,663,
  	5,85,0,0,663,667,5,200,0,0,664,665,5,37,0,0,665,667,3,72,36,0,666,662,
  	1,0,0,0,666,664,1,0,0,0,667,681,1,0,0,0,668,669,5,118,0,0,669,670,3,72,
  	36,0,670,671,5,26,0,0,671,672,3,72,36,0,672,673,5,85,0,0,673,674,5,200,
  	0,0,674,675,5,192,0,0,675,678,3,72,36,0,676,677,5,33,0,0,677,679,5,200,
  	0,0,678,676,1,0,0,0,678,679,1,0,0,0,679,681,1,0,0,0,680,657,1,0,0,0,680,
  	658,1,0,0,0,680,668,1,0,0,0,681,93,1,0,0,0,682,683,5,43,0,0,683,700,5,
  	119,0,0,684,685,3,72,36,0,685,686,5,26,0,0,686,689,3,72,36,0,687,688,
  	5,25,0,0,688,690,3,72,36,0,689,687,1,0,0,0,689,690,1,0,0,0,690,701,1,
  	0,0,0,691,692,5,25,0,0,692,693,3,72,36,0,693,694,5,193,0,0,694,695,3,
  	72,36,0,695,696,5,194,0,0,696,697,3,72,36,0,697,698,5,26,0,0,698,699,
  	3,72,36,0,699,701,1,0,0,0,700,684,1,0,0,0,700,691,1,0,0,0,701,703,1,0,
  	0,0,702,704,3,104,52,0,703,702,1,0,0,0,703,704,1,0,0,0,704,95,1,0,0,0,
  	705,706,5,43,0,0,706,707,5,120,0,0,707,708,5,200,0,0,708,709,5,47,0,0,
  	709,711,3,72,36,0,710,712,3,104,52,0,711,710,1,0,0,0,711,712,1,0,0,0,
  	712,97,1,0,0,0,713,714,5,195,0,0,714,715,3,72,36,0,715,99,1,0,0,0,716,
  	717,7,28,0,0,717,718,5,200,0,0,718,101,1,0,0,0,719,720,5,198,0,0,720,
  	729,3,72,36,0,721,724,5,199,0,0,722,725,3,72,36,0,723,725,5,202,0,0,724,
  	722,1,0,0,0,724,723,1,0,0,0,725,726,1,0,0,0,726,724,1,0,0,0,726,727,1,
  	0,0,0,727,729,1,0,0,0,728,719,1,0,0,0,728,721,1,0,0,0,729,103,1,0,0,0,
  	730,731,5,206,0,0,731,732,3,72,36,0,732,105,1,0,0,0,90,139,146,153,158,
  	164,166,171,182,186,193,201,208,214,216,222,231,239,247,255,263,269,271,
  	278,282,284,293,295,301,303,307,309,319,324,336,355,365,368,374,381,398,
  	418,427,433,440,442,454,466,473,475,483,485,498,503,505,514,521,525,534,
  	544,550,554,556,562,569,575,577,583,590,596,598,605,607,613,619,624,629,
  	635,639,641,654,666,678,680,689,700,703,711,724,726,728
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
    setState(106);
    match(TracerCommandsParser::T__0);
    setState(107);
    command();
    setState(108);
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

TracerCommandsParser::ObjectProbeContext* TracerCommandsParser::CommandContext::objectProbe() {
  return getRuleContext<TracerCommandsParser::ObjectProbeContext>(0);
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
    setState(139);
    _errHandler->sync(this);
    switch (getInterpreter<atn::ParserATNSimulator>()->adaptivePredict(_input, 0, _ctx)) {
    case 1: {
      enterOuterAlt(_localctx, 1);
      setState(110);
      server();
      break;
    }

    case 2: {
      enterOuterAlt(_localctx, 2);
      setState(111);
      attach();
      break;
    }

    case 3: {
      enterOuterAlt(_localctx, 3);
      setState(112);
      processes();
      break;
    }

    case 4: {
      enterOuterAlt(_localctx, 4);
      setState(113);
      sessions();
      break;
    }

    case 5: {
      enterOuterAlt(_localctx, 5);
      setState(114);
      use();
      break;
    }

    case 6: {
      enterOuterAlt(_localctx, 6);
      setState(115);
      status();
      break;
    }

    case 7: {
      enterOuterAlt(_localctx, 7);
      setState(116);
      detach();
      break;
    }

    case 8: {
      enterOuterAlt(_localctx, 8);
      setState(117);
      modules();
      break;
    }

    case 9: {
      enterOuterAlt(_localctx, 9);
      setState(118);
      moduleIndex();
      break;
    }

    case 10: {
      enterOuterAlt(_localctx, 10);
      setState(119);
      threads();
      break;
    }

    case 11: {
      enterOuterAlt(_localctx, 11);
      setState(120);
      symbols();
      break;
    }

    case 12: {
      enterOuterAlt(_localctx, 12);
      setState(121);
      probe();
      break;
    }

    case 13: {
      enterOuterAlt(_localctx, 13);
      setState(122);
      interceptor();
      break;
    }

    case 14: {
      enterOuterAlt(_localctx, 14);
      setState(123);
      events();
      break;
    }

    case 15: {
      enterOuterAlt(_localctx, 15);
      setState(124);
      collect();
      break;
    }

    case 16: {
      enterOuterAlt(_localctx, 16);
      setState(125);
      record();
      break;
    }

    case 17: {
      enterOuterAlt(_localctx, 17);
      setState(126);
      cpp();
      break;
    }

    case 18: {
      enterOuterAlt(_localctx, 18);
      setState(127);
      shell();
      break;
    }

    case 19: {
      enterOuterAlt(_localctx, 19);
      setState(128);
      complete();
      break;
    }

    case 20: {
      enterOuterAlt(_localctx, 20);
      setState(129);
      type();
      break;
    }

    case 21: {
      enterOuterAlt(_localctx, 21);
      setState(130);
      help();
      break;
    }

    case 22: {
      enterOuterAlt(_localctx, 22);
      setState(131);
      quit();
      break;
    }

    case 23: {
      enterOuterAlt(_localctx, 23);
      setState(132);
      bulkTrace();
      break;
    }

    case 24: {
      enterOuterAlt(_localctx, 24);
      setState(133);
      memory();
      break;
    }

    case 25: {
      enterOuterAlt(_localctx, 25);
      setState(134);
      probeEntry();
      break;
    }

    case 26: {
      enterOuterAlt(_localctx, 26);
      setState(135);
      probeReload();
      break;
    }

    case 27: {
      enterOuterAlt(_localctx, 27);
      setState(136);
      objectWatch();
      break;
    }

    case 28: {
      enterOuterAlt(_localctx, 28);
      setState(137);
      objectProbe();
      break;
    }

    case 29: {
      enterOuterAlt(_localctx, 29);
      setState(138);
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
    setState(141);
    match(TracerCommandsParser::T__1);
    setState(153);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__2: {
        setState(142);
        match(TracerCommandsParser::T__2);
        setState(146);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__6

        || _la == TracerCommandsParser::T__7 || _la == TracerCommandsParser::T__195

        || _la == TracerCommandsParser::T__196) {
          setState(143);
          serverOption();
          setState(148);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__3: {
        setState(149);
        match(TracerCommandsParser::T__3);
        setState(150);
        value();
        break;
      }

      case TracerCommandsParser::T__4: {
        setState(151);
        match(TracerCommandsParser::T__4);
        break;
      }

      case TracerCommandsParser::T__5: {
        setState(152);
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
    setState(158);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__6:
      case TracerCommandsParser::T__7: {
        enterOuterAlt(_localctx, 1);
        setState(155);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__6

        || _la == TracerCommandsParser::T__7)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(156);
        value();
        break;
      }

      case TracerCommandsParser::T__195:
      case TracerCommandsParser::T__196: {
        enterOuterAlt(_localctx, 2);
        setState(157);
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
    setState(160);
    match(TracerCommandsParser::T__8);
    setState(166);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::UINT) {
      setState(161);
      match(TracerCommandsParser::UINT);
      setState(164);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == TracerCommandsParser::T__9) {
        setState(162);
        match(TracerCommandsParser::T__9);
        setState(163);
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
    setState(168);
    match(TracerCommandsParser::T__10);
    setState(171);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::T__11) {
      setState(169);
      match(TracerCommandsParser::T__11);
      setState(170);
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
    setState(173);
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
    setState(175);
    match(TracerCommandsParser::T__13);
    setState(176);
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
    setState(178);
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
    setState(180);
    match(TracerCommandsParser::T__14);
    setState(182);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (((((_la - 2) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 2)) & -9222846186824697185) != 0) || ((((_la - 66) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 66)) & -527766067382155) != 0) || ((((_la - 200) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 200)) & 5155) != 0)) {
      setState(181);
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
    setState(184);
    match(TracerCommandsParser::T__15);
    setState(186);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::T__16) {
      setState(185);
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
    setState(188);
    match(TracerCommandsParser::T__17);
    setState(216);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__18: {
        setState(189);
        match(TracerCommandsParser::T__18);
        setState(191); 
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(190);
          moduleBuildOption();
          setState(193); 
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 2130706432) != 0));
        break;
      }

      case TracerCommandsParser::T__13: {
        setState(195);
        match(TracerCommandsParser::T__13);
        setState(196);
        value();
        break;
      }

      case TracerCommandsParser::T__19:
      case TracerCommandsParser::T__20: {
        setState(197);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__19

        || _la == TracerCommandsParser::T__20)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(201);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 100931735552) != 0)) {
          setState(198);
          moduleQueryOption();
          setState(203);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__21: {
        setState(204);
        match(TracerCommandsParser::T__21);
        setState(206); 
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(205);
          moduleShowOption();
          setState(208); 
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 964220157952) != 0));
        break;
      }

      case TracerCommandsParser::T__22: {
        setState(210);
        match(TracerCommandsParser::T__22);
        setState(212); 
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(211);
          moduleEmitOption();
          setState(214); 
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
    setState(222);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__23:
      case TracerCommandsParser::T__24:
      case TracerCommandsParser::T__25:
      case TracerCommandsParser::T__26:
      case TracerCommandsParser::T__27: {
        enterOuterAlt(_localctx, 1);
        setState(218);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 520093696) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(219);
        value();
        break;
      }

      case TracerCommandsParser::T__28:
      case TracerCommandsParser::T__29: {
        enterOuterAlt(_localctx, 2);
        setState(220);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__28

        || _la == TracerCommandsParser::T__29)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(221);
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
    setState(231);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__11:
      case TracerCommandsParser::T__30:
      case TracerCommandsParser::T__31: {
        enterOuterAlt(_localctx, 1);
        setState(224);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 6442455040) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(225);
        value();
        break;
      }

      case TracerCommandsParser::T__32: {
        enterOuterAlt(_localctx, 2);
        setState(226);
        match(TracerCommandsParser::T__32);
        setState(227);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__33: {
        enterOuterAlt(_localctx, 3);
        setState(228);
        match(TracerCommandsParser::T__33);
        setState(229);
        match(TracerCommandsParser::T__34);
        break;
      }

      case TracerCommandsParser::T__35: {
        enterOuterAlt(_localctx, 4);
        setState(230);
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
    setState(233);
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 964220157952) != 0))) {
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
    setState(239);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__30:
      case TracerCommandsParser::T__36:
      case TracerCommandsParser::T__37:
      case TracerCommandsParser::T__38: {
        enterOuterAlt(_localctx, 1);
        setState(236);
        moduleShowOption();
        break;
      }

      case TracerCommandsParser::T__26: {
        enterOuterAlt(_localctx, 2);
        setState(237);
        match(TracerCommandsParser::T__26);
        setState(238);
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
    setState(241);
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
    setState(243);
    match(TracerCommandsParser::T__20);
    setState(247);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 6610055335936) != 0)) {
      setState(244);
      symbolOption();
      setState(249);
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
    setState(255);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__11:
      case TracerCommandsParser::T__24:
      case TracerCommandsParser::T__25:
      case TracerCommandsParser::T__31:
      case TracerCommandsParser::T__40: {
        enterOuterAlt(_localctx, 1);
        setState(250);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 2203418890240) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(251);
        value();
        break;
      }

      case TracerCommandsParser::T__32: {
        enterOuterAlt(_localctx, 2);
        setState(252);
        match(TracerCommandsParser::T__32);
        setState(253);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__41: {
        enterOuterAlt(_localctx, 3);
        setState(254);
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
    setState(309);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__42: {
        enterOuterAlt(_localctx, 1);
        setState(257);
        match(TracerCommandsParser::T__42);
        setState(303);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__43: {
            setState(258);
            match(TracerCommandsParser::T__43);
            setState(259);
            value();
            setState(261); 
            _errHandler->sync(this);
            _la = _input->LA(1);
            do {
              setState(260);
              prepareOption();
              setState(263); 
              _errHandler->sync(this);
              _la = _input->LA(1);
            } while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 69806344147828736) != 0) || ((((_la - 198) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 198)) & 1827) != 0));
            break;
          }

          case TracerCommandsParser::T__44: {
            setState(265);
            match(TracerCommandsParser::T__44);
            setState(266);
            value();
            setState(271);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 520165756994846720) != 0) || ((((_la - 198) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 198)) & 259) != 0)) {
              setState(269);
              _errHandler->sync(this);
              switch (_input->LA(1)) {
                case TracerCommandsParser::T__24:
                case TracerCommandsParser::T__50:
                case TracerCommandsParser::T__51:
                case TracerCommandsParser::T__52:
                case TracerCommandsParser::T__55:
                case TracerCommandsParser::T__56:
                case TracerCommandsParser::T__57:
                case TracerCommandsParser::T__197:
                case TracerCommandsParser::T__198: {
                  setState(267);
                  resolveOption();
                  break;
                }

                case TracerCommandsParser::CWD: {
                  setState(268);
                  cwdOption();
                  break;
                }

              default:
                throw NoViableAltException(this);
              }
              setState(273);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__45: {
            setState(274);
            match(TracerCommandsParser::T__45);
            setState(275);
            value();
            setState(278);
            _errHandler->sync(this);

            _la = _input->LA(1);
            if (_la == TracerCommandsParser::T__46) {
              setState(276);
              match(TracerCommandsParser::T__46);
              setState(277);
              value();
            }
            setState(284);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while (((((_la - 25) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 25)) & 1097833906177) != 0) || ((((_la - 195) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 195)) & 18457) != 0)) {
              setState(282);
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
                case TracerCommandsParser::T__194:
                case TracerCommandsParser::T__197:
                case TracerCommandsParser::T__198:
                case TracerCommandsParser::SNAPSHOT_RETURN_POINTEE: {
                  setState(280);
                  deployOption();
                  break;
                }

                case TracerCommandsParser::CWD: {
                  setState(281);
                  cwdOption();
                  break;
                }

              default:
                throw NoViableAltException(this);
              }
              setState(286);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__47: {
            setState(287);
            match(TracerCommandsParser::T__47);
            setState(288);
            value();
            setState(289);
            match(TracerCommandsParser::T__46);
            setState(290);
            value();
            setState(295);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 520165756994846720) != 0) || ((((_la - 198) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 198)) & 259) != 0)) {
              setState(293);
              _errHandler->sync(this);
              switch (_input->LA(1)) {
                case TracerCommandsParser::T__24:
                case TracerCommandsParser::T__50:
                case TracerCommandsParser::T__51:
                case TracerCommandsParser::T__52:
                case TracerCommandsParser::T__55:
                case TracerCommandsParser::T__56:
                case TracerCommandsParser::T__57:
                case TracerCommandsParser::T__197:
                case TracerCommandsParser::T__198: {
                  setState(291);
                  resolveOption();
                  break;
                }

                case TracerCommandsParser::CWD: {
                  setState(292);
                  cwdOption();
                  break;
                }

              default:
                throw NoViableAltException(this);
              }
              setState(297);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__48: {
            setState(298);
            match(TracerCommandsParser::T__48);
            setState(299);
            match(TracerCommandsParser::UINT);
            setState(301);
            _errHandler->sync(this);

            _la = _input->LA(1);
            if (_la == TracerCommandsParser::CWD) {
              setState(300);
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
        setState(305);
        match(TracerCommandsParser::T__49);
        setState(307);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::CWD) {
          setState(306);
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
    setState(319);
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
        setState(311);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 69806344147828736) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(312);
        value();
        break;
      }

      case TracerCommandsParser::SIGNATURE: {
        enterOuterAlt(_localctx, 2);
        setState(313);
        match(TracerCommandsParser::SIGNATURE);
        setState(314);
        value();
        break;
      }

      case TracerCommandsParser::MEMBER: {
        enterOuterAlt(_localctx, 3);
        setState(315);
        match(TracerCommandsParser::MEMBER);
        break;
      }

      case TracerCommandsParser::METADATA_ONLY: {
        enterOuterAlt(_localctx, 4);
        setState(316);
        match(TracerCommandsParser::METADATA_ONLY);
        break;
      }

      case TracerCommandsParser::T__197:
      case TracerCommandsParser::T__198: {
        enterOuterAlt(_localctx, 5);
        setState(317);
        extraCompileOption();
        break;
      }

      case TracerCommandsParser::CWD: {
        enterOuterAlt(_localctx, 6);
        setState(318);
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
    setState(324);
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
        setState(321);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 520165756994846720) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(322);
        value();
        break;
      }

      case TracerCommandsParser::T__197:
      case TracerCommandsParser::T__198: {
        enterOuterAlt(_localctx, 2);
        setState(323);
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
    setState(336);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__24:
      case TracerCommandsParser::T__50:
      case TracerCommandsParser::T__51:
      case TracerCommandsParser::T__52:
      case TracerCommandsParser::T__55:
      case TracerCommandsParser::T__56:
      case TracerCommandsParser::T__57:
      case TracerCommandsParser::T__197:
      case TracerCommandsParser::T__198: {
        enterOuterAlt(_localctx, 1);
        setState(326);
        resolveOption();
        break;
      }

      case TracerCommandsParser::T__58: {
        enterOuterAlt(_localctx, 2);
        setState(327);
        match(TracerCommandsParser::T__58);
        setState(328);
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
        setState(329);
        match(TracerCommandsParser::T__59);
        break;
      }

      case TracerCommandsParser::T__60: {
        enterOuterAlt(_localctx, 4);
        setState(330);
        match(TracerCommandsParser::T__60);
        break;
      }

      case TracerCommandsParser::SNAPSHOT_RETURN_POINTEE: {
        enterOuterAlt(_localctx, 5);
        setState(331);
        match(TracerCommandsParser::SNAPSHOT_RETURN_POINTEE);
        break;
      }

      case TracerCommandsParser::T__61: {
        enterOuterAlt(_localctx, 6);
        setState(332);
        match(TracerCommandsParser::T__61);
        break;
      }

      case TracerCommandsParser::T__194: {
        enterOuterAlt(_localctx, 7);
        setState(333);
        stackFilter();
        break;
      }

      case TracerCommandsParser::T__62:
      case TracerCommandsParser::T__63: {
        enterOuterAlt(_localctx, 8);
        setState(334);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__62

        || _la == TracerCommandsParser::T__63)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(335);
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
    setState(338);
    match(TracerCommandsParser::T__64);
    setState(368);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__65: {
        setState(339);
        match(TracerCommandsParser::T__65);
        setState(340);
        value();
        setState(341);
        match(TracerCommandsParser::T__66);
        setState(342);
        value();
        break;
      }

      case TracerCommandsParser::T__67: {
        setState(344);
        match(TracerCommandsParser::T__67);
        setState(345);
        value();
        break;
      }

      case TracerCommandsParser::T__21: {
        setState(346);
        match(TracerCommandsParser::T__21);
        setState(347);
        value();
        break;
      }

      case TracerCommandsParser::T__47: {
        setState(348);
        match(TracerCommandsParser::T__47);
        setState(349);
        value();
        setState(350);
        match(TracerCommandsParser::T__68);
        setState(351);
        value();
        setState(355);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 520165756994846720) != 0) || _la == TracerCommandsParser::T__197

        || _la == TracerCommandsParser::T__198) {
          setState(352);
          resolveOption();
          setState(357);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__69: {
        setState(358);
        match(TracerCommandsParser::T__69);
        setState(359);
        value();
        setState(360);
        match(TracerCommandsParser::T__68);
        setState(361);
        value();
        setState(365);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (((((_la - 25) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 25)) & 1097833906177) != 0) || ((((_la - 195) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 195)) & 16409) != 0)) {
          setState(362);
          deployOption();
          setState(367);
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
    setState(370);
    match(TracerCommandsParser::T__70);
    setState(427);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::EOF:
      case TracerCommandsParser::T__66:
      case TracerCommandsParser::T__80:
      case TracerCommandsParser::T__84:
      case TracerCommandsParser::T__85:
      case TracerCommandsParser::T__86:
      case TracerCommandsParser::T__87:
      case TracerCommandsParser::T__88:
      case TracerCommandsParser::T__91:
      case TracerCommandsParser::T__92:
      case TracerCommandsParser::T__93: {
        setState(374);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (((((_la - 67) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 67)) & 243023873) != 0)) {
          setState(371);
          streamOption();
          setState(376);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__71: {
        setState(377);
        match(TracerCommandsParser::T__71);
        setState(378);
        value();
        setState(381);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__32) {
          setState(379);
          match(TracerCommandsParser::T__32);
          setState(380);
          match(TracerCommandsParser::UINT);
        }
        break;
      }

      case TracerCommandsParser::T__21: {
        setState(383);
        match(TracerCommandsParser::T__21);
        setState(384);
        value();
        setState(385);
        match(TracerCommandsParser::T__30);
        setState(386);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__72: {
        setState(388);
        match(TracerCommandsParser::T__72);
        setState(389);
        value();
        setState(390);
        match(TracerCommandsParser::T__73);
        setState(391);
        match(TracerCommandsParser::UINT);
        setState(392);
        match(TracerCommandsParser::T__74);
        setState(393);
        match(TracerCommandsParser::UINT);
        setState(398);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__75) {
          setState(394);
          match(TracerCommandsParser::T__75);
          setState(395);
          value();
          setState(400);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::STACK: {
        setState(401);
        match(TracerCommandsParser::STACK);
        setState(402);
        value();
        setState(403);
        match(TracerCommandsParser::T__30);
        setState(404);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__76: {
        setState(406);
        match(TracerCommandsParser::T__76);
        setState(407);
        value();
        break;
      }

      case TracerCommandsParser::T__77: {
        setState(408);
        match(TracerCommandsParser::T__77);
        setState(409);
        value();
        break;
      }

      case TracerCommandsParser::T__78: {
        setState(410);
        match(TracerCommandsParser::T__78);
        setState(411);
        value();
        break;
      }

      case TracerCommandsParser::T__79: {
        setState(412);
        match(TracerCommandsParser::T__79);
        setState(413);
        value();
        setState(414);
        match(TracerCommandsParser::T__66);
        setState(415);
        value();
        setState(418);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__80) {
          setState(416);
          match(TracerCommandsParser::T__80);
          setState(417);
          value();
        }
        break;
      }

      case TracerCommandsParser::T__81: {
        setState(420);
        match(TracerCommandsParser::T__81);
        setState(421);
        value();
        setState(422);
        match(TracerCommandsParser::T__33);
        setState(423);
        match(TracerCommandsParser::T__34);
        setState(424);
        match(TracerCommandsParser::T__26);
        setState(425);
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
    setState(429);
    match(TracerCommandsParser::T__82);
    setState(433);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (((((_la - 67) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 67)) & 243023873) != 0)) {
      setState(430);
      streamOption();
      setState(435);
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
    setState(436);
    match(TracerCommandsParser::T__83);
    setState(437);
    value();
    setState(442);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (((((_la - 67) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 67)) & 243023873) != 0) || _la == TracerCommandsParser::SQLITE) {
      setState(440);
      _errHandler->sync(this);
      switch (_input->LA(1)) {
        case TracerCommandsParser::T__66:
        case TracerCommandsParser::T__80:
        case TracerCommandsParser::T__84:
        case TracerCommandsParser::T__85:
        case TracerCommandsParser::T__86:
        case TracerCommandsParser::T__87:
        case TracerCommandsParser::T__88:
        case TracerCommandsParser::T__91:
        case TracerCommandsParser::T__92:
        case TracerCommandsParser::T__93: {
          setState(438);
          streamOption();
          break;
        }

        case TracerCommandsParser::SQLITE: {
          setState(439);
          match(TracerCommandsParser::SQLITE);
          break;
        }

      default:
        throw NoViableAltException(this);
      }
      setState(444);
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
    setState(454);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__84:
      case TracerCommandsParser::T__85:
      case TracerCommandsParser::T__86:
      case TracerCommandsParser::T__87: {
        enterOuterAlt(_localctx, 1);
        setState(445);
        _la = _input->LA(1);
        if (!(((((_la - 85) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 85)) & 15) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(446);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__88: {
        enterOuterAlt(_localctx, 2);
        setState(447);
        match(TracerCommandsParser::T__88);
        setState(448);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__89

        || _la == TracerCommandsParser::T__90)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        break;
      }

      case TracerCommandsParser::T__80: {
        enterOuterAlt(_localctx, 3);
        setState(449);
        match(TracerCommandsParser::T__80);
        setState(450);
        value();
        break;
      }

      case TracerCommandsParser::T__66:
      case TracerCommandsParser::T__91:
      case TracerCommandsParser::T__92: {
        enterOuterAlt(_localctx, 4);
        setState(451);
        _la = _input->LA(1);
        if (!(((((_la - 67) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 67)) & 100663297) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(452);
        value();
        break;
      }

      case TracerCommandsParser::T__93: {
        enterOuterAlt(_localctx, 5);
        setState(453);
        match(TracerCommandsParser::T__93);
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
    setState(456);
    match(TracerCommandsParser::T__94);
    setState(485);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__65: {
        setState(457);
        match(TracerCommandsParser::T__65);
        setState(458);
        value();
        break;
      }

      case TracerCommandsParser::T__95: {
        setState(459);
        match(TracerCommandsParser::T__95);
        break;
      }

      case TracerCommandsParser::T__96:
      case TracerCommandsParser::T__97: {
        setState(460);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__96

        || _la == TracerCommandsParser::T__97)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(461);
        value();
        setState(466);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__26

        || _la == TracerCommandsParser::T__36) {
          setState(462);
          _la = _input->LA(1);
          if (!(_la == TracerCommandsParser::T__26

          || _la == TracerCommandsParser::T__36)) {
          _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(463);
          value();
          setState(468);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__98: {
        setState(469);
        match(TracerCommandsParser::T__98);
        setState(483);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__99: {
            setState(470);
            match(TracerCommandsParser::T__99);
            setState(473); 
            _errHandler->sync(this);
            _la = _input->LA(1);
            do {
              setState(473);
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
                case TracerCommandsParser::T__81:
                case TracerCommandsParser::T__82:
                case TracerCommandsParser::T__83:
                case TracerCommandsParser::T__89:
                case TracerCommandsParser::T__90:
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
                case TracerCommandsParser::T__109:
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
                case TracerCommandsParser::UINT:
                case TracerCommandsParser::STRING:
                case TracerCommandsParser::STACK:
                case TracerCommandsParser::RECOVER:
                case TracerCommandsParser::WORD: {
                  setState(471);
                  value();
                  break;
                }

                case TracerCommandsParser::SHELL: {
                  setState(472);
                  match(TracerCommandsParser::SHELL);
                  break;
                }

              default:
                throw NoViableAltException(this);
              }
              setState(475); 
              _errHandler->sync(this);
              _la = _input->LA(1);
            } while (((((_la - 2) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 2)) & -9222846186824697185) != 0) || ((((_la - 66) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 66)) & -527766067382155) != 0) || ((((_la - 200) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 200)) & 5159) != 0));
            break;
          }

          case TracerCommandsParser::T__78: {
            setState(477);
            match(TracerCommandsParser::T__78);
            break;
          }

          case TracerCommandsParser::T__100: {
            setState(478);
            match(TracerCommandsParser::T__100);
            setState(479);
            value();
            setState(480);
            match(TracerCommandsParser::T__23);
            setState(481);
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
    setState(487);
    match(TracerCommandsParser::T__101);
    setState(488);
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
    setState(490);
    match(TracerCommandsParser::T__102);
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
    setState(493);
    match(TracerCommandsParser::T__103);
    setState(494);
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
    setState(496);
    match(TracerCommandsParser::T__104);
    setState(505);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 2103400120314494) != 0) || ((((_la - 65) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 65)) & -1055532134764309) != 0) || _la == TracerCommandsParser::T__128 || ((((_la - 200) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 200)) & 5155) != 0)) {
      setState(498);
      _errHandler->sync(this);

      _la = _input->LA(1);
      if (_la == TracerCommandsParser::T__0) {
        setState(497);
        match(TracerCommandsParser::T__0);
      }
      setState(501); 
      _errHandler->sync(this);
      _la = _input->LA(1);
      do {
        setState(500);
        value();
        setState(503); 
        _errHandler->sync(this);
        _la = _input->LA(1);
      } while (((((_la - 2) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 2)) & -9222846186824697185) != 0) || ((((_la - 66) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 66)) & -527766067382155) != 0) || ((((_la - 200) & ~ 0x3fULL) == 0) &&
        ((1ULL << (_la - 200)) & 5155) != 0));
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
    setState(507);
    _la = _input->LA(1);
    if (!(_la == TracerCommandsParser::T__105

    || _la == TracerCommandsParser::T__106)) {
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
    setState(509);
    match(TracerCommandsParser::T__107);
    setState(525);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__108: {
        setState(510);
        match(TracerCommandsParser::T__108);
        setState(512); 
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(511);
          bulkTraceOption();
          setState(514); 
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while (_la == TracerCommandsParser::T__24

        || _la == TracerCommandsParser::T__50 || ((((_la - 111) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 111)) & 15) != 0));
        break;
      }

      case TracerCommandsParser::T__109: {
        setState(516);
        match(TracerCommandsParser::T__109);
        setState(517);
        match(TracerCommandsParser::T__110);
        setState(518);
        value();
        setState(521);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__46) {
          setState(519);
          match(TracerCommandsParser::T__46);
          setState(520);
          value();
        }
        break;
      }

      case TracerCommandsParser::T__48: {
        setState(523);
        match(TracerCommandsParser::T__48);
        setState(524);
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
    setState(527);
    _la = _input->LA(1);
    if (!(_la == TracerCommandsParser::T__24

    || _la == TracerCommandsParser::T__50 || ((((_la - 111) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 111)) & 15) != 0))) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(528);
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
    setState(534);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::STRING: {
        enterOuterAlt(_localctx, 1);
        setState(530);
        match(TracerCommandsParser::STRING);
        break;
      }

      case TracerCommandsParser::WORD: {
        enterOuterAlt(_localctx, 2);
        setState(531);
        match(TracerCommandsParser::WORD);
        break;
      }

      case TracerCommandsParser::UINT: {
        enterOuterAlt(_localctx, 3);
        setState(532);
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
      case TracerCommandsParser::T__81:
      case TracerCommandsParser::T__82:
      case TracerCommandsParser::T__83:
      case TracerCommandsParser::T__89:
      case TracerCommandsParser::T__90:
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
      case TracerCommandsParser::T__109:
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
      case TracerCommandsParser::STACK:
      case TracerCommandsParser::RECOVER: {
        enterOuterAlt(_localctx, 4);
        setState(533);
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

tree::TerminalNode* TracerCommandsParser::KeywordContext::RECOVER() {
  return getToken(TracerCommandsParser::RECOVER, 0);
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
    setState(536);
    _la = _input->LA(1);
    if (!(((((_la - 2) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 2)) & -9222846186824697185) != 0) || ((((_la - 66) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 66)) & -527766067382155) != 0) || _la == TracerCommandsParser::STACK

    || _la == TracerCommandsParser::RECOVER)) {
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

//----------------- ObjectProbeContext ------------------------------------------------------------------

TracerCommandsParser::ObjectProbeContext::ObjectProbeContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

TracerCommandsParser::ValueContext* TracerCommandsParser::ObjectProbeContext::value() {
  return getRuleContext<TracerCommandsParser::ValueContext>(0);
}

std::vector<TracerCommandsParser::ObjectProbePrepareOptionContext *> TracerCommandsParser::ObjectProbeContext::objectProbePrepareOption() {
  return getRuleContexts<TracerCommandsParser::ObjectProbePrepareOptionContext>();
}

TracerCommandsParser::ObjectProbePrepareOptionContext* TracerCommandsParser::ObjectProbeContext::objectProbePrepareOption(size_t i) {
  return getRuleContext<TracerCommandsParser::ObjectProbePrepareOptionContext>(i);
}

std::vector<TracerCommandsParser::ObjectProbeBindOptionContext *> TracerCommandsParser::ObjectProbeContext::objectProbeBindOption() {
  return getRuleContexts<TracerCommandsParser::ObjectProbeBindOptionContext>();
}

TracerCommandsParser::ObjectProbeBindOptionContext* TracerCommandsParser::ObjectProbeContext::objectProbeBindOption(size_t i) {
  return getRuleContext<TracerCommandsParser::ObjectProbeBindOptionContext>(i);
}

tree::TerminalNode* TracerCommandsParser::ObjectProbeContext::UINT() {
  return getToken(TracerCommandsParser::UINT, 0);
}


size_t TracerCommandsParser::ObjectProbeContext::getRuleIndex() const {
  return TracerCommandsParser::RuleObjectProbe;
}


std::any TracerCommandsParser::ObjectProbeContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitObjectProbe(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::ObjectProbeContext* TracerCommandsParser::objectProbe() {
  ObjectProbeContext *_localctx = _tracker.createInstance<ObjectProbeContext>(_ctx, getState());
  enterRule(_localctx, 76, TracerCommandsParser::RuleObjectProbe);
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
    setState(538);
    match(TracerCommandsParser::T__121);
    setState(556);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__43: {
        setState(539);
        match(TracerCommandsParser::T__43);
        setState(540);
        value();
        setState(542); 
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(541);
          objectProbePrepareOption();
          setState(544); 
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 15762598830014464) != 0) || ((((_la - 130) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 130)) & 31) != 0) || _la == TracerCommandsParser::T__197

        || _la == TracerCommandsParser::T__198);
        break;
      }

      case TracerCommandsParser::T__2:
      case TracerCommandsParser::T__108: {
        setState(546);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__2 || _la == TracerCommandsParser::T__108)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(548); 
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(547);
          objectProbeBindOption();
          setState(550); 
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while (_la == TracerCommandsParser::T__24 || ((((_la - 135) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 135)) & 255) != 0));
        break;
      }

      case TracerCommandsParser::T__4:
      case TracerCommandsParser::T__5: {
        setState(552);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__4

        || _la == TracerCommandsParser::T__5)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(554);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::UINT) {
          setState(553);
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

//----------------- ObjectProbePrepareOptionContext ------------------------------------------------------------------

TracerCommandsParser::ObjectProbePrepareOptionContext::ObjectProbePrepareOptionContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

TracerCommandsParser::ValueContext* TracerCommandsParser::ObjectProbePrepareOptionContext::value() {
  return getRuleContext<TracerCommandsParser::ValueContext>(0);
}

TracerCommandsParser::ExtraCompileOptionContext* TracerCommandsParser::ObjectProbePrepareOptionContext::extraCompileOption() {
  return getRuleContext<TracerCommandsParser::ExtraCompileOptionContext>(0);
}


size_t TracerCommandsParser::ObjectProbePrepareOptionContext::getRuleIndex() const {
  return TracerCommandsParser::RuleObjectProbePrepareOption;
}


std::any TracerCommandsParser::ObjectProbePrepareOptionContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitObjectProbePrepareOption(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::ObjectProbePrepareOptionContext* TracerCommandsParser::objectProbePrepareOption() {
  ObjectProbePrepareOptionContext *_localctx = _tracker.createInstance<ObjectProbePrepareOptionContext>(_ctx, getState());
  enterRule(_localctx, 78, TracerCommandsParser::RuleObjectProbePrepareOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(562);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__26:
      case TracerCommandsParser::T__50:
      case TracerCommandsParser::T__51:
      case TracerCommandsParser::T__52:
      case TracerCommandsParser::T__129:
      case TracerCommandsParser::T__130:
      case TracerCommandsParser::T__131:
      case TracerCommandsParser::T__132: {
        enterOuterAlt(_localctx, 1);
        setState(558);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 15762598830014464) != 0) || ((((_la - 130) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 130)) & 15) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(559);
        value();
        break;
      }

      case TracerCommandsParser::T__133: {
        enterOuterAlt(_localctx, 2);
        setState(560);
        match(TracerCommandsParser::T__133);
        break;
      }

      case TracerCommandsParser::T__197:
      case TracerCommandsParser::T__198: {
        enterOuterAlt(_localctx, 3);
        setState(561);
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

//----------------- ObjectProbeBindOptionContext ------------------------------------------------------------------

TracerCommandsParser::ObjectProbeBindOptionContext::ObjectProbeBindOptionContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

TracerCommandsParser::ValueContext* TracerCommandsParser::ObjectProbeBindOptionContext::value() {
  return getRuleContext<TracerCommandsParser::ValueContext>(0);
}


size_t TracerCommandsParser::ObjectProbeBindOptionContext::getRuleIndex() const {
  return TracerCommandsParser::RuleObjectProbeBindOption;
}


std::any TracerCommandsParser::ObjectProbeBindOptionContext::accept(tree::ParseTreeVisitor *visitor) {
  if (auto parserVisitor = dynamic_cast<TracerCommandsVisitor*>(visitor))
    return parserVisitor->visitObjectProbeBindOption(this);
  else
    return visitor->visitChildren(this);
}

TracerCommandsParser::ObjectProbeBindOptionContext* TracerCommandsParser::objectProbeBindOption() {
  ObjectProbeBindOptionContext *_localctx = _tracker.createInstance<ObjectProbeBindOptionContext>(_ctx, getState());
  enterRule(_localctx, 80, TracerCommandsParser::RuleObjectProbeBindOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(569);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__24:
      case TracerCommandsParser::T__134:
      case TracerCommandsParser::T__135:
      case TracerCommandsParser::T__136:
      case TracerCommandsParser::T__137:
      case TracerCommandsParser::T__138: {
        enterOuterAlt(_localctx, 1);
        setState(564);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__24 || ((((_la - 135) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 135)) & 31) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(565);
        value();
        break;
      }

      case TracerCommandsParser::T__139: {
        enterOuterAlt(_localctx, 2);
        setState(566);
        match(TracerCommandsParser::T__139);
        break;
      }

      case TracerCommandsParser::T__140: {
        enterOuterAlt(_localctx, 3);
        setState(567);
        match(TracerCommandsParser::T__140);
        break;
      }

      case TracerCommandsParser::T__141: {
        enterOuterAlt(_localctx, 4);
        setState(568);
        match(TracerCommandsParser::T__141);
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

//----------------- LiveUpdateContext ------------------------------------------------------------------

TracerCommandsParser::LiveUpdateContext::LiveUpdateContext(ParserRuleContext *parent, size_t invokingState)
  : ParserRuleContext(parent, invokingState) {
}

tree::TerminalNode* TracerCommandsParser::LiveUpdateContext::RECOVER() {
  return getToken(TracerCommandsParser::RECOVER, 0);
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
  enterRule(_localctx, 82, TracerCommandsParser::RuleLiveUpdate);
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
    setState(571);
    match(TracerCommandsParser::T__122);
    setState(607);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__108:
      case TracerCommandsParser::T__123:
      case TracerCommandsParser::T__128: {
        setState(572);
        _la = _input->LA(1);
        if (!(((((_la - 109) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 109)) & 1081345) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(575); 
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(575);
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
            case TracerCommandsParser::T__81:
            case TracerCommandsParser::T__82:
            case TracerCommandsParser::T__83:
            case TracerCommandsParser::T__89:
            case TracerCommandsParser::T__90:
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
            case TracerCommandsParser::T__109:
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
            case TracerCommandsParser::UINT:
            case TracerCommandsParser::STRING:
            case TracerCommandsParser::STACK:
            case TracerCommandsParser::RECOVER:
            case TracerCommandsParser::WORD: {
              setState(573);
              value();
              break;
            }

            case TracerCommandsParser::T__23:
            case TracerCommandsParser::T__24:
            case TracerCommandsParser::T__36:
            case TracerCommandsParser::T__40:
            case TracerCommandsParser::T__51:
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
            case TracerCommandsParser::PAYLOAD_LIMIT: {
              setState(574);
              liveOption();
              break;
            }

          default:
            throw NoViableAltException(this);
          }
          setState(577); 
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 6609336260225660) != 0) || ((((_la - 65) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 65)) & -1055532134764309) != 0) || ((((_la - 129) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 129)) & 134184961) != 0) || ((((_la - 200) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 200)) & 7203) != 0));
        break;
      }

      case TracerCommandsParser::T__4: {
        setState(579);
        match(TracerCommandsParser::T__4);
        break;
      }

      case TracerCommandsParser::RECOVER: {
        setState(580);
        match(TracerCommandsParser::RECOVER);
        break;
      }

      case TracerCommandsParser::T__124: {
        setState(581);
        match(TracerCommandsParser::T__124);
        setState(583);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::UINT) {
          setState(582);
          match(TracerCommandsParser::UINT);
        }
        break;
      }

      case TracerCommandsParser::T__125: {
        setState(585);
        match(TracerCommandsParser::T__125);
        setState(586);
        value();
        setState(588); 
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(587);
          liveOption();
          setState(590); 
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 4505936139911168) != 0) || ((((_la - 144) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 144)) & 4095) != 0) || _la == TracerCommandsParser::PAYLOAD_LIMIT);
        break;
      }

      case TracerCommandsParser::T__126: {
        setState(592);
        match(TracerCommandsParser::T__126);
        setState(593);
        value();
        setState(598);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 4505936139911168) != 0) || ((((_la - 143) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 143)) & 8191) != 0) || _la == TracerCommandsParser::PAYLOAD_LIMIT) {
          setState(596);
          _errHandler->sync(this);
          switch (_input->LA(1)) {
            case TracerCommandsParser::T__23:
            case TracerCommandsParser::T__24:
            case TracerCommandsParser::T__36:
            case TracerCommandsParser::T__40:
            case TracerCommandsParser::T__51:
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
            case TracerCommandsParser::PAYLOAD_LIMIT: {
              setState(594);
              liveOption();
              break;
            }

            case TracerCommandsParser::T__142: {
              setState(595);
              liveScanSelection();
              break;
            }

          default:
            throw NoViableAltException(this);
          }
          setState(600);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__127: {
        setState(601);
        match(TracerCommandsParser::T__127);
        setState(603); 
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(602);
          liveOption();
          setState(605); 
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 4505936139911168) != 0) || ((((_la - 144) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 144)) & 4095) != 0) || _la == TracerCommandsParser::PAYLOAD_LIMIT);
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
  enterRule(_localctx, 84, TracerCommandsParser::RuleLiveScanSelection);
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
    setState(609);
    match(TracerCommandsParser::T__142);
    setState(611); 
    _errHandler->sync(this);
    _la = _input->LA(1);
    do {
      setState(610);
      value();
      setState(613); 
      _errHandler->sync(this);
      _la = _input->LA(1);
    } while (((((_la - 2) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 2)) & -9222846186824697185) != 0) || ((((_la - 66) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 66)) & -527766067382155) != 0) || ((((_la - 200) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 200)) & 5155) != 0));
   
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

tree::TerminalNode* TracerCommandsParser::LiveOptionContext::PAYLOAD_LIMIT() {
  return getToken(TracerCommandsParser::PAYLOAD_LIMIT, 0);
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
  enterRule(_localctx, 86, TracerCommandsParser::RuleLiveOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(619);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__23:
      case TracerCommandsParser::T__24:
      case TracerCommandsParser::T__36:
      case TracerCommandsParser::T__40:
      case TracerCommandsParser::T__51:
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
      case TracerCommandsParser::T__153: {
        enterOuterAlt(_localctx, 1);
        setState(615);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 4505936139911168) != 0) || ((((_la - 144) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 144)) & 2047) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(616);
        value();
        break;
      }

      case TracerCommandsParser::T__154:
      case TracerCommandsParser::PAYLOAD_LIMIT: {
        enterOuterAlt(_localctx, 2);
        setState(617);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__154

        || _la == TracerCommandsParser::PAYLOAD_LIMIT)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(618);
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
  enterRule(_localctx, 88, TracerCommandsParser::RuleObjectWatch);
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
    setState(621);
    match(TracerCommandsParser::T__120);
    setState(641);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__43: {
        setState(622);
        match(TracerCommandsParser::T__43);
        setState(624);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (((((_la - 2) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 2)) & -9222846186824697185) != 0) || ((((_la - 66) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 66)) & -527766067382155) != 0) || ((((_la - 200) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 200)) & 5155) != 0)) {
          setState(623);
          value();
        }
        setState(627); 
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(626);
          objectOption();
          setState(629); 
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & -8631148547175088000) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 64)) & 4198401) != 0) || ((((_la - 132) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 132)) & 1152921504590071779) != 0));
        break;
      }

      case TracerCommandsParser::T__2:
      case TracerCommandsParser::T__108: {
        setState(631);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__2 || _la == TracerCommandsParser::T__108)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(633); 
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(632);
          objectOption();
          setState(635); 
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & -8631148547175088000) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 64)) & 4198401) != 0) || ((((_la - 132) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 132)) & 1152921504590071779) != 0));
        break;
      }

      case TracerCommandsParser::T__4:
      case TracerCommandsParser::T__5: {
        setState(637);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__4

        || _la == TracerCommandsParser::T__5)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(639);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::UINT) {
          setState(638);
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
  enterRule(_localctx, 90, TracerCommandsParser::RuleObjectOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(654);
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
      case TracerCommandsParser::T__131:
      case TracerCommandsParser::T__132:
      case TracerCommandsParser::T__136:
      case TracerCommandsParser::T__137:
      case TracerCommandsParser::T__138:
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
      case TracerCommandsParser::T__165:
      case TracerCommandsParser::T__166:
      case TracerCommandsParser::T__167:
      case TracerCommandsParser::T__168:
      case TracerCommandsParser::T__169:
      case TracerCommandsParser::T__170:
      case TracerCommandsParser::T__171:
      case TracerCommandsParser::T__172:
      case TracerCommandsParser::T__173:
      case TracerCommandsParser::T__174:
      case TracerCommandsParser::T__175:
      case TracerCommandsParser::T__176: {
        enterOuterAlt(_localctx, 1);
        setState(643);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & -9207609300552253312) != 0) || _la == TracerCommandsParser::T__63

        || _la == TracerCommandsParser::T__75 || ((((_la - 132) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 132)) & 70368727400675) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(644);
        value();
        break;
      }

      case TracerCommandsParser::T__29:
      case TracerCommandsParser::T__58:
      case TracerCommandsParser::T__85:
      case TracerCommandsParser::T__177:
      case TracerCommandsParser::T__178:
      case TracerCommandsParser::T__179:
      case TracerCommandsParser::T__180:
      case TracerCommandsParser::T__181:
      case TracerCommandsParser::T__182:
      case TracerCommandsParser::T__183:
      case TracerCommandsParser::T__184:
      case TracerCommandsParser::T__185:
      case TracerCommandsParser::T__186: {
        enterOuterAlt(_localctx, 2);
        setState(645);
        _la = _input->LA(1);
        if (!(((((_la - 30) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 30)) & 72057594574798849) != 0) || ((((_la - 178) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 178)) & 1023) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(646);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__140: {
        enterOuterAlt(_localctx, 3);
        setState(647);
        match(TracerCommandsParser::T__140);
        break;
      }

      case TracerCommandsParser::T__141: {
        enterOuterAlt(_localctx, 4);
        setState(648);
        match(TracerCommandsParser::T__141);
        break;
      }

      case TracerCommandsParser::T__187: {
        enterOuterAlt(_localctx, 5);
        setState(649);
        match(TracerCommandsParser::T__187);
        break;
      }

      case TracerCommandsParser::T__188: {
        enterOuterAlt(_localctx, 6);
        setState(650);
        match(TracerCommandsParser::T__188);
        break;
      }

      case TracerCommandsParser::T__139: {
        enterOuterAlt(_localctx, 7);
        setState(651);
        match(TracerCommandsParser::T__139);
        break;
      }

      case TracerCommandsParser::T__189: {
        enterOuterAlt(_localctx, 8);
        setState(652);
        match(TracerCommandsParser::T__189);
        break;
      }

      case TracerCommandsParser::T__190: {
        enterOuterAlt(_localctx, 9);
        setState(653);
        match(TracerCommandsParser::T__190);
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
  enterRule(_localctx, 92, TracerCommandsParser::RuleMemory);
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
    setState(656);
    match(TracerCommandsParser::T__114);
    setState(680);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__115: {
        setState(657);
        match(TracerCommandsParser::T__115);
        break;
      }

      case TracerCommandsParser::T__116: {
        setState(658);
        match(TracerCommandsParser::T__116);
        setState(659);
        value();
        setState(660);
        match(TracerCommandsParser::T__25);
        setState(661);
        value();
        setState(666);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__84: {
            setState(662);
            match(TracerCommandsParser::T__84);
            setState(663);
            match(TracerCommandsParser::UINT);
            break;
          }

          case TracerCommandsParser::T__36: {
            setState(664);
            match(TracerCommandsParser::T__36);
            setState(665);
            value();
            break;
          }

        default:
          throw NoViableAltException(this);
        }
        break;
      }

      case TracerCommandsParser::T__117: {
        setState(668);
        match(TracerCommandsParser::T__117);
        setState(669);
        value();
        setState(670);
        match(TracerCommandsParser::T__25);
        setState(671);
        value();
        setState(672);
        match(TracerCommandsParser::T__84);
        setState(673);
        match(TracerCommandsParser::UINT);
        setState(674);
        match(TracerCommandsParser::T__191);
        setState(675);
        value();
        setState(678);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__32) {
          setState(676);
          match(TracerCommandsParser::T__32);
          setState(677);
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
  enterRule(_localctx, 94, TracerCommandsParser::RuleProbeEntry);
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
    match(TracerCommandsParser::T__42);
    setState(683);
    match(TracerCommandsParser::T__118);
    setState(700);
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
      case TracerCommandsParser::T__81:
      case TracerCommandsParser::T__82:
      case TracerCommandsParser::T__83:
      case TracerCommandsParser::T__89:
      case TracerCommandsParser::T__90:
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
      case TracerCommandsParser::T__109:
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
      case TracerCommandsParser::UINT:
      case TracerCommandsParser::STRING:
      case TracerCommandsParser::STACK:
      case TracerCommandsParser::RECOVER:
      case TracerCommandsParser::WORD: {
        setState(684);
        value();
        setState(685);
        match(TracerCommandsParser::T__25);
        setState(686);
        value();
        setState(689);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__24) {
          setState(687);
          match(TracerCommandsParser::T__24);
          setState(688);
          value();
        }
        break;
      }

      case TracerCommandsParser::T__24: {
        setState(691);
        match(TracerCommandsParser::T__24);
        setState(692);
        value();
        setState(693);
        match(TracerCommandsParser::T__192);
        setState(694);
        value();
        setState(695);
        match(TracerCommandsParser::T__193);
        setState(696);
        value();
        setState(697);
        match(TracerCommandsParser::T__25);
        setState(698);
        value();
        break;
      }

    default:
      throw NoViableAltException(this);
    }
    setState(703);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::CWD) {
      setState(702);
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
  enterRule(_localctx, 96, TracerCommandsParser::RuleProbeReload);
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
    setState(705);
    match(TracerCommandsParser::T__42);
    setState(706);
    match(TracerCommandsParser::T__119);
    setState(707);
    match(TracerCommandsParser::UINT);
    setState(708);
    match(TracerCommandsParser::T__46);
    setState(709);
    value();
    setState(711);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::CWD) {
      setState(710);
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
  enterRule(_localctx, 98, TracerCommandsParser::RuleStackFilter);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(713);
    match(TracerCommandsParser::T__194);
    setState(714);
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
  enterRule(_localctx, 100, TracerCommandsParser::RuleContextEventBytes);
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
    setState(716);
    _la = _input->LA(1);
    if (!(_la == TracerCommandsParser::T__195

    || _la == TracerCommandsParser::T__196)) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(717);
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
  enterRule(_localctx, 102, TracerCommandsParser::RuleExtraCompileOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(728);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__197: {
        enterOuterAlt(_localctx, 1);
        setState(719);
        match(TracerCommandsParser::T__197);
        setState(720);
        value();
        break;
      }

      case TracerCommandsParser::T__198: {
        enterOuterAlt(_localctx, 2);
        setState(721);
        match(TracerCommandsParser::T__198);
        setState(724); 
        _errHandler->sync(this);
        _la = _input->LA(1);
        do {
          setState(724);
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
            case TracerCommandsParser::T__81:
            case TracerCommandsParser::T__82:
            case TracerCommandsParser::T__83:
            case TracerCommandsParser::T__89:
            case TracerCommandsParser::T__90:
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
            case TracerCommandsParser::T__109:
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
            case TracerCommandsParser::UINT:
            case TracerCommandsParser::STRING:
            case TracerCommandsParser::STACK:
            case TracerCommandsParser::RECOVER:
            case TracerCommandsParser::WORD: {
              setState(722);
              value();
              break;
            }

            case TracerCommandsParser::SHELL: {
              setState(723);
              match(TracerCommandsParser::SHELL);
              break;
            }

          default:
            throw NoViableAltException(this);
          }
          setState(726); 
          _errHandler->sync(this);
          _la = _input->LA(1);
        } while (((((_la - 2) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 2)) & -9222846186824697185) != 0) || ((((_la - 66) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 66)) & -527766067382155) != 0) || ((((_la - 200) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 200)) & 5159) != 0));
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
  enterRule(_localctx, 104, TracerCommandsParser::RuleCwdOption);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(730);
    match(TracerCommandsParser::CWD);
    setState(731);
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
