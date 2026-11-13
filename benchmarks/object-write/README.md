# Two-worker native write capture

Build the fixture once with GCC 8.5, optimized with debug information and
separate translation units. Each worker calls the same out-of-line loop
on its own half of the array. The progress counter is a separate 64-bit
store and is present in every run. The REPL verifies the original 32-bit
store and its successor before arming.

```sh
gxx=/opt/toolchain/x86_64-2.6.32-v4.0.137/bin/g++
"$gxx" -std=c++17 -O2 -g -fno-omit-frame-pointer -fno-tree-vectorize \
  -fcf-protection=none -fno-pie -no-pie -pthread -Wl,--build-id=sha1 \
  -I tests/fixtures/person tests/fixtures/person/person.cpp \
  benchmarks/object-write/{driver,update}.cpp -o object-writes-gcc85
python3 -u benchmarks/object-write/run.py build/clang-tracer-repl \
  build/clang-tracer-server build/libclang_tracer_agent.so \
  ./object-writes-gcc85 --backend both --method --writes 2000 --trials 5
```

The fixture measures from the earliest worker loop entry to the latest
loop exit. Setup, attach, construction, and the startup barrier precede
that interval. All four traced conditions run the same SQLite recorder with
stack capture disabled. Recording runs concurrently with the loop.
Final drain, replay, detach, and result verification follow the timed
interval. Each object-watch trial checks the complete destination/TID set
against the two disjoint ranges and checks for event loss. The ordinary
entry probe traces the `WriteBatch::run()` call in each worker and checks
both PRE/POST pairs. The fixture reports the workers' total CPU time inside
the timed interval. For the code cache the
runner checks the original store bytes, verifies each direct redirect
into anonymous executable memory, checks that the target tasks are free
of ptrace ownership, and matches the translated-store counter.

Run active detach separately. Both workers must have SQLite records and
report partial progress before detach is issued. The runner reattaches
to the same PID, rereads the complete instruction, and verifies that
both workers finished their ranges. Records cover the prefix before
detach. The fixture verifies all final fields.

```sh
python3 -u benchmarks/object-write/run.py build/clang-tracer-repl \
  build/clang-tracer-server build/libclang_tracer_agent.so \
  ./object-writes-gcc85 --writes 2000 --trials 3 --detach
```

Use the paced atomic fixture to compare active detach while both workers
are executing LOCK XADD. The runner checks partial progress before detach,
continued progress afterward, final values, and complete byte restoration.

```sh
"$gxx" -std=c++17 -O2 -g -fno-omit-frame-pointer -fcf-protection=none \
  -fno-pie -no-pie -pthread -Wl,--build-id=sha1 \
  tests/fixtures/atomic_person/{operations,driver}.cpp -o atomic-person-gcc85
python3 -u tests/repl_atomic_store.py build/clang-tracer-repl \
  build/clang-tracer-server build/libclang_tracer_agent.so \
  ./atomic-person-gcc85 --backend both --detach --iterations 1000 --trials 3
```

Use the same atomic command without `--detach` to verify atomic store,
fetch-add, and compare-and-exchange in both value modes.
