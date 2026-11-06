#!/usr/bin/env bash
set -euo pipefail

script=$(readlink -f -- "${BASH_SOURCE[0]}")
root=$(cd -- "$(dirname -- "$script")/../.." && pwd)
build=$(realpath -m -- "${CLANG_TRACER_PREPARED_BUILD:-$root/build}")
repl=${CLANG_TRACER_REPL:-$build/clang-tracer-repl}
server=${CLANG_TRACER_SERVER:-$build/clang-tracer-server}
agent=${CLANG_TRACER_AGENT:-$build/libclang_tracer_agent.so}
example=prepared-injectors-target
if [[ ${1:-} == --methods ]]; then
  example=prepared-methods-target
  shift
fi
target=${CLANG_TRACER_PREPARED_TARGET:-$build/examples/prepared-injectors/$example}
compdb=${CLANG_TRACER_PREPARED_COMPDB:-$build/compile_commands.json}

if [[ ${1:-} == --help ]]; then
  printf '%s\n' 'Use --methods for the typed method demo and --smoke for an automated demonstration.'
  exit 0
fi
if [[ $# -gt 1 || ( $# == 1 && $1 != --smoke ) ]]; then
  printf '%s\n' 'Usage: run-native.sh [--methods] [--smoke|--help]' >&2
  exit 2
fi
for required in "$repl" "$server" "$agent" "$target" "$compdb"; do
  [[ -f $required ]] || { printf 'Missing demo input: %s\n' "$required" >&2; exit 1; }
done

run=$(mktemp -d "${TMPDIR:-/tmp}/native-prepared-demo.XXXXXXXX")
ln -s "$root/examples/prepared-injectors" "$run/demo"
ln -s "$(realpath -- "$compdb")" "$run/compile_commands.json"
"$target" >"$run/target.log" 2>&1 &
target_pid=$!
cleanup() {
  kill "$target_pid" 2>/dev/null || true
  wait "$target_pid" 2>/dev/null || true
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM HUP
cd -- "$run"
unset CLANG_TRACER_ENDPOINT

printf 'Native C++ REPL example: %s\nTarget PID: %s\n' "$run" "$target_pid"
if [[ $example == prepared-methods-target ]]; then
  if [[ ${1:-} == --smoke ]]; then
    {
      cat demo/methods/try.repl
      cat <<'REPL'
%collect --count 2 --timeout 15 --phase PRE --as entry
auto pre = tracer.batch("entry").at(0);
bool correct = e.argument_as<std::string>(0) == "deposit" && e.argument_snapshot(1).at(1).as<int>() == 5 && e.argument_snapshot(2).field("delta").as<int>() == 4 && e.result_snapshot().field("label").as<std::string>() == "deposit";
correct = correct && e.receiver_snapshot().field("prepared_demo::Account::account_id").as<int>() == 42 && e.receiver_snapshot().field("reserve").as<int>() == 25 && e.receiver_snapshot().field("ledger_id").as<int>() == 7 && e.receiver_snapshot().field("ledger_version").as<int>() == 3 && e.receiver_snapshot().field("ledger_owner").as<std::string>() == "inherited private" && e.receiver_snapshot().field("audit_id").as<int>() == 19;
correct = correct && e.receiver_snapshot().field("balance").as<int>() + 21 == e.result_snapshot().field("total").as<int>() && e.receiver_snapshot().field("last_adjustment").field("delta").as<int>() == 4;
correct = correct && pre.call_id() == e.call_id() && pre.receiver_snapshot().field("balance").as<int>() == e.receiver_snapshot().field("balance").as<int>();
correct = correct && e.receiver_snapshot().field("prepared_demo::Audit::account_id").as<int>() == 8 && e.receiver_snapshot().field("ledger_flags").as<int>() == -3;
correct = correct && e.receiver_snapshot().field("limits").field("second").as<std::string>() == "ceiling" && e.receiver_snapshot().field("description").field("item_1").as<std::string>() == "tuple" && e.receiver_snapshot().field("recent").at(1).as<int>() == 12;
correct = correct && e.receiver_snapshot().field("balance_address").as<unsigned long>() != 0 && e.receiver_snapshot().field("category").as<std::string>() == "savings";
auto receiver = e.receiver_snapshot();
correct = correct && e.argument_snapshot(3).field("value").as<int>() == 5 && e.argument_snapshot(4).field("value").as<int>() == 2 && !e.argument_snapshot(5).field("expired").as<bool>();
correct = correct && adjustment.delta == 4 && adjustment.limit.value() == 30;
correct = correct && std::get<2>(e.argument_as<std::variant<std::monostate, int, std::string, int>>(6)) == "argument choice";
correct = correct && receiver.field("ledger_credit").field("value").field("delta").as<int>() == 6 && receiver.field("empty_credit").field("address").as<unsigned long>() == 0 && receiver.field("empty_credit").field("value").is_null();
correct = correct && receiver.field("ledger_total").field("value").as<int>() == 81 && !receiver.field("ledger_total_ref").field("expired").as<bool>() && receiver.field("expired_total_ref").field("expired").as<bool>();
correct = correct && receiver.field("ledger_choice").field("index").as<unsigned>() == 3 && receiver.field("ledger_choice").field("alternative_3").as<int>() == 17 && receiver.field("ledger_choice").field("alternative_1").is_null();
correct = correct && receiver.field("ledger_empty_choice").field("index").as<unsigned>() == 0 && receiver.field("ledger_empty_choice").field("alternative_0").fields().empty();
std::cout << "NATIVE_METHOD_RESULTS=" << correct << std::endl;
%quit
REPL
    } | "$repl" --server "$server" --agent "$agent" --attach "$target_pid" --timeout 60 --fail-fast | tee "$run/smoke.log"
    grep -q '^NATIVE_METHOD_RESULTS=1$' "$run/smoke.log"
  else
    printf 'Paste these commands at the native tracer prompt.\n\n'
    cat demo/methods/try.repl
    printf '\n%%quit exits the REPL. Generated files will be in %s/prepared.\n' "$run"
    "$repl" --server "$server" --agent "$agent" --attach "$target_pid" --timeout 60
  fi
elif [[ ${1:-} == --smoke ]]; then
  "$repl" --server "$server" --agent "$agent" --attach "$target_pid" --timeout 60 --fail-fast <<'REPL'
%probe prepare "prepared_demo::compute(int)" --include demo/calculator.hpp --output prepared
%probe resolve "prepared_demo::compute(int)"
%probe validate "prepared_demo::compute(int)" --callback demo/interceptor.cpp
%probe inject "prepared_demo::compute(int)" --callback demo/interceptor.cpp --print-arguments --print-result
%collect --count 4 --timeout 15 --phase POST --as captured
auto captured = tracer.batch("captured");
std::cout << "NATIVE_EVENT_COUNT=" << captured.size() << std::endl;
bool correct = captured.size() == 4;
for (const auto& e : captured) { int value = e.argument_as<int>(0); correct = correct && e.result_as<int>() == value * 3 + 7; }
std::cout << "NATIVE_RESULTS=" << correct << std::endl;
tracer.detach(session);
%quit
REPL
else
  cat <<'STEPS'
Paste each command at the native tracer prompt:

%probe prepare "prepared_demo::compute(int)" --include demo/calculator.hpp --output prepared
%probe validate "prepared_demo::compute(int)" --callback demo/interceptor.cpp
%probe inject "prepared_demo::compute(int)" --callback demo/interceptor.cpp --print-arguments --print-result
%collect --count 4 --timeout 15 --phase POST --as captured
auto captured = tracer.batch("captured");
captured.size()
tracer.print(captured.at(0))
%probe remove PROBE_ID
%quit

Generated files: prepared/prepared.pb and prepared/generated/adapter.hpp, adapter.cpp.
Replace PROBE_ID with the number reported by injection. The callback stays
in demo/interceptor.cpp. Use %quit to restore all hooks and exit.
STEPS
  "$repl" --server "$server" --agent "$agent" --attach "$target_pid" --timeout 60
fi
printf 'Example files retained in %s\n' "$run"
