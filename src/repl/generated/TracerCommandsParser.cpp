
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
      "sessions", "use", "status", "detach", "modules", "symbols", "symbolOption",
      "probe", "prepareOption", "resolveOption", "deployOption", "interceptor",
      "events", "collect", "record", "streamOption", "cpp", "shell", "complete",
      "help", "quit", "value", "keyword"
    },
    std::vector<std::string>{
      "", "'%'", "'server'", "'start'", "'connect'", "'status'", "'stop'",
      "'--executable'", "'--agent'", "'attach'", "'processes'", "'--filter'",
      "'sessions'", "'use'", "'detach'", "'modules'", "'symbols'", "'--module'",
      "'--page'", "'--limit'", "'probe'", "'prepare'", "'resolve'", "'inject'",
      "'--callback'", "'validate'", "'remove'", "'probes'", "'--include'",
      "'--compdb'", "'--translation-unit'", "'--output'", "'--compile-option'",
      "'--prepared'", "'--prepared-mode'", "'--stack-depth'", "'--print-arguments'",
      "'--print-result'", "'interceptor'", "'load'", "'--as'", "'edit'",
      "'show'", "'upload'", "'--target'", "'events'", "'list'", "'--index'",
      "'calls'", "'stats'", "'clear'", "'open'", "'export'", "'--format'",
      "'json'", "'collect'", "'record'", "'--count'", "'--timeout'", "'--after'",
      "'--probe'", "'--phase'", "'PRE'", "'POST'", "'--name'", "'--session'",
      "'--all'", "'cpp'", "'reset'", "'import'", "'generate'", "'--type'",
      "'options'", "'add'", "'compdb'", "'--file'", "'shell'", "'complete'",
      "'help'", "'quit'", "'exit'"
    },
    std::vector<std::string>{
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
      "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
      "", "", "", "", "", "", "", "", "", "", "", "", "", "UINT", "STRING",
      "SHELL", "WORD", "WS"
    }
  );
  static const int32_t serializedATNSegment[] = {
    4,1,85,327,2,0,7,0,2,1,7,1,2,2,7,2,2,3,7,3,2,4,7,4,2,5,7,5,2,6,7,6,2,
    7,7,7,2,8,7,8,2,9,7,9,2,10,7,10,2,11,7,11,2,12,7,12,2,13,7,13,2,14,7,
    14,2,15,7,15,2,16,7,16,2,17,7,17,2,18,7,18,2,19,7,19,2,20,7,20,2,21,7,
    21,2,22,7,22,2,23,7,23,2,24,7,24,2,25,7,25,2,26,7,26,2,27,7,27,2,28,7,
    28,1,0,1,0,1,0,1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,3,1,82,8,1,1,2,1,2,1,2,5,2,87,8,2,10,2,12,2,
    90,9,2,1,2,1,2,1,2,1,2,3,2,96,8,2,1,3,1,3,1,3,1,4,1,4,3,4,103,8,4,1,5,
    1,5,1,5,3,5,108,8,5,1,6,1,6,1,7,1,7,1,7,1,8,1,8,1,9,1,9,3,9,119,8,9,1,
    10,1,10,1,11,1,11,5,11,125,8,11,10,11,12,11,128,9,11,1,12,1,12,1,12,1,
    12,3,12,134,8,12,1,13,1,13,1,13,1,13,4,13,140,8,13,11,13,12,13,141,1,
    13,1,13,1,13,5,13,147,8,13,10,13,12,13,150,9,13,1,13,1,13,1,13,1,13,1,
    13,5,13,157,8,13,10,13,12,13,160,9,13,1,13,1,13,1,13,1,13,1,13,5,13,167,
    8,13,10,13,12,13,170,9,13,1,13,1,13,3,13,174,8,13,1,13,3,13,177,8,13,
    1,14,1,14,1,14,1,15,1,15,1,15,1,16,1,16,1,16,1,16,1,16,3,16,190,8,16,
    1,17,1,17,1,17,1,17,1,17,1,17,1,17,1,17,1,17,1,17,1,17,1,17,1,17,1,17,
    1,17,5,17,207,8,17,10,17,12,17,210,9,17,3,17,212,8,17,1,18,1,18,5,18,
    216,8,18,10,18,12,18,219,9,18,1,18,1,18,1,18,1,18,3,18,225,8,18,1,18,
    1,18,1,18,1,18,1,18,1,18,1,18,1,18,1,18,1,18,1,18,1,18,1,18,1,18,1,18,
    1,18,1,18,1,18,1,18,1,18,1,18,1,18,1,18,3,18,250,8,18,1,19,1,19,5,19,
    254,8,19,10,19,12,19,257,9,19,1,20,1,20,1,20,5,20,262,8,20,10,20,12,20,
    265,9,20,1,21,1,21,1,21,1,21,1,21,1,21,1,21,3,21,274,8,21,1,22,1,22,1,
    22,1,22,1,22,1,22,1,22,1,22,5,22,284,8,22,10,22,12,22,287,9,22,1,22,1,
    22,1,22,1,22,4,22,293,8,22,11,22,12,22,294,1,22,1,22,1,22,1,22,1,22,1,
    22,3,22,303,8,22,3,22,305,8,22,1,23,1,23,1,23,1,24,1,24,1,24,1,25,1,25,
    3,25,315,8,25,1,26,1,26,1,27,1,27,1,27,1,27,3,27,323,8,27,1,28,1,28,1,
    28,0,0,29,0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,34,36,38,40,42,
    44,46,48,50,52,54,56,0,11,1,0,7,8,2,0,11,11,17,18,2,0,17,17,28,31,3,0,
    17,17,28,29,32,34,1,0,57,60,1,0,62,63,2,0,40,40,64,65,1,0,69,70,2,0,31,
    31,71,71,1,0,79,80,14,0,2,6,9,10,12,16,20,23,25,27,38,39,41,43,45,46,
    48,52,54,56,62,63,67,70,72,74,76,80,367,0,58,1,0,0,0,2,81,1,0,0,0,4,83,
    1,0,0,0,6,97,1,0,0,0,8,100,1,0,0,0,10,104,1,0,0,0,12,109,1,0,0,0,14,111,
    1,0,0,0,16,114,1,0,0,0,18,116,1,0,0,0,20,120,1,0,0,0,22,122,1,0,0,0,24,
    133,1,0,0,0,26,176,1,0,0,0,28,178,1,0,0,0,30,181,1,0,0,0,32,189,1,0,0,
    0,34,191,1,0,0,0,36,213,1,0,0,0,38,251,1,0,0,0,40,258,1,0,0,0,42,273,
    1,0,0,0,44,275,1,0,0,0,46,306,1,0,0,0,48,309,1,0,0,0,50,312,1,0,0,0,52,
    316,1,0,0,0,54,322,1,0,0,0,56,324,1,0,0,0,58,59,5,1,0,0,59,60,3,2,1,0,
    60,61,5,0,0,1,61,1,1,0,0,0,62,82,3,4,2,0,63,82,3,8,4,0,64,82,3,10,5,0,
    65,82,3,12,6,0,66,82,3,14,7,0,67,82,3,16,8,0,68,82,3,18,9,0,69,82,3,20,
    10,0,70,82,3,22,11,0,71,82,3,26,13,0,72,82,3,34,17,0,73,82,3,36,18,0,
    74,82,3,38,19,0,75,82,3,40,20,0,76,82,3,44,22,0,77,82,3,46,23,0,78,82,
    3,48,24,0,79,82,3,50,25,0,80,82,3,52,26,0,81,62,1,0,0,0,81,63,1,0,0,0,
    81,64,1,0,0,0,81,65,1,0,0,0,81,66,1,0,0,0,81,67,1,0,0,0,81,68,1,0,0,0,
    81,69,1,0,0,0,81,70,1,0,0,0,81,71,1,0,0,0,81,72,1,0,0,0,81,73,1,0,0,0,
    81,74,1,0,0,0,81,75,1,0,0,0,81,76,1,0,0,0,81,77,1,0,0,0,81,78,1,0,0,0,
    81,79,1,0,0,0,81,80,1,0,0,0,82,3,1,0,0,0,83,95,5,2,0,0,84,88,5,3,0,0,
    85,87,3,6,3,0,86,85,1,0,0,0,87,90,1,0,0,0,88,86,1,0,0,0,88,89,1,0,0,0,
    89,96,1,0,0,0,90,88,1,0,0,0,91,92,5,4,0,0,92,96,3,54,27,0,93,96,5,5,0,
    0,94,96,5,6,0,0,95,84,1,0,0,0,95,91,1,0,0,0,95,93,1,0,0,0,95,94,1,0,0,
    0,96,5,1,0,0,0,97,98,7,0,0,0,98,99,3,54,27,0,99,7,1,0,0,0,100,102,5,9,
    0,0,101,103,5,81,0,0,102,101,1,0,0,0,102,103,1,0,0,0,103,9,1,0,0,0,104,
    107,5,10,0,0,105,106,5,11,0,0,106,108,3,54,27,0,107,105,1,0,0,0,107,108,
    1,0,0,0,108,11,1,0,0,0,109,110,5,12,0,0,110,13,1,0,0,0,111,112,5,13,0,
    0,112,113,3,54,27,0,113,15,1,0,0,0,114,115,5,5,0,0,115,17,1,0,0,0,116,
    118,5,14,0,0,117,119,3,54,27,0,118,117,1,0,0,0,118,119,1,0,0,0,119,19,
    1,0,0,0,120,121,5,15,0,0,121,21,1,0,0,0,122,126,5,16,0,0,123,125,3,24,
    12,0,124,123,1,0,0,0,125,128,1,0,0,0,126,124,1,0,0,0,126,127,1,0,0,0,
    127,23,1,0,0,0,128,126,1,0,0,0,129,130,7,1,0,0,130,134,3,54,27,0,131,
    132,5,19,0,0,132,134,5,81,0,0,133,129,1,0,0,0,133,131,1,0,0,0,134,25,
    1,0,0,0,135,173,5,20,0,0,136,137,5,21,0,0,137,139,3,54,27,0,138,140,3,
    28,14,0,139,138,1,0,0,0,140,141,1,0,0,0,141,139,1,0,0,0,141,142,1,0,0,
    0,142,174,1,0,0,0,143,144,5,22,0,0,144,148,3,54,27,0,145,147,3,30,15,
    0,146,145,1,0,0,0,147,150,1,0,0,0,148,146,1,0,0,0,148,149,1,0,0,0,149,
    174,1,0,0,0,150,148,1,0,0,0,151,152,5,23,0,0,152,153,3,54,27,0,153,154,
    5,24,0,0,154,158,3,54,27,0,155,157,3,32,16,0,156,155,1,0,0,0,157,160,
    1,0,0,0,158,156,1,0,0,0,158,159,1,0,0,0,159,174,1,0,0,0,160,158,1,0,0,
    0,161,162,5,25,0,0,162,163,3,54,27,0,163,164,5,24,0,0,164,168,3,54,27,
    0,165,167,3,30,15,0,166,165,1,0,0,0,167,170,1,0,0,0,168,166,1,0,0,0,168,
    169,1,0,0,0,169,174,1,0,0,0,170,168,1,0,0,0,171,172,5,26,0,0,172,174,
    5,81,0,0,173,136,1,0,0,0,173,143,1,0,0,0,173,151,1,0,0,0,173,161,1,0,
    0,0,173,171,1,0,0,0,174,177,1,0,0,0,175,177,5,27,0,0,176,135,1,0,0,0,
    176,175,1,0,0,0,177,27,1,0,0,0,178,179,7,2,0,0,179,180,3,54,27,0,180,
    29,1,0,0,0,181,182,7,3,0,0,182,183,3,54,27,0,183,31,1,0,0,0,184,190,3,
    30,15,0,185,186,5,35,0,0,186,190,5,81,0,0,187,190,5,36,0,0,188,190,5,
    37,0,0,189,184,1,0,0,0,189,185,1,0,0,0,189,187,1,0,0,0,189,188,1,0,0,
    0,190,33,1,0,0,0,191,211,5,38,0,0,192,193,5,39,0,0,193,194,3,54,27,0,
    194,195,5,40,0,0,195,196,3,54,27,0,196,212,1,0,0,0,197,198,5,41,0,0,198,
    212,3,54,27,0,199,200,5,42,0,0,200,212,3,54,27,0,201,202,5,43,0,0,202,
    203,3,54,27,0,203,204,5,44,0,0,204,208,3,54,27,0,205,207,3,32,16,0,206,
    205,1,0,0,0,207,210,1,0,0,0,208,206,1,0,0,0,208,209,1,0,0,0,209,212,1,
    0,0,0,210,208,1,0,0,0,211,192,1,0,0,0,211,197,1,0,0,0,211,199,1,0,0,0,
    211,201,1,0,0,0,212,35,1,0,0,0,213,249,5,45,0,0,214,216,3,42,21,0,215,
    214,1,0,0,0,216,219,1,0,0,0,217,215,1,0,0,0,217,218,1,0,0,0,218,250,1,
    0,0,0,219,217,1,0,0,0,220,221,5,46,0,0,221,224,3,54,27,0,222,223,5,19,
    0,0,223,225,5,81,0,0,224,222,1,0,0,0,224,225,1,0,0,0,225,250,1,0,0,0,
    226,227,5,42,0,0,227,228,3,54,27,0,228,229,5,47,0,0,229,230,5,81,0,0,
    230,250,1,0,0,0,231,232,5,48,0,0,232,250,3,54,27,0,233,234,5,49,0,0,234,
    250,3,54,27,0,235,236,5,50,0,0,236,250,3,54,27,0,237,238,5,51,0,0,238,
    239,3,54,27,0,239,240,5,40,0,0,240,241,3,54,27,0,241,250,1,0,0,0,242,
    243,5,52,0,0,243,244,3,54,27,0,244,245,5,53,0,0,245,246,5,54,0,0,246,
    247,5,31,0,0,247,248,3,54,27,0,248,250,1,0,0,0,249,217,1,0,0,0,249,220,
    1,0,0,0,249,226,1,0,0,0,249,231,1,0,0,0,249,233,1,0,0,0,249,235,1,0,0,
    0,249,237,1,0,0,0,249,242,1,0,0,0,250,37,1,0,0,0,251,255,5,55,0,0,252,
    254,3,42,21,0,253,252,1,0,0,0,254,257,1,0,0,0,255,253,1,0,0,0,255,256,
    1,0,0,0,256,39,1,0,0,0,257,255,1,0,0,0,258,259,5,56,0,0,259,263,3,54,
    27,0,260,262,3,42,21,0,261,260,1,0,0,0,262,265,1,0,0,0,263,261,1,0,0,
    0,263,264,1,0,0,0,264,41,1,0,0,0,265,263,1,0,0,0,266,267,7,4,0,0,267,
    274,5,81,0,0,268,269,5,61,0,0,269,274,7,5,0,0,270,271,7,6,0,0,271,274,
    3,54,27,0,272,274,5,66,0,0,273,266,1,0,0,0,273,268,1,0,0,0,273,270,1,
    0,0,0,273,272,1,0,0,0,274,43,1,0,0,0,275,304,5,67,0,0,276,277,5,39,0,
    0,277,305,3,54,27,0,278,305,5,68,0,0,279,280,7,7,0,0,280,285,3,54,27,
    0,281,282,7,8,0,0,282,284,3,54,27,0,283,281,1,0,0,0,284,287,1,0,0,0,285,
    283,1,0,0,0,285,286,1,0,0,0,286,305,1,0,0,0,287,285,1,0,0,0,288,302,5,
    72,0,0,289,292,5,73,0,0,290,293,3,54,27,0,291,293,5,83,0,0,292,290,1,
    0,0,0,292,291,1,0,0,0,293,294,1,0,0,0,294,292,1,0,0,0,294,295,1,0,0,0,
    295,303,1,0,0,0,296,303,5,50,0,0,297,298,5,74,0,0,298,299,3,54,27,0,299,
    300,5,75,0,0,300,301,3,54,27,0,301,303,1,0,0,0,302,289,1,0,0,0,302,296,
    1,0,0,0,302,297,1,0,0,0,302,303,1,0,0,0,303,305,1,0,0,0,304,276,1,0,0,
    0,304,278,1,0,0,0,304,279,1,0,0,0,304,288,1,0,0,0,305,45,1,0,0,0,306,
    307,5,76,0,0,307,308,3,54,27,0,308,47,1,0,0,0,309,310,5,77,0,0,310,311,
    3,54,27,0,311,49,1,0,0,0,312,314,5,78,0,0,313,315,3,54,27,0,314,313,1,
    0,0,0,314,315,1,0,0,0,315,51,1,0,0,0,316,317,7,9,0,0,317,53,1,0,0,0,318,
    323,5,82,0,0,319,323,5,84,0,0,320,323,5,81,0,0,321,323,3,56,28,0,322,
    318,1,0,0,0,322,319,1,0,0,0,322,320,1,0,0,0,322,321,1,0,0,0,323,55,1,
    0,0,0,324,325,7,10,0,0,325,57,1,0,0,0,30,81,88,95,102,107,118,126,133,
    141,148,158,168,173,176,189,208,211,217,224,249,255,263,273,285,292,294,
    302,304,314,322
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
    setState(58);
    match(TracerCommandsParser::T__0);
    setState(59);
    command();
    setState(60);
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

TracerCommandsParser::HelpContext* TracerCommandsParser::CommandContext::help() {
  return getRuleContext<TracerCommandsParser::HelpContext>(0);
}

TracerCommandsParser::QuitContext* TracerCommandsParser::CommandContext::quit() {
  return getRuleContext<TracerCommandsParser::QuitContext>(0);
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
    setState(81);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__1: {
        enterOuterAlt(_localctx, 1);
        setState(62);
        server();
        break;
      }

      case TracerCommandsParser::T__8: {
        enterOuterAlt(_localctx, 2);
        setState(63);
        attach();
        break;
      }

      case TracerCommandsParser::T__9: {
        enterOuterAlt(_localctx, 3);
        setState(64);
        processes();
        break;
      }

      case TracerCommandsParser::T__11: {
        enterOuterAlt(_localctx, 4);
        setState(65);
        sessions();
        break;
      }

      case TracerCommandsParser::T__12: {
        enterOuterAlt(_localctx, 5);
        setState(66);
        use();
        break;
      }

      case TracerCommandsParser::T__4: {
        enterOuterAlt(_localctx, 6);
        setState(67);
        status();
        break;
      }

      case TracerCommandsParser::T__13: {
        enterOuterAlt(_localctx, 7);
        setState(68);
        detach();
        break;
      }

      case TracerCommandsParser::T__14: {
        enterOuterAlt(_localctx, 8);
        setState(69);
        modules();
        break;
      }

      case TracerCommandsParser::T__15: {
        enterOuterAlt(_localctx, 9);
        setState(70);
        symbols();
        break;
      }

      case TracerCommandsParser::T__19:
      case TracerCommandsParser::T__26: {
        enterOuterAlt(_localctx, 10);
        setState(71);
        probe();
        break;
      }

      case TracerCommandsParser::T__37: {
        enterOuterAlt(_localctx, 11);
        setState(72);
        interceptor();
        break;
      }

      case TracerCommandsParser::T__44: {
        enterOuterAlt(_localctx, 12);
        setState(73);
        events();
        break;
      }

      case TracerCommandsParser::T__54: {
        enterOuterAlt(_localctx, 13);
        setState(74);
        collect();
        break;
      }

      case TracerCommandsParser::T__55: {
        enterOuterAlt(_localctx, 14);
        setState(75);
        record();
        break;
      }

      case TracerCommandsParser::T__66: {
        enterOuterAlt(_localctx, 15);
        setState(76);
        cpp();
        break;
      }

      case TracerCommandsParser::T__75: {
        enterOuterAlt(_localctx, 16);
        setState(77);
        shell();
        break;
      }

      case TracerCommandsParser::T__76: {
        enterOuterAlt(_localctx, 17);
        setState(78);
        complete();
        break;
      }

      case TracerCommandsParser::T__77: {
        enterOuterAlt(_localctx, 18);
        setState(79);
        help();
        break;
      }

      case TracerCommandsParser::T__78:
      case TracerCommandsParser::T__79: {
        enterOuterAlt(_localctx, 19);
        setState(80);
        quit();
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
    setState(83);
    match(TracerCommandsParser::T__1);
    setState(95);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__2: {
        setState(84);
        match(TracerCommandsParser::T__2);
        setState(88);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__6

        || _la == TracerCommandsParser::T__7) {
          setState(85);
          serverOption();
          setState(90);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__3: {
        setState(91);
        match(TracerCommandsParser::T__3);
        setState(92);
        value();
        break;
      }

      case TracerCommandsParser::T__4: {
        setState(93);
        match(TracerCommandsParser::T__4);
        break;
      }

      case TracerCommandsParser::T__5: {
        setState(94);
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
    setState(97);
    _la = _input->LA(1);
    if (!(_la == TracerCommandsParser::T__6

    || _la == TracerCommandsParser::T__7)) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(98);
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

tree::TerminalNode* TracerCommandsParser::AttachContext::UINT() {
  return getToken(TracerCommandsParser::UINT, 0);
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
    setState(100);
    match(TracerCommandsParser::T__8);
    setState(102);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::UINT) {
      setState(101);
      match(TracerCommandsParser::UINT);
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
    setState(104);
    match(TracerCommandsParser::T__9);
    setState(107);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::T__10) {
      setState(105);
      match(TracerCommandsParser::T__10);
      setState(106);
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
    setState(109);
    match(TracerCommandsParser::T__11);

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
    setState(111);
    match(TracerCommandsParser::T__12);
    setState(112);
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
    setState(114);
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
    setState(116);
    match(TracerCommandsParser::T__13);
    setState(118);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & -4476737733419469188) != 0) || ((((_la - 67) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 67)) & 196335) != 0)) {
      setState(117);
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

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(120);
    match(TracerCommandsParser::T__14);

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
  enterRule(_localctx, 22, TracerCommandsParser::RuleSymbols);
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
    setState(122);
    match(TracerCommandsParser::T__15);
    setState(126);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 919552) != 0)) {
      setState(123);
      symbolOption();
      setState(128);
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
  enterRule(_localctx, 24, TracerCommandsParser::RuleSymbolOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(133);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__10:
      case TracerCommandsParser::T__16:
      case TracerCommandsParser::T__17: {
        enterOuterAlt(_localctx, 1);
        setState(129);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 395264) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(130);
        value();
        break;
      }

      case TracerCommandsParser::T__18: {
        enterOuterAlt(_localctx, 2);
        setState(131);
        match(TracerCommandsParser::T__18);
        setState(132);
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
  enterRule(_localctx, 26, TracerCommandsParser::RuleProbe);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(176);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__19: {
        enterOuterAlt(_localctx, 1);
        setState(135);
        match(TracerCommandsParser::T__19);
        setState(173);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__20: {
            setState(136);
            match(TracerCommandsParser::T__20);
            setState(137);
            value();
            setState(139);
            _errHandler->sync(this);
            _la = _input->LA(1);
            do {
              setState(138);
              prepareOption();
              setState(141);
              _errHandler->sync(this);
              _la = _input->LA(1);
            } while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 4026662912) != 0));
            break;
          }

          case TracerCommandsParser::T__21: {
            setState(143);
            match(TracerCommandsParser::T__21);
            setState(144);
            value();
            setState(148);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 30870208512) != 0)) {
              setState(145);
              resolveOption();
              setState(150);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__22: {
            setState(151);
            match(TracerCommandsParser::T__22);
            setState(152);
            value();
            setState(153);
            match(TracerCommandsParser::T__23);
            setState(154);
            value();
            setState(158);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 271388377088) != 0)) {
              setState(155);
              deployOption();
              setState(160);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__24: {
            setState(161);
            match(TracerCommandsParser::T__24);
            setState(162);
            value();
            setState(163);
            match(TracerCommandsParser::T__23);
            setState(164);
            value();
            setState(168);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 30870208512) != 0)) {
              setState(165);
              resolveOption();
              setState(170);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__25: {
            setState(171);
            match(TracerCommandsParser::T__25);
            setState(172);
            match(TracerCommandsParser::UINT);
            break;
          }

        default:
          throw NoViableAltException(this);
        }
        break;
      }

      case TracerCommandsParser::T__26: {
        enterOuterAlt(_localctx, 2);
        setState(175);
        match(TracerCommandsParser::T__26);
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
  enterRule(_localctx, 28, TracerCommandsParser::RulePrepareOption);
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
    setState(178);
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 4026662912) != 0))) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(179);
    value();

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
  enterRule(_localctx, 30, TracerCommandsParser::RuleResolveOption);
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
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 30870208512) != 0))) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(182);
    value();

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
  enterRule(_localctx, 32, TracerCommandsParser::RuleDeployOption);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(189);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__16:
      case TracerCommandsParser::T__27:
      case TracerCommandsParser::T__28:
      case TracerCommandsParser::T__31:
      case TracerCommandsParser::T__32:
      case TracerCommandsParser::T__33: {
        enterOuterAlt(_localctx, 1);
        setState(184);
        resolveOption();
        break;
      }

      case TracerCommandsParser::T__34: {
        enterOuterAlt(_localctx, 2);
        setState(185);
        match(TracerCommandsParser::T__34);
        setState(186);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__35: {
        enterOuterAlt(_localctx, 3);
        setState(187);
        match(TracerCommandsParser::T__35);
        break;
      }

      case TracerCommandsParser::T__36: {
        enterOuterAlt(_localctx, 4);
        setState(188);
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
  enterRule(_localctx, 34, TracerCommandsParser::RuleInterceptor);
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
    setState(191);
    match(TracerCommandsParser::T__37);
    setState(211);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__38: {
        setState(192);
        match(TracerCommandsParser::T__38);
        setState(193);
        value();
        setState(194);
        match(TracerCommandsParser::T__39);
        setState(195);
        value();
        break;
      }

      case TracerCommandsParser::T__40: {
        setState(197);
        match(TracerCommandsParser::T__40);
        setState(198);
        value();
        break;
      }

      case TracerCommandsParser::T__41: {
        setState(199);
        match(TracerCommandsParser::T__41);
        setState(200);
        value();
        break;
      }

      case TracerCommandsParser::T__42: {
        setState(201);
        match(TracerCommandsParser::T__42);
        setState(202);
        value();
        setState(203);
        match(TracerCommandsParser::T__43);
        setState(204);
        value();
        setState(208);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 271388377088) != 0)) {
          setState(205);
          deployOption();
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
  enterRule(_localctx, 36, TracerCommandsParser::RuleEvents);
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
    match(TracerCommandsParser::T__44);
    setState(249);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::EOF:
      case TracerCommandsParser::T__39:
      case TracerCommandsParser::T__56:
      case TracerCommandsParser::T__57:
      case TracerCommandsParser::T__58:
      case TracerCommandsParser::T__59:
      case TracerCommandsParser::T__60:
      case TracerCommandsParser::T__63:
      case TracerCommandsParser::T__64:
      case TracerCommandsParser::T__65: {
        setState(217);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (((((_la - 40) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 40)) & 121503745) != 0)) {
          setState(214);
          streamOption();
          setState(219);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__45: {
        setState(220);
        match(TracerCommandsParser::T__45);
        setState(221);
        value();
        setState(224);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__18) {
          setState(222);
          match(TracerCommandsParser::T__18);
          setState(223);
          match(TracerCommandsParser::UINT);
        }
        break;
      }

      case TracerCommandsParser::T__41: {
        setState(226);
        match(TracerCommandsParser::T__41);
        setState(227);
        value();
        setState(228);
        match(TracerCommandsParser::T__46);
        setState(229);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__47: {
        setState(231);
        match(TracerCommandsParser::T__47);
        setState(232);
        value();
        break;
      }

      case TracerCommandsParser::T__48: {
        setState(233);
        match(TracerCommandsParser::T__48);
        setState(234);
        value();
        break;
      }

      case TracerCommandsParser::T__49: {
        setState(235);
        match(TracerCommandsParser::T__49);
        setState(236);
        value();
        break;
      }

      case TracerCommandsParser::T__50: {
        setState(237);
        match(TracerCommandsParser::T__50);
        setState(238);
        value();
        setState(239);
        match(TracerCommandsParser::T__39);
        setState(240);
        value();
        break;
      }

      case TracerCommandsParser::T__51: {
        setState(242);
        match(TracerCommandsParser::T__51);
        setState(243);
        value();
        setState(244);
        match(TracerCommandsParser::T__52);
        setState(245);
        match(TracerCommandsParser::T__53);
        setState(246);
        match(TracerCommandsParser::T__30);
        setState(247);
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
  enterRule(_localctx, 38, TracerCommandsParser::RuleCollect);
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
    match(TracerCommandsParser::T__54);
    setState(255);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (((((_la - 40) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 40)) & 121503745) != 0)) {
      setState(252);
      streamOption();
      setState(257);
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
  enterRule(_localctx, 40, TracerCommandsParser::RuleRecord);
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
    match(TracerCommandsParser::T__55);
    setState(259);
    value();
    setState(263);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while (((((_la - 40) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 40)) & 121503745) != 0)) {
      setState(260);
      streamOption();
      setState(265);
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
  enterRule(_localctx, 42, TracerCommandsParser::RuleStreamOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(273);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__56:
      case TracerCommandsParser::T__57:
      case TracerCommandsParser::T__58:
      case TracerCommandsParser::T__59: {
        enterOuterAlt(_localctx, 1);
        setState(266);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 2161727821137838080) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(267);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__60: {
        enterOuterAlt(_localctx, 2);
        setState(268);
        match(TracerCommandsParser::T__60);
        setState(269);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__61

        || _la == TracerCommandsParser::T__62)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        break;
      }

      case TracerCommandsParser::T__39:
      case TracerCommandsParser::T__63:
      case TracerCommandsParser::T__64: {
        enterOuterAlt(_localctx, 3);
        setState(270);
        _la = _input->LA(1);
        if (!(((((_la - 40) & ~ 0x3fULL) == 0) &&
          ((1ULL << (_la - 40)) & 50331649) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(271);
        value();
        break;
      }

      case TracerCommandsParser::T__65: {
        enterOuterAlt(_localctx, 4);
        setState(272);
        match(TracerCommandsParser::T__65);
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
  enterRule(_localctx, 44, TracerCommandsParser::RuleCpp);
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
    setState(275);
    match(TracerCommandsParser::T__66);
    setState(304);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__38: {
        setState(276);
        match(TracerCommandsParser::T__38);
        setState(277);
        value();
        break;
      }

      case TracerCommandsParser::T__67: {
        setState(278);
        match(TracerCommandsParser::T__67);
        break;
      }

      case TracerCommandsParser::T__68:
      case TracerCommandsParser::T__69: {
        setState(279);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__68

        || _la == TracerCommandsParser::T__69)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(280);
        value();
        setState(285);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__30

        || _la == TracerCommandsParser::T__70) {
          setState(281);
          _la = _input->LA(1);
          if (!(_la == TracerCommandsParser::T__30

          || _la == TracerCommandsParser::T__70)) {
          _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(282);
          value();
          setState(287);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__71: {
        setState(288);
        match(TracerCommandsParser::T__71);
        setState(302);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__72: {
            setState(289);
            match(TracerCommandsParser::T__72);
            setState(292);
            _errHandler->sync(this);
            _la = _input->LA(1);
            do {
              setState(292);
              _errHandler->sync(this);
              switch (_input->LA(1)) {
                case TracerCommandsParser::T__1:
                case TracerCommandsParser::T__2:
                case TracerCommandsParser::T__3:
                case TracerCommandsParser::T__4:
                case TracerCommandsParser::T__5:
                case TracerCommandsParser::T__8:
                case TracerCommandsParser::T__9:
                case TracerCommandsParser::T__11:
                case TracerCommandsParser::T__12:
                case TracerCommandsParser::T__13:
                case TracerCommandsParser::T__14:
                case TracerCommandsParser::T__15:
                case TracerCommandsParser::T__19:
                case TracerCommandsParser::T__20:
                case TracerCommandsParser::T__21:
                case TracerCommandsParser::T__22:
                case TracerCommandsParser::T__24:
                case TracerCommandsParser::T__25:
                case TracerCommandsParser::T__26:
                case TracerCommandsParser::T__37:
                case TracerCommandsParser::T__38:
                case TracerCommandsParser::T__40:
                case TracerCommandsParser::T__41:
                case TracerCommandsParser::T__42:
                case TracerCommandsParser::T__44:
                case TracerCommandsParser::T__45:
                case TracerCommandsParser::T__47:
                case TracerCommandsParser::T__48:
                case TracerCommandsParser::T__49:
                case TracerCommandsParser::T__50:
                case TracerCommandsParser::T__51:
                case TracerCommandsParser::T__53:
                case TracerCommandsParser::T__54:
                case TracerCommandsParser::T__55:
                case TracerCommandsParser::T__61:
                case TracerCommandsParser::T__62:
                case TracerCommandsParser::T__66:
                case TracerCommandsParser::T__67:
                case TracerCommandsParser::T__68:
                case TracerCommandsParser::T__69:
                case TracerCommandsParser::T__71:
                case TracerCommandsParser::T__72:
                case TracerCommandsParser::T__73:
                case TracerCommandsParser::T__75:
                case TracerCommandsParser::T__76:
                case TracerCommandsParser::T__77:
                case TracerCommandsParser::T__78:
                case TracerCommandsParser::T__79:
                case TracerCommandsParser::UINT:
                case TracerCommandsParser::STRING:
                case TracerCommandsParser::WORD: {
                  setState(290);
                  value();
                  break;
                }

                case TracerCommandsParser::SHELL: {
                  setState(291);
                  match(TracerCommandsParser::SHELL);
                  break;
                }

              default:
                throw NoViableAltException(this);
              }
              setState(294);
              _errHandler->sync(this);
              _la = _input->LA(1);
            } while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & -4476737733419469188) != 0) || ((((_la - 67) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 67)) & 261871) != 0));
            break;
          }

          case TracerCommandsParser::T__49: {
            setState(296);
            match(TracerCommandsParser::T__49);
            break;
          }

          case TracerCommandsParser::T__73: {
            setState(297);
            match(TracerCommandsParser::T__73);
            setState(298);
            value();
            setState(299);
            match(TracerCommandsParser::T__74);
            setState(300);
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
  enterRule(_localctx, 46, TracerCommandsParser::RuleShell);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(306);
    match(TracerCommandsParser::T__75);
    setState(307);
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
  enterRule(_localctx, 48, TracerCommandsParser::RuleComplete);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(309);
    match(TracerCommandsParser::T__76);
    setState(310);
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

TracerCommandsParser::ValueContext* TracerCommandsParser::HelpContext::value() {
  return getRuleContext<TracerCommandsParser::ValueContext>(0);
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
  enterRule(_localctx, 50, TracerCommandsParser::RuleHelp);
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
    setState(312);
    match(TracerCommandsParser::T__77);
    setState(314);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & -4476737733419469188) != 0) || ((((_la - 67) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 67)) & 196335) != 0)) {
      setState(313);
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
  enterRule(_localctx, 52, TracerCommandsParser::RuleQuit);
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
    setState(316);
    _la = _input->LA(1);
    if (!(_la == TracerCommandsParser::T__78

    || _la == TracerCommandsParser::T__79)) {
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
  enterRule(_localctx, 54, TracerCommandsParser::RuleValue);

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
      case TracerCommandsParser::STRING: {
        enterOuterAlt(_localctx, 1);
        setState(318);
        match(TracerCommandsParser::STRING);
        break;
      }

      case TracerCommandsParser::WORD: {
        enterOuterAlt(_localctx, 2);
        setState(319);
        match(TracerCommandsParser::WORD);
        break;
      }

      case TracerCommandsParser::UINT: {
        enterOuterAlt(_localctx, 3);
        setState(320);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__1:
      case TracerCommandsParser::T__2:
      case TracerCommandsParser::T__3:
      case TracerCommandsParser::T__4:
      case TracerCommandsParser::T__5:
      case TracerCommandsParser::T__8:
      case TracerCommandsParser::T__9:
      case TracerCommandsParser::T__11:
      case TracerCommandsParser::T__12:
      case TracerCommandsParser::T__13:
      case TracerCommandsParser::T__14:
      case TracerCommandsParser::T__15:
      case TracerCommandsParser::T__19:
      case TracerCommandsParser::T__20:
      case TracerCommandsParser::T__21:
      case TracerCommandsParser::T__22:
      case TracerCommandsParser::T__24:
      case TracerCommandsParser::T__25:
      case TracerCommandsParser::T__26:
      case TracerCommandsParser::T__37:
      case TracerCommandsParser::T__38:
      case TracerCommandsParser::T__40:
      case TracerCommandsParser::T__41:
      case TracerCommandsParser::T__42:
      case TracerCommandsParser::T__44:
      case TracerCommandsParser::T__45:
      case TracerCommandsParser::T__47:
      case TracerCommandsParser::T__48:
      case TracerCommandsParser::T__49:
      case TracerCommandsParser::T__50:
      case TracerCommandsParser::T__51:
      case TracerCommandsParser::T__53:
      case TracerCommandsParser::T__54:
      case TracerCommandsParser::T__55:
      case TracerCommandsParser::T__61:
      case TracerCommandsParser::T__62:
      case TracerCommandsParser::T__66:
      case TracerCommandsParser::T__67:
      case TracerCommandsParser::T__68:
      case TracerCommandsParser::T__69:
      case TracerCommandsParser::T__71:
      case TracerCommandsParser::T__72:
      case TracerCommandsParser::T__73:
      case TracerCommandsParser::T__75:
      case TracerCommandsParser::T__76:
      case TracerCommandsParser::T__77:
      case TracerCommandsParser::T__78:
      case TracerCommandsParser::T__79: {
        enterOuterAlt(_localctx, 4);
        setState(321);
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
  enterRule(_localctx, 56, TracerCommandsParser::RuleKeyword);
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
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & -4476737733419469188) != 0) || ((((_la - 67) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 67)) & 16111) != 0))) {
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

void TracerCommandsParser::initialize() {
#if ANTLR4_USE_THREAD_LOCAL_CACHE
  tracercommandsParserInitialize();
#else
  ::antlr4::internal::call_once(tracercommandsParserOnceFlag, tracercommandsParserInitialize);
#endif
}
