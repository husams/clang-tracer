#!/usr/bin/env bash
# A context hook calls functions of the target process through the target-call
# bridge: virtual and free functions, a std::string result that crosses from
# the target's runtime into the hook's, a target function that throws, and a
# target function that carries its own probe.
set -euo pipefail

repl=$1
server=$2
agent=$3
target=$4
fixtures=$5
unset CLANG_TRACER_ENDPOINT
export TRACER_PREPARE_EXECUTABLE="$(dirname "$repl")/clang-tracer-prepare-injectors-native"

run=$(mktemp -d)
echo "Context target calls: $run"
cd "$run"
export HOME="$run/home"
mkdir home application
cp "$fixtures/items.hpp" "$fixtures/payload.hpp" application/
printf '[{"directory":"%s/application","file":"items.cpp","arguments":["clang++","-std=c++17","-c","items.cpp"]}]\n' \
  "$run" > compile_commands.json

selector='target_calls::populate(std::vector<target_calls::Item *> &)'
total='target_calls::total(const std::vector<target_calls::Item *> &)'

hook_begin='template <>
class Interceptor<void(std::vector<target_calls::Item *> &)> {
public:
  void pre(const Call &) {}
  void post(const Call &call) {
    target_calls_probe::Payload payload;'
hook_end='    call.context().init_scope<target_calls_probe::ItemsScope>(std::move(payload));
    call.context().event("target.calls");
  }
};'
declarations='namespace target_calls {
int total(const std::vector<Item *> &items);
bool heavier(const Item &item, int limit);
int failing(int value);
}'

cat >calls.cpp <<CPP
$declarations
$hook_begin
    for (const target_calls::Item *item : call.argument<0>()) {
      payload.items.push_back(item->serialize());
      payload.labels.push_back(item->next()->label());
      payload.numbers.push_back(item->weight());
      payload.numbers.push_back(target_calls::heavier(*item, 1) ? 1 : 0);
    }
    payload.numbers.push_back(target_calls::total(call.argument<0>()));
$hook_end
CPP
cat >throws.cpp <<CPP
$declarations
$hook_begin
    payload.numbers.push_back(target_calls::failing(7));
$hook_end
CPP
cat >stored.cpp <<CPP
$declarations
template <>
class Interceptor<void(std::vector<target_calls::Item *> &)> {
public:
  void pre(const Call &) {}
  void post(const Call &call) {
    const int total = target_calls::total(call.argument<0>());
    const std::string first = call.argument<0>().front()->serialize();
    call.context().init_scope<target_calls_probe::ItemsScope>(target_calls_probe::Payload{});
    call.context().with<target_calls_probe::ItemsScope, target_calls_probe::Payload>(
        [&](target_calls_probe::Payload &state) {
          state.numbers.push_back(total);
          state.items.push_back(first);
        });
    call.context().event("target.stored");
  }
};
CPP
cat >caught.cpp <<CPP
$declarations
$hook_begin
    try {
      payload.numbers.push_back(target_calls::failing(7));
    } catch (...) {
      payload.numbers.push_back(-1);
    }
$hook_end
CPP
cat >locked.cpp <<CPP
#include <tracer/context.hpp>
$declarations
struct LockedScope {};
struct LockedState : tracer::shared_state<LockedScope> { int value = 0; };
template <>
class Interceptor<void(std::vector<target_calls::Item *> &)> {
public:
  Interceptor(tracer::State<LockedState>);
  void pre(const Call &) {}
  void post(const Call &call) {
    call.context().get<LockedState>().value = target_calls::total(call.argument<0>());
    call.context().event("target.locked");
  }
};
CPP
cat >by-value.cpp <<CPP
namespace target_calls { void store(Item item); }
$hook_begin
    for (const target_calls::Item *item : call.argument<0>()) target_calls::store(*item);
$hook_end
CPP
cat >missing.cpp <<CPP
namespace target_calls { int missing(int value); }
$hook_begin
    payload.numbers.push_back(target_calls::missing(1));
$hook_end
CPP
cat >plain.cpp <<'CPP'
template <> class Interceptor<tracer::ResolvedSignature> {
public:
  void post(const Call &) {}
};
CPP

"$target" >target.log 2>&1 &
pid=$!
trap 'kill "$pid" 2>/dev/null || true; wait "$pid" 2>/dev/null || true' EXIT
session() { "$repl" --server "$server" --agent "$agent" --attach "$pid" --timeout 60 "$@"; }
print_payload='for (const auto &text : payload.field("items").elements()) std::cout << "item=" << text.as<std::string>() << std::endl; for (const auto &text : payload.field("labels").elements()) std::cout << "label=" << text.as<std::string>() << std::endl; std::cout << "numbers="; for (const auto &number : payload.field("numbers").elements()) std::cout << number.as<int>() << ","; std::cout << std::endl;'

# The hook calls four virtual methods and two free functions of the target.
cat >prepare.repl <<REPL
%probe prepare "$selector" --include application/items.hpp --include application/payload.hpp --output prepared
%probe prepare "$total" --include application/items.hpp --output prepared-total
REPL
"$repl" --fail-fast <prepare.repl
cat >calls.repl <<REPL
%probe validate "$selector" --callback calls.cpp --prepared prepared --prepared-mode require
%probe inject "$selector" --callback calls.cpp --prepared prepared --prepared-mode require
%collect --name target.calls --count 2 --timeout 20 --as calls
auto payload = tracer.batch("calls").at(1).state_snapshot();
$print_payload
%quit
REPL
session --fail-fast <calls.repl | tee calls.log
grep -qx 'item=alpha:1' calls.log
grep -qx 'item=a considerably longer name than the small string buffer:2' calls.log
grep -qx 'label=a considerably longer name than the small string buffer' calls.log
grep -qx 'label=alpha' calls.log
grep -qx 'numbers=1,0,2,1,3,' calls.log
kill -0 "$pid"

# A target function that carries its own probe is still called: its probe is
# not traced while the hook runs, and the call is not rejected.
cat >probed.repl <<REPL
%probe inject "$total" --callback plain.cpp --prepared prepared-total --prepared-mode require
%probe inject "$selector" --callback calls.cpp --prepared prepared --prepared-mode require
%collect --name target.calls --count 2 --timeout 20 --as calls
auto payload = tracer.batch("calls").at(1).state_snapshot();
$print_payload
%quit
REPL
session --fail-fast <probed.repl | tee probed.log
test "$(grep -c '^installed probe=' probed.log)" -eq 2
grep -qx 'numbers=1,0,2,1,3,' probed.log
kill -0 "$pid"

# The state is filled through a visitor after the calls.
cat >stored.repl <<REPL
%probe inject "$selector" --callback stored.cpp --prepared prepared --prepared-mode require
%collect --name target.stored --count 2 --timeout 20 --as stored
auto stored = tracer.batch("stored").at(1).state_snapshot();
std::cout << "stored=" << stored.field("numbers").at(0).as<int>() << "," << stored.field("items").at(0).as<std::string>() << std::endl;
%quit
REPL
session --fail-fast <stored.repl | tee stored.log
grep -qx 'stored=3,alpha:1' stored.log
kill -0 "$pid"

# The exception of a target function is caught on the target side and ends the
# hook: no event is published and the target keeps running.
cat >throws.repl <<REPL
%probe inject "$selector" --callback throws.cpp --prepared prepared --prepared-mode require
%collect --name target.calls --count 1 --timeout 5 --as thrown
std::cout << "thrown-events=" << tracer.batch("thrown").size() << std::endl;
%quit
REPL
session <throws.repl >throws.log 2>&1 || true
cat throws.log
grep -q '^installed probe=' throws.log
grep -qx 'thrown-events=0' throws.log
sleep 1
kill -0 "$pid"

# The hook sees the failure as an exception of its own runtime and can handle
# it.
cat >caught.repl <<REPL
%probe inject "$selector" --callback caught.cpp --prepared prepared --prepared-mode require
%collect --name target.calls --count 2 --timeout 20 --as calls
auto payload = tracer.batch("calls").at(1).state_snapshot();
$print_payload
%quit
REPL
session --fail-fast <caught.repl | tee caught.log
grep -qx 'numbers=-1,' caught.log
kill -0 "$pid"

# Target code never runs under a context lock, a call the bridge cannot carry
# is rejected with the callee, the reason and the position, and so is a
# function that the target does not have.
cat >rejected.repl <<REPL
%probe validate "$selector" --callback locked.cpp --prepared prepared --prepared-mode require
%probe validate "$selector" --callback by-value.cpp --prepared prepared --prepared-mode require
%probe validate "$selector" --callback missing.cpp --prepared prepared --prepared-mode require
%probe inject "$selector" --callback missing.cpp --prepared prepared --prepared-mode require
%quit
REPL
session <rejected.repl >rejected.log 2>&1 || true
cat rejected.log
grep -F 'invalid_context_access_plan: target function target_calls::total cannot be called in a hook that holds a direct access frame' rejected.log
grep -F 'unsupported_context_effect: call to target function target_calls::store: it takes a by-value parameter of type Item (parameter 1)' rejected.log
grep -F 'invalid_context_target_call: the attached process does not export target function target_calls::missing (_ZN12target_calls7missingEi)' rejected.log
test "$(grep -c 'does not export target function' rejected.log)" -ge 2
if grep -q '^installed probe=' rejected.log || grep -q 'has no visible body' rejected.log; then
  exit 1
fi

# After the failures the bridge still works.
session --fail-fast <calls.repl | tee again.log
grep -qx 'numbers=1,0,2,1,3,' again.log
kill -0 "$pid"
echo CONTEXT_TARGET_CALLS=1
