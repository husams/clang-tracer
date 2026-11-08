# Console string values and hook replacement — 2026-11-04

Branch: `codex/cpp17-console-values-and-reprobe`, based on C++17 `953d505`.

- [x] Decode shared-state payloads using the schema bundled with each event;
  display C++ field names and actual strings, nested fields and containers.
  Preserve terminal escaping, decoding diagnostics and bounded output.
- [x] Re-inject the same address in the same session with a new probe ID.
  Prepare the new callback before restoring/draining the previous hook, then
  install the replacement using the original prologue. Keep overlap and
  cross-session protections. Compilation rejection preserves the old hook.
- [x] Keep compiler workers and threads without a parent context out of the
  application task ledger, so retained JIT workers cannot block hook removal.
- [x] Add regression coverage for replacement, string decoding, compiler-thread
  suppression and `bool(const std::string&)` with a captured shared-state string.
- [x] Preserve the PCH producer's target in tooling consumers, and keep the
  affected GCC 8–11 string-header templates textual for interpreter correctness.
- [x] Resolve the actual libc provider in the runtime identity test rather than
  an executable's PLT stub.
- [x] Repair the PTY cancellation assertion, Protobuf 5.29 repeated-field test
  mutation, and all reported Python type-check errors.
- [x] Preserve ABI-bearing inline namespaces when comparing signature template
  identities; valid `std::__cxx11::basic_string` must match while ABI 0 is rejected.
- [x] Carry fully specialized interceptor types through shared-state planning,
  lifecycle lowering and destruction, including the signature template prelude.
  Verify syntax/compile validation, injection, actual string capture and replacement
  for both ordinary and `Interceptor<bool(const std::string&)>` shared hooks.
- [x] Keep generated wheel build copies out of Python source linting, matching
  the existing generated-protobuf exclusion; repair the final test line length.
- [x] Finish clean full Linux C++ and Python suites after the typed shared fix.
- [x] Install the rebuilt native runtime and verified Python wheel on linux-dev.
- [x] Verify both fixes through the installed interactive console.

## Validation

- Final complete Linux native suite: **38/38 passed, zero failures**, 302.57s
  (`ctest-final.log`).
- Final complete Linux Python suite: **246/246 passed, zero failures or skips**,
  222.05s (`python-final.log`), including all native cases and both ordinary and
  signature-specialized shared-state hooks.
- Full Python lint and strict mypy checks passed (all 10 source files).
- Installed interactive console: actual argument `"signature fixture"` decoded,
  same symbol replaced with a new ID emitting `"installed updated value"`,
  original bytes restored, detached and server shut down. Both ordinary and
  `Interceptor<bool(const std::string&)>` versions passed
  (`installed-smoke-final.log`, `installed-typed-smoke-final.log`).
- Installed agent passes export/dependency isolation checks and has the same
  build ID as the tested binary; CMake adjusts its installation runtime path.
  Installed Python value decoder matches tested source byte-for-byte.
- Verification logs and installation backup are under
  `/tmp/clang-tracer-console-fixes-cpp17-20261104/` on `linux-dev`.

## Delivery notes

Rebuild/reinstall the Python client, native server and native agent together.
Bootstrap version 19 rejects an already loaded older agent; restart that target
before attaching with the rebuilt agent. Replacement has a brief untraced
interval while the old hook is restored and drained. The existing 32 retained
probe limit still counts replaced versions.

Installed on `linux-dev`: `/usr/local/bin/clang-tracer`,
`/usr/local/bin/clang-tracer-server`, `/usr/local/lib64/libclang_tracer_agent.so`
and `/home/husam/.local/bin/clang-tracer-console`. The Python tool uses the final
wheel and the exact verified dependency constraints. Prior installation backup:
`/tmp/clang-tracer-console-fixes-cpp17-20261104/installed-before/`.

On 2026-11-04, the user authorized committing all these fixes, merging them
into `c++17` and pushing that branch; the validation above covers the source
changes being delivered.
The original workspace also retains a separate C++20 implementation on
`codex/console-values-and-reprobe`.
