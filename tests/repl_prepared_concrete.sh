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
echo "Concrete prepared demo: $run"
cd "$run"
mkdir application
cp "$header" application/types.hpp
printf '[{"directory":"%s/application","file":"target.cpp","arguments":["clang++","-std=c++17","-c","target.cpp"]}]\n' \
  "$run" > application/compile_commands.json

selectors=(
  'prepared_concrete::Handler::route(int) const'
  'prepared_concrete::twice<int>(int)'
  'prepared_concrete::twice<double>(double)'
  'prepared_concrete::Handler::convert<int>(int) const'
  'prepared_concrete::Handler::convert<double>(double) const'
  'prepared_concrete::Box<int, 5>::apply(int) const'
)
types=(int int double int double int)
results=(23 8 4.5 11 11.5 15)

for i in "${!selectors[@]}"; do
  printf '%%probe prepare "%s" --include application/types.hpp --compdb application/compile_commands.json --output package-%s --metadata-only\n' \
    "${selectors[$i]}" "$i"
done > prepare.repl
"$repl" --fail-fast < prepare.repl
unlink application/types.hpp
unlink application/compile_commands.json
rmdir application
test ! -e application
echo APPLICATION_HEADERS_REMOVED

cat > interceptor.cpp <<'CPP'
template<> class Interceptor<tracer::ResolvedResult(tracer::ResolvedArgument<0>)> {
public:
  void post(const Call &) {}
};
CPP

"$target" >target.log 2>&1 &
pid=$!
trap 'kill "$pid" 2>/dev/null || true; wait "$pid" 2>/dev/null || true' EXIT

{
  printf '%%probe validate "%s" --callback interceptor.cpp --prepared package-0 --prepared-mode require\n' \
    "${selectors[0]}"
  for i in "${!selectors[@]}"; do
    printf '%%probe inject "%s" --callback interceptor.cpp --prepared package-%s --prepared-mode require\n' \
      "${selectors[$i]}" "$i"
    printf '%%collect --name "%s" --phase POST --count 1 --timeout 15 --as capture\n' "${selectors[$i]}"
    printf 'if (tracer.batch("capture").size() != 1) throw std::runtime_error("missing event for probe %s");\n' "$i"
    printf 'auto e%s = tracer.batch("capture").at(0);\n' "$i"
    printf 'if (e%s.result_as<%s>() != %s) throw std::runtime_error("wrong result for probe %s");\n' \
      "$i" "${types[$i]}" "${results[$i]}" "$i"
  done
  cat <<'REPL'
if (e0.receiver_snapshot().field("factor").as<int>() != 7 || e0.receiver_snapshot().field("base").as<int>() != 3 || e0.receiver_snapshot().field("shared").as<int>() != 9) throw std::runtime_error("wrong virtual receiver snapshot");
if (e5.receiver_snapshot().field("stored").as<int>() != 6) throw std::runtime_error("wrong template receiver snapshot");
std::cout << "PREPARED_CONCRETE_RESULTS=1" << std::endl;
%quit
REPL
} > verify.repl
"$repl" --server "$server" --agent "$agent" --attach "$pid" --timeout 60 \
  --fail-fast < verify.repl | tee smoke.log
test "$(grep -c '^installed probe=' smoke.log)" -eq "${#selectors[@]}"
grep -qx 'prepared_used: true' smoke.log
grep -qx PREPARED_CONCRETE_RESULTS=1 smoke.log
kill -0 "$pid"
