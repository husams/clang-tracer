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
mkdir directory-compdb
cat >application/exact_source.cpp <<'CPP'
#ifndef EXACT_TU
#error exact source compile command was not selected
#endif
namespace prepared_history {
int exact_source(int value) { return value + 1; }
}
CPP
cat >application/exact_source.hpp <<'CPP'
#ifndef FALLBACK_TU
#error the first same-directory compile command was not selected
#endif
namespace prepared_history {
int sibling_fallback(int value);
}
CPP
printf '[{"directory":"%s/application","file":"decoy.cpp","arguments":["clang++","-std=c++17","-DFALLBACK_TU=1","-c","decoy.cpp"]},{"directory":"%s/application","file":"exact_source.cpp","arguments":["clang++","-std=c++17","-DEXACT_TU=1","-c","exact_source.cpp"]}]\n' \
  "$run" "$run" > directory-compdb/compile_commands.json
cp directory-compdb/compile_commands.json application/compile_commands.json
printf '[{"directory":"%s/application","file":"history.cpp","arguments":["clang++","-std=c++17","-c","history.cpp"]}]\n' \
  "$run" > compile_commands.json
printf '[{"directory":"%s/application","file":"history.cpp","arguments":["clang","--driver-mode=cl","--target=x86_64-unknown-linux-gnu","/std:c++17","/c","history.cpp"]}]\n' \
  "$run" > clang-cl-compile_commands.json
mkdir -p application/include 'supplemental include'
cat >application/extra.hpp <<'CPP'
#include "history.hpp"
#include <prepare-priority.hpp>
#include <prepare-extra.hpp>
#if PREPARE_EXTRA != 37 || PREPARE_EXTRA_HEADER != 19 || PREPARE_DATABASE_HEADER != 73
#error expected additive flags and the original compilation database include order
#endif
CPP
echo '#define PREPARE_DATABASE_HEADER 73' >application/include/prepare-priority.hpp
echo '#error the supplemental directory shadowed the database include' >'supplemental include/prepare-priority.hpp'
echo '#define PREPARE_EXTRA_HEADER 19' >'supplemental include/prepare-extra.hpp'
printf '[{"directory":"%s/application","file":"extra.cpp","arguments":["clang++","-std=c++17","-isystem","include","-UPREPARE_DATABASE_UNDEFINED","-c","extra.cpp"]}]\n' \
  "$run" > extra-compile_commands.json
mkdir application/-include-dir
sed 's/"-isystem","include"/"-I","-include-dir"/' extra-compile_commands.json > extra-dash-compile_commands.json
mkdir -p alternate-cwd/alternate-include
printf '[{"directory":"%s/application","file":"extra.cpp","arguments":["clang++","-std=c++17","-working-directory","%s/alternate-cwd","-I","alternate-include","-c","extra.cpp"]}]\n' \
  "$run" "$run" > extra-working-directory-compile_commands.json
cat >extra-flags.sh <<'SH'
printf '%s\n' "-DPREPARE_EXTRA=37 -I'supplemental include'"
SH
cat >prepare.repl <<'REPL'
%probe prepare "prepared_history::via_pointer(const prepared_history::HistoryItem*, int)" --include application/history.hpp --output pointer
%probe prepare "prepared_history::via_reference(const prepared_history::HistoryItem&, int)" --include application/history.hpp --output reference
%probe prepare "prepared_history::via_value(prepared_history::HistoryItem, int)" --include application/history.hpp --output value
%probe prepare "HFW::HfwManager::createItem()" --include application/history.hpp --include application/payload.hpp --output manager-item --metadata-only
%probe prepare "prepared_history::uses_toolchain_headers(const std::error_code*, const tm*)" --include application/payload.hpp --compdb clang-cl-compile_commands.json --output toolchain-headers
%probe prepare "prepared_history::exact_source(int)" --include application/exact_source.cpp --compdb directory-compdb/compile_commands.json --output exact-source-file --metadata-only
%probe prepare "prepared_history::exact_source(int)" --include application/exact_source.cpp --compdb directory-compdb --output exact-source-dir --metadata-only
%probe prepare "prepared_history::sibling_fallback(int)" --include application/exact_source.hpp --compdb directory-compdb --output sibling-fallback --metadata-only
%probe prepare "prepared_history::exact_source(int)" --include exact_source.cpp --output ../exact-source-cwd --metadata-only --cwd application
%probe prepare "prepared_history::via_opaque(const prepared_history::OpaqueItem*, int)" --include application/extra.hpp --compdb extra-compile_commands.json --output extra-literal --extra-args "-DPREPARE_EXTRA=37 -I'supplemental include'"
%probe prepare "prepared_history::via_opaque(const prepared_history::OpaqueItem*, int)" --include application/extra.hpp --compdb extra-compile_commands.json --output opaque --extra-args `sh ./extra-flags.sh`
%probe prepare "prepared_history::via_opaque(const prepared_history::OpaqueItem*, int)" --include extra.hpp --compdb ../extra-compile_commands.json --output ../cwd-extra --extra-args `test -f extra.hpp && printf '%s\n' "-DPREPARE_EXTRA=37 -I'../supplemental include'"` --cwd application
REPL
"$repl" --fail-fast <prepare.repl
for package in exact-source-file exact-source-dir sibling-fallback exact-source-cwd cwd-extra; do
  test -s "$package/prepared.pb"
done
test -f extra-literal/generated/adapter.hpp
test -f opaque/generated/adapter.hpp
cat >prepare-startup.repl <<'REPL'
%probe prepare "prepared_history::via_opaque(const prepared_history::OpaqueItem*, int)" --include application/extra.hpp --compdb extra-compile_commands.json --output extra-startup
REPL
"$repl" --extra-args "$(sh ./extra-flags.sh)" --fail-fast <prepare-startup.repl
test -f extra-startup/generated/adapter.hpp
cat >conflicting-extra.repl <<'REPL'
%probe prepare "prepared_history::via_opaque(const prepared_history::OpaqueItem*, int)" --include application/extra.hpp --compdb extra-compile_commands.json --output extra-conflict --extra-args "-DPREPARE_EXTRA=37 -I'supplemental include' -DPREPARE_DATABASE_UNDEFINED=1"
%probe prepare "prepared_history::via_opaque(const prepared_history::OpaqueItem*, int)" --include application/extra.hpp --compdb extra-compile_commands.json --output extra-duplicate --extra-args "-Iapplication/include"
%probe prepare "prepared_history::via_opaque(const prepared_history::OpaqueItem*, int)" --include application/extra.hpp --compdb extra-dash-compile_commands.json --output extra-duplicate-dash --extra-args "-Iapplication/-include-dir"
%probe prepare "prepared_history::via_opaque(const prepared_history::OpaqueItem*, int)" --include application/extra.hpp --compdb extra-working-directory-compile_commands.json --output extra-duplicate-cwd --extra-args "-Ialternate-cwd/alternate-include"
%probe prepare "prepared_history::via_opaque(const prepared_history::OpaqueItem*, int)" --include application/extra.hpp --compdb extra-compile_commands.json --output extra-standard --extra-args "-std=c++20"
%probe prepare "prepared_history::via_opaque(const prepared_history::OpaqueItem*, int)" --include application/extra.hpp --compdb extra-compile_commands.json --output extra-shell-failed --extra-args `false`
REPL
if "$repl" <conflicting-extra.repl >conflicting-extra.log 2>&1; then
  echo "invalid extra compiler arguments unexpectedly succeeded" >&2
  exit 1
fi
grep -F 'extra compiler macro conflicts with existing compiler arguments: PREPARE_DATABASE_UNDEFINED' conflicting-extra.log
grep -F 'extra compiler include duplicates an existing compiler include: include' conflicting-extra.log
grep -F 'extra compiler include duplicates an existing compiler include: -include-dir' conflicting-extra.log
grep -F 'extra compiler include duplicates an existing compiler include: alternate-include' conflicting-extra.log
grep -F 'unsupported extra compiler option: -std' conflicting-extra.log
grep -F -- '--extra-args shell returned 1' conflicting-extra.log
for package in extra-conflict extra-duplicate extra-duplicate-dash extra-duplicate-cwd extra-standard extra-shell-failed; do
  test ! -e "$package"
done
cat >unsupported.repl <<'REPL'
%probe prepare "HFW::HfwManager::createBox()" --include application/history.hpp --output manager-box --metadata-only
REPL
if "$repl" --fail-fast <unsupported.repl >unsupported.log 2>&1; then
  echo "nested class-template preparation unexpectedly succeeded" >&2
  exit 1
fi
grep -F "cannot prepare standalone type 'HFW::HfwManager::ItemBox<int>'" unsupported.log
test ! -e manager-box
grep -Fx '#include <system_error>' toolchain-headers/generated/adapter.hpp
grep -Ex '#include <(time\.h|ctime)>' toolchain-headers/generated/adapter.hpp
unlink application/compile_commands.json
unlink application/exact_source.cpp
unlink application/exact_source.hpp
unlink directory-compdb/compile_commands.json
rmdir directory-compdb
unlink application/history.hpp
unlink application/payload.hpp
unlink application/extra.hpp
unlink application/include/prepare-priority.hpp
unlink 'supplemental include/prepare-priority.hpp'
unlink 'supplemental include/prepare-extra.hpp'
rmdir application/include application/-include-dir
unlink compile_commands.json
unlink clang-cl-compile_commands.json
unlink extra-compile_commands.json
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
#include <type_traits>

using NestedPayloadFromPackage = HFW::Payload::Nested;
using NestedResultFromPackage = HFW::HfwManager::ItemResult;
static_assert(std::is_same_v<HFW::ItemDetails<>,
                             HFW::ItemDetails<int, UP_SQLObject::False>>);
static_assert(UP_SQLObject::False == 2);
template<> class Interceptor<tracer::ResolvedSignature> {
public:
  void post(const Call &) {
    NestedPayloadFromPackage nested_payload_check{23};
    if (nested_payload_check.marker != 23)
      throw std::runtime_error("extra-header nested payload was not retained");
  }
};
CPP
mkdir -p callback-build/include
cat >callback-build/include/history-callback.hpp <<'CPP'
#ifndef _MT
#error expected the callback to use clang-cl driver mode
#endif
#if HISTORY_CALLBACK_CONTEXT != 73
#error expected the callback compilation database macro
#endif
constexpr const char *callback_event = "history.callback-compdb";
CPP
cat >callback-build/include/history-callback-config.hpp <<'CPP'
constexpr int expected_callback_result = 1;
CPP
echo '#error the supplemental directory shadowed the callback database include' >'supplemental include/history-callback.hpp'
echo 'constexpr int expected_extra_value = 19;' >'supplemental include/callback-extra.hpp'
touch callback-build/callback-donor.cpp callback-build/decoy.cpp
printf -v callback_command '{"directory":"%s/callback-build","file":"callback-donor.cpp","arguments":["clang-cl","--target=x86_64-unknown-linux-gnu","/std:c++17","/Iinclude","/DHISTORY_CALLBACK_CONTEXT=73","/FIhistory-callback-config.hpp","/c","callback-donor.cpp"]}' "$run"
printf -v decoy_command '{"directory":"%s/callback-build","file":"decoy.cpp","arguments":["clang++","-std=c++17","-Iinclude","-DHISTORY_CALLBACK_CONTEXT=42","-c","decoy.cpp"]}' "$run"
printf '[%s]\n' "$callback_command" > callback-compile_commands.json
printf '[%s,%s]\n' "$decoy_command" "$callback_command" > callback-choices-compile_commands.json
cat >clang-cl-mode.cpp <<'CPP'
#ifndef _MT
#error expected clang-cl driver mode from the prepared package
#endif
template<> class Interceptor<tracer::ResolvedSignature> {
public:
  void post(const Call &) {}
};
CPP
cat >toolchain-callback.cpp <<'CPP'
#include <history-callback.hpp>
#include <callback-extra.hpp>

static_assert(CALLBACK_STARTUP_EXTRA == 41);
static_assert(CALLBACK_LOCAL_EXTRA == expected_extra_value);

template<> class Interceptor<tracer::ResolvedSignature> {
public:
  void post(const Call &call) {
    if (call.result() == expected_callback_result)
      call.context().event(callback_event);
  }
};
CPP
cat >verify.repl <<'REPL'
%probe validate "prepared_history::via_pointer(const prepared_history::HistoryItem*, int)" --callback pointer.cpp --compile-option "-fno-access-control" --prepared pointer --prepared-mode require
%probe inject "prepared_history::via_pointer(const prepared_history::HistoryItem*, int)" --callback ../pointer.cpp --compile-option "-fno-access-control" --prepared ../pointer --prepared-mode require --cwd "supplemental include"
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
%probe inject "prepared_history::via_opaque(const prepared_history::OpaqueItem*, int)" --callback snapshot.cpp --prepared cwd-extra --prepared-mode require
%collect --name "prepared_history::via_opaque(const prepared_history::OpaqueItem*, int)" --phase POST --count 1 --timeout 15 --as opaque
if (tracer.batch("opaque").size() != 1) throw std::runtime_error("missing address-only pointer event");
%probe validate "HFW::HfwManager::createItem()" --callback manager-snapshot.cpp --prepared manager-item --prepared-mode require
%probe inject "HFW::HfwManager::createItem()" --callback manager-snapshot.cpp --prepared manager-item --prepared-mode require
%collect --name "HFW::HfwManager::createItem()" --phase POST --count 1 --timeout 15 --as manager-item
auto manager_item = tracer.batch("manager-item").at(0);
auto returned_item = manager_item.result_snapshot();
if (returned_item.field("id").as<int>() != 73 || returned_item.field("kind").as<int>() != 3 || returned_item.field("details").field("quantity").as<int>() != 5) throw std::runtime_error("wrong nested manager item fields");
if (manager_item.receiver_snapshot().field("seed").as<int>() != 71) throw std::runtime_error("wrong manager receiver fields");
%probe validate "prepared_history::uses_toolchain_headers(const std::error_code*, const tm*)" --callback clang-cl-mode.cpp --prepared toolchain-headers --prepared-mode require
%probe validate "prepared_history::uses_toolchain_headers(const std::error_code*, const tm*)" --callback ../toolchain-callback.cpp --prepared ../toolchain-headers --prepared-mode require --compdb ../callback-compile_commands.json --extra-args "/DCALLBACK_LOCAL_EXTRA=19 /I." --cwd "supplemental include"
%probe inject "prepared_history::uses_toolchain_headers(const std::error_code*, const tm*)" --callback toolchain-callback.cpp --prepared toolchain-headers --prepared-mode require --compdb callback-choices-compile_commands.json --translation-unit callback-build/callback-donor.cpp --extra-args "-DCALLBACK_LOCAL_EXTRA=19 -I'supplemental include'"
%collect --name history.callback-compdb --count 1 --timeout 15 --as callback-compdb
if (tracer.batch("callback-compdb").size() != 1) throw std::runtime_error("missing compilation database callback event");
std::cout << "PREPARED_CLANG_CL_DRIVER=1" << std::endl;
std::cout << "PREPARED_HISTORY_FIELDS=1" << std::endl;
%quit
REPL

"$target" >target.log 2>&1 &
pid=$!
trap 'kill "$pid" 2>/dev/null || true; wait "$pid" 2>/dev/null || true' EXIT
"$repl" --server "$server" --agent "$agent" --attach "$pid" --timeout 60 \
  --extra-arg=-DCALLBACK_STARTUP_EXTRA=41 --fail-fast <verify.repl | tee smoke.log
test "$(grep -c '^installed probe=' smoke.log)" -eq 6
grep -qx 'prepared_used: true' smoke.log
grep -qx PREPARED_CLANG_CL_DRIVER=1 smoke.log
grep -qx PREPARED_HISTORY_FIELDS=1 smoke.log

sed 's/--target=x86_64-unknown-linux-gnu/--target=x86_64-pc-windows-msvc/' \
  callback-compile_commands.json > incompatible-compile_commands.json
sed 's|"/std:c++17",|"/std:c++17","-Xclang","-load","-Xclang","missing.so",|' \
  callback-compile_commands.json > unsafe-compile_commands.json
cat >invalid-compdb.repl <<'REPL'
%probe validate "prepared_history::uses_toolchain_headers(const std::error_code*, const tm*)" --callback toolchain-callback.cpp --prepared toolchain-headers --prepared-mode require --compdb incompatible-compile_commands.json
%probe inject "prepared_history::uses_toolchain_headers(const std::error_code*, const tm*)" --callback toolchain-callback.cpp --prepared toolchain-headers --prepared-mode require --compdb unsafe-compile_commands.json
%probe inject "prepared_history::uses_toolchain_headers(const std::error_code*, const tm*)" --callback toolchain-callback.cpp --prepared toolchain-headers --prepared-mode require --compdb callback-compile_commands.json --extra-args "-DHISTORY_CALLBACK_CONTEXT=74"
%quit
REPL
if "$repl" --server "$server" --agent "$agent" --attach "$pid" --timeout 60 \
    <invalid-compdb.repl >invalid-compdb.log 2>&1; then
  echo "invalid callback compilation database unexpectedly succeeded" >&2
  exit 1
fi
grep -F 'compilation database target does not match the engine target' invalid-compdb.log
grep -F 'header compilation command contains unsupported option: -Xclang' invalid-compdb.log
grep -F 'extra compiler macro conflicts with existing compiler arguments: HISTORY_CALLBACK_CONTEXT' invalid-compdb.log
if grep -q '^installed probe=' invalid-compdb.log; then
  cat invalid-compdb.log >&2
  exit 1
fi
kill -0 "$pid"
