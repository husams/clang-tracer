// Replay a SQLite recording and read the recorded result as an int.
tracer_repl::EventQuery query;
query.names = {"add(int, int)"};
query.phase = "POST";
auto recorded = tracer.replay("calls.db", query);
recorded.at(0).result_as<int>()
