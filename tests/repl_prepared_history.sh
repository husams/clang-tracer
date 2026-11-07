#!/usr/bin/env bash
set -euo pipefail

repl=$1
server=$2
agent=$3
target=$4
header=$5
unset CLANG_TRACER_ENDPOINT
export TRACER_PREPARE_EXECUTABLE="$(dirname "$repl")/clang-tracer-prepare-injectors-native"

run=$(mktemp -d)
echo "Prepared history demo: $run"
cd "$run"
mkdir application
cp "$header" application/history.hpp
cp "$(dirname "$header")/payload.hpp" application/payload.hpp
printf '[{"directory":"%s/application","file":"history.cpp","arguments":["clang++","-std=c++17","-c","history.cpp"]}]\n' \
  "$run" > compile_commands.json
cat >prepare.repl <<'REPL'
%probe prepare "prepared_history::via_pointer(const prepared_history::HistoryItem*, int)" --include application/history.hpp --output pointer
%probe prepare "prepared_history::via_reference(const prepared_history::HistoryItem&, int)" --include application/history.hpp --output reference
%probe prepare "prepared_history::via_value(prepared_history::HistoryItem, int)" --include application/history.hpp --output value
%probe prepare "prepared_history::via_opaque(const prepared_history::OpaqueItem*, int)" --include application/history.hpp --output opaque
%probe prepare "HFW::HfwManager::createItem()" --include application/history.hpp --include application/payload.hpp --output manager-item --metadata-only
REPL
"$repl" --fail-fast <prepare.repl
cat >unsupported.repl <<'REPL'
%probe prepare "HFW::HfwManager::createBox()" --include application/history.hpp --output manager-box --metadata-only
REPL
if "$repl" --fail-fast <unsupported.repl >unsupported.log 2>&1; then
  echo "nested class-template preparation unexpectedly succeeded" >&2
  exit 1
fi
grep -F "cannot prepare standalone type 'HFW::HfwManager::ItemBox<int>'" unsupported.log
test ! -e manager-box
unlink application/history.hpp
unlink application/payload.hpp
unlink compile_commands.json
rmdir application
test ! -e application
echo APPLICATION_HEADERS_REMOVED

cat >pointer.cpp <<'CPP'
struct HistoryScope {};
struct HistoryFields {
  std::string label;
  int code;
  int sequence;
};
template<> class Interceptor<tracer::ResolvedResult(
    tracer::ResolvedArgument<0>, tracer::ResolvedArgument<1>)> {
public:
  void pre(const Call &call) {
    if (const auto *item = call.argument<0>())
      call.context().init_scope<HistoryScope>(
          HistoryFields{item->label, item->code, item->sequence});
  }
  void post(const Call &call) {
    if (call.result())
      call.context().event("history.pointer");
  }
};
CPP
cat >snapshot.cpp <<'CPP'
template<> class Interceptor<tracer::ResolvedResult(
    tracer::ResolvedArgument<0>, tracer::ResolvedArgument<1>)> {
public:
  void post(const Call &) {}
};
CPP
cat >manager-snapshot.cpp <<'CPP'
using NestedPayloadFromPackage = HFW::Payload::Nested;
using NestedResultFromPackage = HFW::HfwManager::ItemResult;
template<> class Interceptor<tracer::ResolvedSignature> {
public:
  void post(const Call &) {
    NestedPayloadFromPackage nested_payload_check{23};
    if (nested_payload_check.marker != 23)
      throw std::runtime_error("extra-header nested payload was not retained");
  }
};
CPP
cat >verify.repl <<'REPL'
%probe validate "prepared_history::via_pointer(const prepared_history::HistoryItem*, int)" --callback pointer.cpp --compile-option "-fno-access-control" --prepared pointer --prepared-mode require
%probe inject "prepared_history::via_pointer(const prepared_history::HistoryItem*, int)" --callback pointer.cpp --compile-option "-fno-access-control" --prepared pointer --prepared-mode require
%collect --name history.pointer --count 1 --timeout 15 --as pointer
auto pointer_fields = tracer.batch("pointer").at(0).state_snapshot();
if (pointer_fields.field("label").as<std::string>() != "entry" || pointer_fields.field("code").as<int>() != 42 || pointer_fields.field("sequence").as<int>() != 7) throw std::runtime_error("wrong pointer fields");
%probe inject "prepared_history::via_reference(const prepared_history::HistoryItem&, int)" --callback snapshot.cpp --prepared reference --prepared-mode require
%collect --name "prepared_history::via_reference(const prepared_history::HistoryItem&, int)" --phase POST --count 1 --timeout 15 --as reference
auto ref = tracer.batch("reference").at(0);
if (ref.argument_snapshot(0).field("code").as<int>() != 42 || ref.argument_snapshot(0).field("sequence").as<int>() != 7 || ref.result_snapshot().field("label").as<std::string>() != "entry") throw std::runtime_error("wrong reference fields");
%probe inject "prepared_history::via_value(prepared_history::HistoryItem, int)" --callback snapshot.cpp --prepared value --prepared-mode require
%collect --name "prepared_history::via_value(prepared_history::HistoryItem, int)" --phase POST --count 1 --timeout 15 --as value
auto value = tracer.batch("value").at(0);
if (value.argument_snapshot(0).field("label").as<std::string>() != "entry" || value.result_snapshot().field("label").as<std::string>() != "entry4" || value.result_snapshot().field("code").as<int>() != 42) throw std::runtime_error("wrong by-value fields");
%probe inject "prepared_history::via_opaque(const prepared_history::OpaqueItem*, int)" --callback snapshot.cpp --prepared opaque --prepared-mode require
%collect --name "prepared_history::via_opaque(const prepared_history::OpaqueItem*, int)" --phase POST --count 1 --timeout 15 --as opaque
if (tracer.batch("opaque").size() != 1) throw std::runtime_error("missing address-only pointer event");
%probe inject "HFW::HfwManager::createItem()" --callback manager-snapshot.cpp --prepared manager-item --prepared-mode require
%collect --name "HFW::HfwManager::createItem()" --phase POST --count 1 --timeout 15 --as manager-item
auto manager_item = tracer.batch("manager-item").at(0);
auto returned_item = manager_item.result_snapshot();
if (returned_item.field("id").as<int>() != 73 || returned_item.field("kind").as<int>() != 3 || returned_item.field("details").field("quantity").as<int>() != 5) throw std::runtime_error("wrong nested manager item fields");
if (manager_item.receiver_snapshot().field("seed").as<int>() != 71) throw std::runtime_error("wrong manager receiver fields");
std::cout << "PREPARED_HISTORY_FIELDS=1" << std::endl;
%quit
REPL

"$target" >target.log 2>&1 &
pid=$!
trap 'kill "$pid" 2>/dev/null || true; wait "$pid" 2>/dev/null || true' EXIT
"$repl" --server "$server" --agent "$agent" --attach "$pid" --timeout 60 \
  --fail-fast <verify.repl | tee smoke.log
test "$(grep -c '^installed probe=' smoke.log)" -eq 5
grep -qx 'prepared_used: true' smoke.log
grep -qx PREPARED_HISTORY_FIELDS=1 smoke.log
kill -0 "$pid"
