#!/usr/bin/env bash
set -euo pipefail

repl=$1
client=$2
server=$3
agent=$4
target=$5
methods=$6
run=$(mktemp -d)
echo "SQLite replay demo: $run"
cd "$run"
export XDG_CACHE_HOME="$run/capture-cache"
export TRACER_PREPARE_EXECUTABLE="$(dirname "$repl")/clang-tracer-prepare-injectors-native"
export CLANG_TRACER_ENDPOINT="unix://$run/tracer.sock"
cp "$methods"/{account.hpp,reading.hpp,state_interceptor.cpp} .
printf '[{"directory":"%s","file":"account.cpp","arguments":["clang++","-std=c++17","-c","account.cpp"]}]\n' "$run" >compile_commands.json

"$server" --agent "$agent" --listen "$CLANG_TRACER_ENDPOINT" --timeout 60 >server.log 2>&1 &
server_pid=$!
"$target" >target.log 2>&1 &
target_pid=$!
cleanup() {
  kill "$target_pid" "$server_pid" 2>/dev/null || true
  wait "$target_pid" "$server_pid" 2>/dev/null || true
}
trap cleanup EXIT
timeout 10 bash -c 'until test -S tracer.sock; do sleep 0.1; done'

selector='prepared_demo::Account::apply(const std::string &, std::vector<int>, prepared_demo::Adjustment, std::unique_ptr<int>, const std::shared_ptr<int> &, const std::weak_ptr<int> &, Choice)'
{
  printf '%%probe prepare "%s" --include account.hpp --include reading.hpp --output prepared --metadata-only\n' "$selector"
  printf '%s\n' '%shell "unlink account.hpp && unlink reading.hpp && unlink compile_commands.json"'
  printf '%%probe inject "%s" --callback state_interceptor.cpp --prepared-mode require\n' "$selector"
  printf '%%shell "\047%s\047 events %s --sqlite streamed.db --count 12 --timeout 8"\n' "$client" "$target_pid"
  printf '%%shell "\047%s\047 drain %s --sqlite drained.db --count 12 --timeout 8"\n' "$client" "$target_pid"
  printf '%s\n' '%quit'
} | "$repl" --attach "$target_pid" --timeout 60 --fail-fast | tee capture.log
grep -q '^installed probe=' capture.log
grep -q '^drained 12 events' capture.log
test ! -e account.hpp
test ! -e reading.hpp
test ! -e compile_commands.json
cleanup
trap - EXIT
unset CLANG_TRACER_ENDPOINT

mkdir offline
mv streamed.db drained.db offline/
cd offline
for recording in streamed drained; do
  export XDG_CACHE_HOME="$run/offline-$recording-cache"
  {
    if [[ "$recording" == streamed ]]; then
      cat <<'REPL'
tracer_repl::EventQuery query;
query.names = {"account.adjusted"};
query.count = 1;
auto matches = tracer.replay("streamed.db", query);
REPL
    else
      printf '%s\n' '%events open drained.db --as saved' 'auto matches = tracer.batch("saved");'
    fi
    cat <<'REPL'
if (!matches.size()) throw std::runtime_error("missing account.adjusted");
auto reading = matches.at(0).state_as<prepared_demo::Reading>();
if (reading.label != "deposit" || reading.parts.at(1) != 5 || reading.adjustment.delta != 4 || reading.adjustment.limit.value() != 30) throw std::runtime_error("incorrect replayed Reading");
std::cout << "SQLITE_READING=" << reading.label << ' ' << reading.parts.at(1) << ' ' << reading.adjustment.delta << ' ' << reading.adjustment.limit.value() << std::endl;
%quit
REPL
  } | "$repl" --fail-fast | tee "$recording.log"
  grep -qx 'SQLITE_READING=deposit 5 4 30' "$recording.log"
done
