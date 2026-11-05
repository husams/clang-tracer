
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
      "probe", "resolveOption", "deployOption", "interceptor", "events",
      "collect", "record", "streamOption", "cpp", "shell", "complete", "help",
      "quit", "value", "keyword"
    },
    std::vector<std::string>{
      "", "'%'", "'server'", "'start'", "'connect'", "'status'", "'stop'",
      "'--executable'", "'--agent'", "'attach'", "'processes'", "'--filter'",
      "'sessions'", "'use'", "'detach'", "'modules'", "'symbols'", "'--module'",
      "'--page'", "'--limit'", "'probe'", "'resolve'", "'inject'", "'--callback'",
      "'validate'", "'remove'", "'probes'", "'--include'", "'--compdb'",
      "'--compile-option'", "'--stack-depth'", "'--print-arguments'", "'--print-result'",
      "'interceptor'", "'load'", "'--as'", "'edit'", "'show'", "'upload'",
      "'--target'", "'events'", "'list'", "'--index'", "'calls'", "'stats'",
      "'clear'", "'open'", "'export'", "'--format'", "'json'", "'--output'",
      "'collect'", "'record'", "'--count'", "'--timeout'", "'--after'",
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
      "", "", "", "", "", "", "", "", "", "UINT", "STRING", "SHELL", "WORD",
      "WS"
    }
  );
  static const int32_t serializedATNSegment[] = {
    4,1,81,315,2,0,7,0,2,1,7,1,2,2,7,2,2,3,7,3,2,4,7,4,2,5,7,5,2,6,7,6,2,
    7,7,7,2,8,7,8,2,9,7,9,2,10,7,10,2,11,7,11,2,12,7,12,2,13,7,13,2,14,7,
    14,2,15,7,15,2,16,7,16,2,17,7,17,2,18,7,18,2,19,7,19,2,20,7,20,2,21,7,
    21,2,22,7,22,2,23,7,23,2,24,7,24,2,25,7,25,2,26,7,26,2,27,7,27,1,0,1,
    0,1,0,1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,3,1,80,8,1,1,2,1,2,1,2,5,2,85,8,2,10,2,12,2,88,9,2,1,
    2,1,2,1,2,1,2,3,2,94,8,2,1,3,1,3,1,3,1,4,1,4,3,4,101,8,4,1,5,1,5,1,5,
    3,5,106,8,5,1,6,1,6,1,7,1,7,1,7,1,8,1,8,1,9,1,9,3,9,117,8,9,1,10,1,10,
    1,11,1,11,5,11,123,8,11,10,11,12,11,126,9,11,1,12,1,12,1,12,1,12,3,12,
    132,8,12,1,13,1,13,1,13,1,13,5,13,138,8,13,10,13,12,13,141,9,13,1,13,
    1,13,1,13,1,13,1,13,5,13,148,8,13,10,13,12,13,151,9,13,1,13,1,13,1,13,
    1,13,1,13,5,13,158,8,13,10,13,12,13,161,9,13,1,13,1,13,3,13,165,8,13,
    1,13,3,13,168,8,13,1,14,1,14,1,14,1,15,1,15,1,15,1,15,1,15,3,15,178,8,
    15,1,16,1,16,1,16,1,16,1,16,1,16,1,16,1,16,1,16,1,16,1,16,1,16,1,16,1,
    16,1,16,5,16,195,8,16,10,16,12,16,198,9,16,3,16,200,8,16,1,17,1,17,5,
    17,204,8,17,10,17,12,17,207,9,17,1,17,1,17,1,17,1,17,3,17,213,8,17,1,
    17,1,17,1,17,1,17,1,17,1,17,1,17,1,17,1,17,1,17,1,17,1,17,1,17,1,17,1,
    17,1,17,1,17,1,17,1,17,1,17,1,17,1,17,1,17,3,17,238,8,17,1,18,1,18,5,
    18,242,8,18,10,18,12,18,245,9,18,1,19,1,19,1,19,5,19,250,8,19,10,19,12,
    19,253,9,19,1,20,1,20,1,20,1,20,1,20,1,20,1,20,3,20,262,8,20,1,21,1,21,
    1,21,1,21,1,21,1,21,1,21,1,21,5,21,272,8,21,10,21,12,21,275,9,21,1,21,
    1,21,1,21,1,21,4,21,281,8,21,11,21,12,21,282,1,21,1,21,1,21,1,21,1,21,
    1,21,3,21,291,8,21,3,21,293,8,21,1,22,1,22,1,22,1,23,1,23,1,23,1,24,1,
    24,3,24,303,8,24,1,25,1,25,1,26,1,26,1,26,1,26,3,26,311,8,26,1,27,1,27,
    1,27,0,0,28,0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,34,36,38,40,
    42,44,46,48,50,52,54,0,10,1,0,7,8,2,0,11,11,17,18,2,0,17,17,27,29,1,0,
    53,56,1,0,58,59,2,0,35,35,60,61,1,0,65,66,2,0,50,50,67,67,1,0,75,76,15,
    0,2,6,9,10,12,16,20,22,24,26,33,34,36,38,40,41,43,47,49,49,51,52,58,59,
    63,66,68,70,72,76,354,0,56,1,0,0,0,2,79,1,0,0,0,4,81,1,0,0,0,6,95,1,0,
    0,0,8,98,1,0,0,0,10,102,1,0,0,0,12,107,1,0,0,0,14,109,1,0,0,0,16,112,
    1,0,0,0,18,114,1,0,0,0,20,118,1,0,0,0,22,120,1,0,0,0,24,131,1,0,0,0,26,
    167,1,0,0,0,28,169,1,0,0,0,30,177,1,0,0,0,32,179,1,0,0,0,34,201,1,0,0,
    0,36,239,1,0,0,0,38,246,1,0,0,0,40,261,1,0,0,0,42,263,1,0,0,0,44,294,
    1,0,0,0,46,297,1,0,0,0,48,300,1,0,0,0,50,304,1,0,0,0,52,310,1,0,0,0,54,
    312,1,0,0,0,56,57,5,1,0,0,57,58,3,2,1,0,58,59,5,0,0,1,59,1,1,0,0,0,60,
    80,3,4,2,0,61,80,3,8,4,0,62,80,3,10,5,0,63,80,3,12,6,0,64,80,3,14,7,0,
    65,80,3,16,8,0,66,80,3,18,9,0,67,80,3,20,10,0,68,80,3,22,11,0,69,80,3,
    26,13,0,70,80,3,32,16,0,71,80,3,34,17,0,72,80,3,36,18,0,73,80,3,38,19,
    0,74,80,3,42,21,0,75,80,3,44,22,0,76,80,3,46,23,0,77,80,3,48,24,0,78,
    80,3,50,25,0,79,60,1,0,0,0,79,61,1,0,0,0,79,62,1,0,0,0,79,63,1,0,0,0,
    79,64,1,0,0,0,79,65,1,0,0,0,79,66,1,0,0,0,79,67,1,0,0,0,79,68,1,0,0,0,
    79,69,1,0,0,0,79,70,1,0,0,0,79,71,1,0,0,0,79,72,1,0,0,0,79,73,1,0,0,0,
    79,74,1,0,0,0,79,75,1,0,0,0,79,76,1,0,0,0,79,77,1,0,0,0,79,78,1,0,0,0,
    80,3,1,0,0,0,81,93,5,2,0,0,82,86,5,3,0,0,83,85,3,6,3,0,84,83,1,0,0,0,
    85,88,1,0,0,0,86,84,1,0,0,0,86,87,1,0,0,0,87,94,1,0,0,0,88,86,1,0,0,0,
    89,90,5,4,0,0,90,94,3,52,26,0,91,94,5,5,0,0,92,94,5,6,0,0,93,82,1,0,0,
    0,93,89,1,0,0,0,93,91,1,0,0,0,93,92,1,0,0,0,94,5,1,0,0,0,95,96,7,0,0,
    0,96,97,3,52,26,0,97,7,1,0,0,0,98,100,5,9,0,0,99,101,5,77,0,0,100,99,
    1,0,0,0,100,101,1,0,0,0,101,9,1,0,0,0,102,105,5,10,0,0,103,104,5,11,0,
    0,104,106,3,52,26,0,105,103,1,0,0,0,105,106,1,0,0,0,106,11,1,0,0,0,107,
    108,5,12,0,0,108,13,1,0,0,0,109,110,5,13,0,0,110,111,3,52,26,0,111,15,
    1,0,0,0,112,113,5,5,0,0,113,17,1,0,0,0,114,116,5,14,0,0,115,117,3,52,
    26,0,116,115,1,0,0,0,116,117,1,0,0,0,117,19,1,0,0,0,118,119,5,15,0,0,
    119,21,1,0,0,0,120,124,5,16,0,0,121,123,3,24,12,0,122,121,1,0,0,0,123,
    126,1,0,0,0,124,122,1,0,0,0,124,125,1,0,0,0,125,23,1,0,0,0,126,124,1,
    0,0,0,127,128,7,1,0,0,128,132,3,52,26,0,129,130,5,19,0,0,130,132,5,77,
    0,0,131,127,1,0,0,0,131,129,1,0,0,0,132,25,1,0,0,0,133,164,5,20,0,0,134,
    135,5,21,0,0,135,139,3,52,26,0,136,138,3,28,14,0,137,136,1,0,0,0,138,
    141,1,0,0,0,139,137,1,0,0,0,139,140,1,0,0,0,140,165,1,0,0,0,141,139,1,
    0,0,0,142,143,5,22,0,0,143,144,3,52,26,0,144,145,5,23,0,0,145,149,3,52,
    26,0,146,148,3,30,15,0,147,146,1,0,0,0,148,151,1,0,0,0,149,147,1,0,0,
    0,149,150,1,0,0,0,150,165,1,0,0,0,151,149,1,0,0,0,152,153,5,24,0,0,153,
    154,3,52,26,0,154,155,5,23,0,0,155,159,3,52,26,0,156,158,3,28,14,0,157,
    156,1,0,0,0,158,161,1,0,0,0,159,157,1,0,0,0,159,160,1,0,0,0,160,165,1,
    0,0,0,161,159,1,0,0,0,162,163,5,25,0,0,163,165,5,77,0,0,164,134,1,0,0,
    0,164,142,1,0,0,0,164,152,1,0,0,0,164,162,1,0,0,0,165,168,1,0,0,0,166,
    168,5,26,0,0,167,133,1,0,0,0,167,166,1,0,0,0,168,27,1,0,0,0,169,170,7,
    2,0,0,170,171,3,52,26,0,171,29,1,0,0,0,172,178,3,28,14,0,173,174,5,30,
    0,0,174,178,5,77,0,0,175,178,5,31,0,0,176,178,5,32,0,0,177,172,1,0,0,
    0,177,173,1,0,0,0,177,175,1,0,0,0,177,176,1,0,0,0,178,31,1,0,0,0,179,
    199,5,33,0,0,180,181,5,34,0,0,181,182,3,52,26,0,182,183,5,35,0,0,183,
    184,3,52,26,0,184,200,1,0,0,0,185,186,5,36,0,0,186,200,3,52,26,0,187,
    188,5,37,0,0,188,200,3,52,26,0,189,190,5,38,0,0,190,191,3,52,26,0,191,
    192,5,39,0,0,192,196,3,52,26,0,193,195,3,30,15,0,194,193,1,0,0,0,195,
    198,1,0,0,0,196,194,1,0,0,0,196,197,1,0,0,0,197,200,1,0,0,0,198,196,1,
    0,0,0,199,180,1,0,0,0,199,185,1,0,0,0,199,187,1,0,0,0,199,189,1,0,0,0,
    200,33,1,0,0,0,201,237,5,40,0,0,202,204,3,40,20,0,203,202,1,0,0,0,204,
    207,1,0,0,0,205,203,1,0,0,0,205,206,1,0,0,0,206,238,1,0,0,0,207,205,1,
    0,0,0,208,209,5,41,0,0,209,212,3,52,26,0,210,211,5,19,0,0,211,213,5,77,
    0,0,212,210,1,0,0,0,212,213,1,0,0,0,213,238,1,0,0,0,214,215,5,37,0,0,
    215,216,3,52,26,0,216,217,5,42,0,0,217,218,5,77,0,0,218,238,1,0,0,0,219,
    220,5,43,0,0,220,238,3,52,26,0,221,222,5,44,0,0,222,238,3,52,26,0,223,
    224,5,45,0,0,224,238,3,52,26,0,225,226,5,46,0,0,226,227,3,52,26,0,227,
    228,5,35,0,0,228,229,3,52,26,0,229,238,1,0,0,0,230,231,5,47,0,0,231,232,
    3,52,26,0,232,233,5,48,0,0,233,234,5,49,0,0,234,235,5,50,0,0,235,236,
    3,52,26,0,236,238,1,0,0,0,237,205,1,0,0,0,237,208,1,0,0,0,237,214,1,0,
    0,0,237,219,1,0,0,0,237,221,1,0,0,0,237,223,1,0,0,0,237,225,1,0,0,0,237,
    230,1,0,0,0,238,35,1,0,0,0,239,243,5,51,0,0,240,242,3,40,20,0,241,240,
    1,0,0,0,242,245,1,0,0,0,243,241,1,0,0,0,243,244,1,0,0,0,244,37,1,0,0,
    0,245,243,1,0,0,0,246,247,5,52,0,0,247,251,3,52,26,0,248,250,3,40,20,
    0,249,248,1,0,0,0,250,253,1,0,0,0,251,249,1,0,0,0,251,252,1,0,0,0,252,
    39,1,0,0,0,253,251,1,0,0,0,254,255,7,3,0,0,255,262,5,77,0,0,256,257,5,
    57,0,0,257,262,7,4,0,0,258,259,7,5,0,0,259,262,3,52,26,0,260,262,5,62,
    0,0,261,254,1,0,0,0,261,256,1,0,0,0,261,258,1,0,0,0,261,260,1,0,0,0,262,
    41,1,0,0,0,263,292,5,63,0,0,264,265,5,34,0,0,265,293,3,52,26,0,266,293,
    5,64,0,0,267,268,7,6,0,0,268,273,3,52,26,0,269,270,7,7,0,0,270,272,3,
    52,26,0,271,269,1,0,0,0,272,275,1,0,0,0,273,271,1,0,0,0,273,274,1,0,0,
    0,274,293,1,0,0,0,275,273,1,0,0,0,276,290,5,68,0,0,277,280,5,69,0,0,278,
    281,3,52,26,0,279,281,5,79,0,0,280,278,1,0,0,0,280,279,1,0,0,0,281,282,
    1,0,0,0,282,280,1,0,0,0,282,283,1,0,0,0,283,291,1,0,0,0,284,291,5,45,
    0,0,285,286,5,70,0,0,286,287,3,52,26,0,287,288,5,71,0,0,288,289,3,52,
    26,0,289,291,1,0,0,0,290,277,1,0,0,0,290,284,1,0,0,0,290,285,1,0,0,0,
    290,291,1,0,0,0,291,293,1,0,0,0,292,264,1,0,0,0,292,266,1,0,0,0,292,267,
    1,0,0,0,292,276,1,0,0,0,293,43,1,0,0,0,294,295,5,72,0,0,295,296,3,52,
    26,0,296,45,1,0,0,0,297,298,5,73,0,0,298,299,3,52,26,0,299,47,1,0,0,0,
    300,302,5,74,0,0,301,303,3,52,26,0,302,301,1,0,0,0,302,303,1,0,0,0,303,
    49,1,0,0,0,304,305,7,8,0,0,305,51,1,0,0,0,306,311,5,78,0,0,307,311,5,
    80,0,0,308,311,5,77,0,0,309,311,3,54,27,0,310,306,1,0,0,0,310,307,1,0,
    0,0,310,308,1,0,0,0,310,309,1,0,0,0,311,53,1,0,0,0,312,313,7,9,0,0,313,
    55,1,0,0,0,29,79,86,93,100,105,116,124,131,139,149,159,164,167,177,196,
    199,205,212,237,243,251,261,273,280,282,290,292,302,310
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
    setState(56);
    match(TracerCommandsParser::T__0);
    setState(57);
    command();
    setState(58);
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
    setState(79);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__1: {
        enterOuterAlt(_localctx, 1);
        setState(60);
        server();
        break;
      }

      case TracerCommandsParser::T__8: {
        enterOuterAlt(_localctx, 2);
        setState(61);
        attach();
        break;
      }

      case TracerCommandsParser::T__9: {
        enterOuterAlt(_localctx, 3);
        setState(62);
        processes();
        break;
      }

      case TracerCommandsParser::T__11: {
        enterOuterAlt(_localctx, 4);
        setState(63);
        sessions();
        break;
      }

      case TracerCommandsParser::T__12: {
        enterOuterAlt(_localctx, 5);
        setState(64);
        use();
        break;
      }

      case TracerCommandsParser::T__4: {
        enterOuterAlt(_localctx, 6);
        setState(65);
        status();
        break;
      }

      case TracerCommandsParser::T__13: {
        enterOuterAlt(_localctx, 7);
        setState(66);
        detach();
        break;
      }

      case TracerCommandsParser::T__14: {
        enterOuterAlt(_localctx, 8);
        setState(67);
        modules();
        break;
      }

      case TracerCommandsParser::T__15: {
        enterOuterAlt(_localctx, 9);
        setState(68);
        symbols();
        break;
      }

      case TracerCommandsParser::T__19:
      case TracerCommandsParser::T__25: {
        enterOuterAlt(_localctx, 10);
        setState(69);
        probe();
        break;
      }

      case TracerCommandsParser::T__32: {
        enterOuterAlt(_localctx, 11);
        setState(70);
        interceptor();
        break;
      }

      case TracerCommandsParser::T__39: {
        enterOuterAlt(_localctx, 12);
        setState(71);
        events();
        break;
      }

      case TracerCommandsParser::T__50: {
        enterOuterAlt(_localctx, 13);
        setState(72);
        collect();
        break;
      }

      case TracerCommandsParser::T__51: {
        enterOuterAlt(_localctx, 14);
        setState(73);
        record();
        break;
      }

      case TracerCommandsParser::T__62: {
        enterOuterAlt(_localctx, 15);
        setState(74);
        cpp();
        break;
      }

      case TracerCommandsParser::T__71: {
        enterOuterAlt(_localctx, 16);
        setState(75);
        shell();
        break;
      }

      case TracerCommandsParser::T__72: {
        enterOuterAlt(_localctx, 17);
        setState(76);
        complete();
        break;
      }

      case TracerCommandsParser::T__73: {
        enterOuterAlt(_localctx, 18);
        setState(77);
        help();
        break;
      }

      case TracerCommandsParser::T__74:
      case TracerCommandsParser::T__75: {
        enterOuterAlt(_localctx, 19);
        setState(78);
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
    setState(81);
    match(TracerCommandsParser::T__1);
    setState(93);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__2: {
        setState(82);
        match(TracerCommandsParser::T__2);
        setState(86);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__6

        || _la == TracerCommandsParser::T__7) {
          setState(83);
          serverOption();
          setState(88);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__3: {
        setState(89);
        match(TracerCommandsParser::T__3);
        setState(90);
        value();
        break;
      }

      case TracerCommandsParser::T__4: {
        setState(91);
        match(TracerCommandsParser::T__4);
        break;
      }

      case TracerCommandsParser::T__5: {
        setState(92);
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
    setState(95);
    _la = _input->LA(1);
    if (!(_la == TracerCommandsParser::T__6

    || _la == TracerCommandsParser::T__7)) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(96);
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
    setState(98);
    match(TracerCommandsParser::T__8);
    setState(100);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::UINT) {
      setState(99);
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
    setState(102);
    match(TracerCommandsParser::T__9);
    setState(105);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if (_la == TracerCommandsParser::T__10) {
      setState(103);
      match(TracerCommandsParser::T__10);
      setState(104);
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
    setState(107);
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
    setState(109);
    match(TracerCommandsParser::T__12);
    setState(110);
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
    setState(112);
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
    setState(114);
    match(TracerCommandsParser::T__13);
    setState(116);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & -8351086074655541636) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 64)) & 98167) != 0)) {
      setState(115);
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
    setState(118);
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
    setState(120);
    match(TracerCommandsParser::T__15);
    setState(124);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 919552) != 0)) {
      setState(121);
      symbolOption();
      setState(126);
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
    setState(131);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__10:
      case TracerCommandsParser::T__16:
      case TracerCommandsParser::T__17: {
        enterOuterAlt(_localctx, 1);
        setState(127);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 395264) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(128);
        value();
        break;
      }

      case TracerCommandsParser::T__18: {
        enterOuterAlt(_localctx, 2);
        setState(129);
        match(TracerCommandsParser::T__18);
        setState(130);
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
    setState(167);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__19: {
        enterOuterAlt(_localctx, 1);
        setState(133);
        match(TracerCommandsParser::T__19);
        setState(164);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__20: {
            setState(134);
            match(TracerCommandsParser::T__20);
            setState(135);
            value();
            setState(139);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 939655168) != 0)) {
              setState(136);
              resolveOption();
              setState(141);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__21: {
            setState(142);
            match(TracerCommandsParser::T__21);
            setState(143);
            value();
            setState(144);
            match(TracerCommandsParser::T__22);
            setState(145);
            value();
            setState(149);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 8455847936) != 0)) {
              setState(146);
              deployOption();
              setState(151);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__23: {
            setState(152);
            match(TracerCommandsParser::T__23);
            setState(153);
            value();
            setState(154);
            match(TracerCommandsParser::T__22);
            setState(155);
            value();
            setState(159);
            _errHandler->sync(this);
            _la = _input->LA(1);
            while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & 939655168) != 0)) {
              setState(156);
              resolveOption();
              setState(161);
              _errHandler->sync(this);
              _la = _input->LA(1);
            }
            break;
          }

          case TracerCommandsParser::T__24: {
            setState(162);
            match(TracerCommandsParser::T__24);
            setState(163);
            match(TracerCommandsParser::UINT);
            break;
          }

        default:
          throw NoViableAltException(this);
        }
        break;
      }

      case TracerCommandsParser::T__25: {
        enterOuterAlt(_localctx, 2);
        setState(166);
        match(TracerCommandsParser::T__25);
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
  enterRule(_localctx, 28, TracerCommandsParser::RuleResolveOption);
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
    setState(169);
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 939655168) != 0))) {
    _errHandler->recoverInline(this);
    }
    else {
      _errHandler->reportMatch(this);
      consume();
    }
    setState(170);
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
  enterRule(_localctx, 30, TracerCommandsParser::RuleDeployOption);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(177);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__16:
      case TracerCommandsParser::T__26:
      case TracerCommandsParser::T__27:
      case TracerCommandsParser::T__28: {
        enterOuterAlt(_localctx, 1);
        setState(172);
        resolveOption();
        break;
      }

      case TracerCommandsParser::T__29: {
        enterOuterAlt(_localctx, 2);
        setState(173);
        match(TracerCommandsParser::T__29);
        setState(174);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__30: {
        enterOuterAlt(_localctx, 3);
        setState(175);
        match(TracerCommandsParser::T__30);
        break;
      }

      case TracerCommandsParser::T__31: {
        enterOuterAlt(_localctx, 4);
        setState(176);
        match(TracerCommandsParser::T__31);
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
  enterRule(_localctx, 32, TracerCommandsParser::RuleInterceptor);
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
    match(TracerCommandsParser::T__32);
    setState(199);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__33: {
        setState(180);
        match(TracerCommandsParser::T__33);
        setState(181);
        value();
        setState(182);
        match(TracerCommandsParser::T__34);
        setState(183);
        value();
        break;
      }

      case TracerCommandsParser::T__35: {
        setState(185);
        match(TracerCommandsParser::T__35);
        setState(186);
        value();
        break;
      }

      case TracerCommandsParser::T__36: {
        setState(187);
        match(TracerCommandsParser::T__36);
        setState(188);
        value();
        break;
      }

      case TracerCommandsParser::T__37: {
        setState(189);
        match(TracerCommandsParser::T__37);
        setState(190);
        value();
        setState(191);
        match(TracerCommandsParser::T__38);
        setState(192);
        value();
        setState(196);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 8455847936) != 0)) {
          setState(193);
          deployOption();
          setState(198);
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
  enterRule(_localctx, 34, TracerCommandsParser::RuleEvents);
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
    setState(201);
    match(TracerCommandsParser::T__39);
    setState(237);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::EOF:
      case TracerCommandsParser::T__34:
      case TracerCommandsParser::T__52:
      case TracerCommandsParser::T__53:
      case TracerCommandsParser::T__54:
      case TracerCommandsParser::T__55:
      case TracerCommandsParser::T__56:
      case TracerCommandsParser::T__59:
      case TracerCommandsParser::T__60:
      case TracerCommandsParser::T__61: {
        setState(205);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while ((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 8349673743504637952) != 0)) {
          setState(202);
          streamOption();
          setState(207);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__40: {
        setState(208);
        match(TracerCommandsParser::T__40);
        setState(209);
        value();
        setState(212);
        _errHandler->sync(this);

        _la = _input->LA(1);
        if (_la == TracerCommandsParser::T__18) {
          setState(210);
          match(TracerCommandsParser::T__18);
          setState(211);
          match(TracerCommandsParser::UINT);
        }
        break;
      }

      case TracerCommandsParser::T__36: {
        setState(214);
        match(TracerCommandsParser::T__36);
        setState(215);
        value();
        setState(216);
        match(TracerCommandsParser::T__41);
        setState(217);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__42: {
        setState(219);
        match(TracerCommandsParser::T__42);
        setState(220);
        value();
        break;
      }

      case TracerCommandsParser::T__43: {
        setState(221);
        match(TracerCommandsParser::T__43);
        setState(222);
        value();
        break;
      }

      case TracerCommandsParser::T__44: {
        setState(223);
        match(TracerCommandsParser::T__44);
        setState(224);
        value();
        break;
      }

      case TracerCommandsParser::T__45: {
        setState(225);
        match(TracerCommandsParser::T__45);
        setState(226);
        value();
        setState(227);
        match(TracerCommandsParser::T__34);
        setState(228);
        value();
        break;
      }

      case TracerCommandsParser::T__46: {
        setState(230);
        match(TracerCommandsParser::T__46);
        setState(231);
        value();
        setState(232);
        match(TracerCommandsParser::T__47);
        setState(233);
        match(TracerCommandsParser::T__48);
        setState(234);
        match(TracerCommandsParser::T__49);
        setState(235);
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
  enterRule(_localctx, 36, TracerCommandsParser::RuleCollect);
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
    setState(239);
    match(TracerCommandsParser::T__50);
    setState(243);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 8349673743504637952) != 0)) {
      setState(240);
      streamOption();
      setState(245);
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
  enterRule(_localctx, 38, TracerCommandsParser::RuleRecord);
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
    setState(246);
    match(TracerCommandsParser::T__51);
    setState(247);
    value();
    setState(251);
    _errHandler->sync(this);
    _la = _input->LA(1);
    while ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & 8349673743504637952) != 0)) {
      setState(248);
      streamOption();
      setState(253);
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
  enterRule(_localctx, 40, TracerCommandsParser::RuleStreamOption);
  size_t _la = 0;

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    setState(261);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__52:
      case TracerCommandsParser::T__53:
      case TracerCommandsParser::T__54:
      case TracerCommandsParser::T__55: {
        enterOuterAlt(_localctx, 1);
        setState(254);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 135107988821114880) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(255);
        match(TracerCommandsParser::UINT);
        break;
      }

      case TracerCommandsParser::T__56: {
        enterOuterAlt(_localctx, 2);
        setState(256);
        match(TracerCommandsParser::T__56);
        setState(257);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__57

        || _la == TracerCommandsParser::T__58)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        break;
      }

      case TracerCommandsParser::T__34:
      case TracerCommandsParser::T__59:
      case TracerCommandsParser::T__60: {
        enterOuterAlt(_localctx, 3);
        setState(258);
        _la = _input->LA(1);
        if (!((((_la & ~ 0x3fULL) == 0) &&
          ((1ULL << _la) & 3458764548180279296) != 0))) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(259);
        value();
        break;
      }

      case TracerCommandsParser::T__61: {
        enterOuterAlt(_localctx, 4);
        setState(260);
        match(TracerCommandsParser::T__61);
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
  enterRule(_localctx, 42, TracerCommandsParser::RuleCpp);
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
    setState(263);
    match(TracerCommandsParser::T__62);
    setState(292);
    _errHandler->sync(this);
    switch (_input->LA(1)) {
      case TracerCommandsParser::T__33: {
        setState(264);
        match(TracerCommandsParser::T__33);
        setState(265);
        value();
        break;
      }

      case TracerCommandsParser::T__63: {
        setState(266);
        match(TracerCommandsParser::T__63);
        break;
      }

      case TracerCommandsParser::T__64:
      case TracerCommandsParser::T__65: {
        setState(267);
        _la = _input->LA(1);
        if (!(_la == TracerCommandsParser::T__64

        || _la == TracerCommandsParser::T__65)) {
        _errHandler->recoverInline(this);
        }
        else {
          _errHandler->reportMatch(this);
          consume();
        }
        setState(268);
        value();
        setState(273);
        _errHandler->sync(this);
        _la = _input->LA(1);
        while (_la == TracerCommandsParser::T__49

        || _la == TracerCommandsParser::T__66) {
          setState(269);
          _la = _input->LA(1);
          if (!(_la == TracerCommandsParser::T__49

          || _la == TracerCommandsParser::T__66)) {
          _errHandler->recoverInline(this);
          }
          else {
            _errHandler->reportMatch(this);
            consume();
          }
          setState(270);
          value();
          setState(275);
          _errHandler->sync(this);
          _la = _input->LA(1);
        }
        break;
      }

      case TracerCommandsParser::T__67: {
        setState(276);
        match(TracerCommandsParser::T__67);
        setState(290);
        _errHandler->sync(this);
        switch (_input->LA(1)) {
          case TracerCommandsParser::T__68: {
            setState(277);
            match(TracerCommandsParser::T__68);
            setState(280);
            _errHandler->sync(this);
            _la = _input->LA(1);
            do {
              setState(280);
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
                case TracerCommandsParser::T__23:
                case TracerCommandsParser::T__24:
                case TracerCommandsParser::T__25:
                case TracerCommandsParser::T__32:
                case TracerCommandsParser::T__33:
                case TracerCommandsParser::T__35:
                case TracerCommandsParser::T__36:
                case TracerCommandsParser::T__37:
                case TracerCommandsParser::T__39:
                case TracerCommandsParser::T__40:
                case TracerCommandsParser::T__42:
                case TracerCommandsParser::T__43:
                case TracerCommandsParser::T__44:
                case TracerCommandsParser::T__45:
                case TracerCommandsParser::T__46:
                case TracerCommandsParser::T__48:
                case TracerCommandsParser::T__50:
                case TracerCommandsParser::T__51:
                case TracerCommandsParser::T__57:
                case TracerCommandsParser::T__58:
                case TracerCommandsParser::T__62:
                case TracerCommandsParser::T__63:
                case TracerCommandsParser::T__64:
                case TracerCommandsParser::T__65:
                case TracerCommandsParser::T__67:
                case TracerCommandsParser::T__68:
                case TracerCommandsParser::T__69:
                case TracerCommandsParser::T__71:
                case TracerCommandsParser::T__72:
                case TracerCommandsParser::T__73:
                case TracerCommandsParser::T__74:
                case TracerCommandsParser::T__75:
                case TracerCommandsParser::UINT:
                case TracerCommandsParser::STRING:
                case TracerCommandsParser::WORD: {
                  setState(278);
                  value();
                  break;
                }

                case TracerCommandsParser::SHELL: {
                  setState(279);
                  match(TracerCommandsParser::SHELL);
                  break;
                }

              default:
                throw NoViableAltException(this);
              }
              setState(282);
              _errHandler->sync(this);
              _la = _input->LA(1);
            } while ((((_la & ~ 0x3fULL) == 0) &&
              ((1ULL << _la) & -8351086074655541636) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
              ((1ULL << (_la - 64)) & 130935) != 0));
            break;
          }

          case TracerCommandsParser::T__44: {
            setState(284);
            match(TracerCommandsParser::T__44);
            break;
          }

          case TracerCommandsParser::T__69: {
            setState(285);
            match(TracerCommandsParser::T__69);
            setState(286);
            value();
            setState(287);
            match(TracerCommandsParser::T__70);
            setState(288);
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
  enterRule(_localctx, 44, TracerCommandsParser::RuleShell);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(294);
    match(TracerCommandsParser::T__71);
    setState(295);
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
  enterRule(_localctx, 46, TracerCommandsParser::RuleComplete);

#if __cplusplus > 201703L
  auto onExit = finally([=, this] {
#else
  auto onExit = finally([=] {
#endif
    exitRule();
  });
  try {
    enterOuterAlt(_localctx, 1);
    setState(297);
    match(TracerCommandsParser::T__72);
    setState(298);
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
  enterRule(_localctx, 48, TracerCommandsParser::RuleHelp);
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
    setState(300);
    match(TracerCommandsParser::T__73);
    setState(302);
    _errHandler->sync(this);

    _la = _input->LA(1);
    if ((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & -8351086074655541636) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 64)) & 98167) != 0)) {
      setState(301);
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
  enterRule(_localctx, 50, TracerCommandsParser::RuleQuit);
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
    setState(304);
    _la = _input->LA(1);
    if (!(_la == TracerCommandsParser::T__74

    || _la == TracerCommandsParser::T__75)) {
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
  enterRule(_localctx, 52, TracerCommandsParser::RuleValue);

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
      case TracerCommandsParser::STRING: {
        enterOuterAlt(_localctx, 1);
        setState(306);
        match(TracerCommandsParser::STRING);
        break;
      }

      case TracerCommandsParser::WORD: {
        enterOuterAlt(_localctx, 2);
        setState(307);
        match(TracerCommandsParser::WORD);
        break;
      }

      case TracerCommandsParser::UINT: {
        enterOuterAlt(_localctx, 3);
        setState(308);
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
      case TracerCommandsParser::T__23:
      case TracerCommandsParser::T__24:
      case TracerCommandsParser::T__25:
      case TracerCommandsParser::T__32:
      case TracerCommandsParser::T__33:
      case TracerCommandsParser::T__35:
      case TracerCommandsParser::T__36:
      case TracerCommandsParser::T__37:
      case TracerCommandsParser::T__39:
      case TracerCommandsParser::T__40:
      case TracerCommandsParser::T__42:
      case TracerCommandsParser::T__43:
      case TracerCommandsParser::T__44:
      case TracerCommandsParser::T__45:
      case TracerCommandsParser::T__46:
      case TracerCommandsParser::T__48:
      case TracerCommandsParser::T__50:
      case TracerCommandsParser::T__51:
      case TracerCommandsParser::T__57:
      case TracerCommandsParser::T__58:
      case TracerCommandsParser::T__62:
      case TracerCommandsParser::T__63:
      case TracerCommandsParser::T__64:
      case TracerCommandsParser::T__65:
      case TracerCommandsParser::T__67:
      case TracerCommandsParser::T__68:
      case TracerCommandsParser::T__69:
      case TracerCommandsParser::T__71:
      case TracerCommandsParser::T__72:
      case TracerCommandsParser::T__73:
      case TracerCommandsParser::T__74:
      case TracerCommandsParser::T__75: {
        enterOuterAlt(_localctx, 4);
        setState(309);
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
  enterRule(_localctx, 54, TracerCommandsParser::RuleKeyword);
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
    _la = _input->LA(1);
    if (!((((_la & ~ 0x3fULL) == 0) &&
      ((1ULL << _la) & -8351086074655541636) != 0) || ((((_la - 64) & ~ 0x3fULL) == 0) &&
      ((1ULL << (_la - 64)) & 8055) != 0))) {
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
