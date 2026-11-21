#!/usr/bin/env bash
# Exercise the process boundary as well as the in-process context checks.
set -euo pipefail
ulimit -c 0
repl=$1 server=$2 agent=$3 compiler=$4
root=$(mktemp -d "${TMPDIR:-/tmp}/shared-context-failures.XXXXXX")
pid=
cleanup() {
  if [[ -n $pid ]]; then
    kill -CONT "$pid" 2>/dev/null || true
    kill -TERM "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
  fi
  rm -rf "$root"
}
trap cleanup EXIT
fail() {
  cat "$root/repl.log" >&2
  tail -n 30 "$root/target.log" >&2
  echo "$mode: $*" >&2
  exit 1
}
cat > "$root/target.cpp" <<'CPP'
#include <cstdio>
#include <csignal>
#include <unistd.h>
#include <sys/prctl.h>
#ifndef PR_SET_PTRACER
#define PR_SET_PTRACER 0x59616d61
#define PR_SET_PTRACER_ANY ((unsigned long)-1)
#endif
volatile std::sig_atomic_t running = 1;
void stop(int) { running = 0; }
__attribute__((noinline)) int inner(int n) { volatile int v = n; return v + 1; }
__attribute__((noinline)) int outer(int n) { volatile int v = n; return inner(v) + 1; }
int main() {
  prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY);
  std::signal(SIGTERM, stop);
  for (int n = 1; running; ++n) {
    if (outer(n) != n + 2) return 3;
    std::printf("progress=%d\n", n); std::fflush(stdout);
    usleep(50000);
  }
}
CPP
"$compiler" -std=c++17 -g -O0 -fno-omit-frame-pointer -fcf-protection=none \
  "$root/target.cpp" -o "$root/target"
printf '[{"directory":"%s","file":"%s","arguments":["%s","-std=c++17","-g","-O0","-c","%s"]}]\n' \
  "$root" "$root/target.cpp" "$compiler" "$root/target.cpp" > "$root/compile_commands.json"
echo 'int outer(int); int inner(int);' > "$root/functions.hpp"
cat > "$root/state.hpp" <<'CPP'
#include <string>
#include <vector>
#include <optional>
struct Scope {};
struct Step { std::string name; int value = 0; bool ok = false; };
struct Request : shared_state<Scope> {
  std::string label;
  std::vector<Step> steps;
  std::vector<int> counts;
  std::optional<std::string> reason;
};
CPP
cat > "$root/outer.cpp" <<'CPP'
#include "state.hpp"
struct Interceptor {
  Interceptor(State<Request>);
  void pre(const Call&, Request& s) { s.label = "outer"; s.counts.push_back(1); }
  void post(const Call& call, Request& s) {
    s.steps.push_back({"outer", 1, true});
    call.context().event("scope.check");
  }
};
CPP
sed 's/call.context().event("scope.check");/if (s.counts.size() == 2) call.context().event("scope.check");/' \
  "$root/outer.cpp" > "$root/both-outer.cpp"
cat > "$root/inner.cpp" <<'CPP'
#include "state.hpp"
struct Interceptor {
  Interceptor(State<Request>);
  void post(const Call&, Request& s) { s.counts.push_back(2); }
};
CPP
cat > "$root/callback-failure.cpp" <<'CPP'
#include "state.hpp"
struct Interceptor {
  Interceptor(State<Request>);
  void pre(const Call&, Request&) { throw 7; }
  void post(const Call& call) { call.context().event("unexpected.post"); }
};
CPP
cat > "$root/construction-failure.cpp" <<'CPP'
struct FailureScope {};
struct Failure : shared_state<FailureScope> { Failure() { throw 7; } int value = 0; };
struct Interceptor {
  Interceptor(State<Failure>);
  void post(const Call& call) { call.context().event("unexpected.post"); }
};
CPP
cat > "$root/view.cpp" <<'CPP'
#include <string_view>
struct ViewScope {};
struct View : shared_state<ViewScope> { std::string_view value; };
struct Interceptor { Interceptor(State<View>); void pre(const Call&, View&) {} };
CPP
# Match the selected target compiler without relying on a developer compdb.
export TRACER_PREPARE_EXECUTABLE="$(dirname "$repl")/clang-tracer-prepare-injectors-native"
export NO_COLOR=1 LLVM_PROFILE_FILE=/dev/null
export XDG_CACHE_HOME="$root/cache"
unset CLANG_TRACER_ENDPOINT
install() {
  echo "%probe prepare \"$1(int)\" --include $root/functions.hpp --compdb $root/compile_commands.json --output $root/$mode-$1 --metadata-only"
  echo "%probe inject \"$1(int)\" --callback $root/$2.cpp --prepared $root/$mode-$1 --prepared-mode require"
}
cd "$root"
for mode in ${SHARED_CONTEXT_CASES:-outer inner both callback-failure construction-failure view view-with-probe}; do
  : > target.log
  ./target > target.log 2>&1 & pid=$!
  sleep .2
  {
    case $mode in inner) install inner inner;; esac
    case $mode in callback-failure|construction-failure) install inner "$mode";; esac
    case $mode in outer|callback-failure|construction-failure|view-with-probe) install outer outer;; esac
    case $mode in both) install outer both-outer; install inner inner;; esac
    case $mode in
      view) echo "%probe inject \"inner(int)\" --callback $root/view.cpp";;
      view-with-probe) install inner view;;
    esac
    echo '%probes'
    case $mode in
      inner|view) echo '%collect --name scope.check --count 1 --timeout 1 --as events';;
      *) echo '%collect --name scope.check --count 3 --timeout 15 --as events';
         for event_index in 0 1 2; do echo "%events show events --index $event_index"; done;;
    esac
    case $mode in *failure) echo '%collect --name unexpected.post --count 1 --timeout 1 --as unexpected';; esac
    echo '%quit'
  } > input.repl
  repl_status=0
  timeout 150 "$repl" --server "$server" --agent "$agent" --attach "$pid" \
    < input.repl > repl.log 2>&1 || repl_status=$?
  [[ $repl_status == 0 || ($repl_status == 1 && $mode == view*) ]] || fail 'REPL failed'
  kill -0 "$pid" 2>/dev/null || fail 'target exited'
  ! grep -Eq 'uncertain|remains stopped|cleanup incomplete|signal 11' repl.log || fail 'unsafe recovery'
  if [[ $mode == view* ]]; then
    grep -q 'unsupported shared state member value: std::string_view refers to borrowed storage' repl.log || fail 'missing view diagnostic'
    [[ $(grep -c '^error:' repl.log) == 1 ]] || fail 'unexpected error beside view rejection'
  else
    ! grep -q 'error:' repl.log || fail 'unexpected error'
  fi
  case $mode in
    inner|view) grep -q 'received=0 ' repl.log || fail 'unexpected events';;
    *) grep -q 'received=3 ' repl.log || fail 'missing events'
       expected='"counts":[1]'
       [[ $mode != both ]] || expected='"counts":[1,2]'
       [[ $(grep -Fc "$expected" repl.log) == 3 ]] || fail 'state did not propagate in every event';;
  esac
  case $mode in
    *failure) grep -Eq 'received=0 .*collection=unexpected' repl.log || fail 'failed callback published'
      reason=hook_failure
      [[ $mode != construction-failure ]] || reason=constructor_failure
      grep -q "$reason" target.log || fail 'missing failure diagnostic';;
  esac
  [[ $(awk '/^TracerPid:/{print $2}' /proc/"$pid"/status) == 0 ]] || fail 'target still traced'
  [[ $(awk '/^State:/{print $2}' /proc/"$pid"/status) != T ]] || fail 'target stopped'
  before=$(grep -c '^progress=' target.log)
  sleep .3
  [[ $(grep -c '^progress=' target.log) -gt $before ]] || fail 'no target progress after detach'
  kill -TERM "$pid"
  wait "$pid" || fail 'target result changed'
  pid=
  echo "$mode: passed"
done
