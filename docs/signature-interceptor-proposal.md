Use the function signature as the template argument: `Interceptor<MyClass(int)>` or `Interceptor<int(MyClass&)>`. The tracer recovers required types from the target's DWARF or uses matching application headers supplied through per-request compiler options. It derives return and parameter types from the signature and provides typed access inside PRE and POST hooks. The server provides explicit syntax and compilation validation, and injection must pass full validation before installation.

# Proposed usage

For a by-value class result, the engine first reconstructs `MyClass` from sufficient DWARF layout and calling-convention evidence, then makes that type available before compiling this callback:

```cpp
template <>
class Interceptor<MyClass(int)> {
public:
  void pre(const Call& call) {
    const auto seed = call.argument<0>(); // int
    call.field("seed", seed);
  }

  void post(const Call& call) {
    const auto& object = call.result();  // const MyClass&
    call.field("value", object.value);
  }
};
```

The engine supplies the primary `Interceptor<Signature>` template and keeps `Call` non-templated. Per-probe generated result and argument traits come from validated DWARF or real headers; they retain original signature types separately from borrowed read-only observation types. `Interceptor<Sig>` is a selector checked against the original recovered signature by a generated `static_assert`.

No application function body, record-layout description, result-type alias, or argument-type specialization belongs in this callback.

# Reference argument recovered from DWARF

For an existing target selected as `int process(MyClass&)`, the engine resolves `MyClass` from that callable's debug type information. Assume its complete DWARF describes an `int value` member. The user supplies only this callback, without a target header or a manual class declaration:

```cpp
template <>
class Interceptor<int(MyClass&)> {
public:
  void pre(const Call& call) {
    const auto& object = call.argument<0>(); // Borrowed const MyClass&.
    call.field("value_before", object.value);
  }

  void post(const Call& call) {
    const auto& object = call.argument<0>();
    call.field("value_after", object.value);
    const int result = call.result();
    call.field("result", result);
  }
};
```

The engine reconstructs the declaration with the correct namespace, member type, offset, size and alignment, and checks the reconstructed layout before allowing these field reads. The original target receives the same `MyClass&`; it may modify the object, while the callback receives a read-only view. If the target changes `value`, POST can observe the updated member while the object remains alive.

Forwarding `MyClass&` does not copy or destroy `MyClass` and does not require its by-value calling convention. If only the class identity is recoverable, the engine can supply an opaque forward declaration and still support the signature and scalar result. The example's `object.value` reads require a complete usable layout and must be rejected if that layout cannot be recovered. [C++ member access rules](https://eel.is/c++draft/expr.ref#5)

# Calling a recovered const method

For the same `int process(MyClass&)` target, assume matching DWARF describes an accessible nonvirtual method `int MyClass::score() const`, and the target retains a callable out-of-line implementation. The engine reconstructs its declaration and binds calls to that implementation before installing the interceptor.

The user supplies only the specialization and hooks:

```cpp
template <>
class Interceptor<int(MyClass&)> {
public:
  void pre(const Call& call) {
    const auto& object = call.argument<0>(); // Borrowed const MyClass&.
    const int score = object.score();       // Calls the target's method.
    call.field("score_before", score);
  }

  void post(const Call& call) {
    const auto& object = call.argument<0>();
    const int score = object.score();       // Observe the current object.
    call.field("score_after", score);
    const int result = call.result();
    call.field("result", result);
  }
};
```

No manual `MyClass` definition, method body, address lookup or function-pointer cast appears in the callback. The engine imports the declaration from DWARF and resolves the implementation in the target; Clang checks overload resolution, accessibility and const qualification.

## Method binding steps

1. Recover the owning class and method declaration, including parameter and result types, const and volatile qualifiers, reference qualifiers and the calling-convention information needed for the call.

2. Resolve the exact implementation using the method's recorded linkage name or concrete out-of-line debug entry. Follow declaration and definition references, account for module load addresses, and verify that the recovered declaration and code belong to the same loaded binary.

3. Register the resolved method with the JIT linker so the compiled `object.score()` call reaches existing target code with the original object as its receiver. Do not synthesize a method body or substitute a similarly named overload.

4. Validate the receiver layout and every argument and result ABI needed by that method, then require all referenced bindings to resolve before probe installation.

The first implementation should cover accessible nonvirtual const instance methods on classes admitted by the existing layout checks, using the validated scalar, pointer and reference call paths. Class-valued method arguments or results require the corresponding by-value ABI support. Virtual dispatch, inheritance-related receiver adjustments and unsupported method ABIs require separate validated support; do not replace virtual calls with unconditional direct calls.

## Method availability and execution

A recovered layout alone is insufficient: the callback needs both a method declaration and callable code. If optimization leaves only inlined instances, do not treat an inline instruction range as a standalone function. Try a matching header route that can compile an available inline definition; otherwise reject that method call with a diagnostic naming the method and missing implementation.

The callback's `const MyClass&` permits const methods through normal C++ access rules. Const does not guarantee purity: a method can change mutable or global state, acquire locks, throw, or trigger other instrumented calls. Apply the existing callback failure and reentrancy rules, keep the borrowed receiver alive throughout the call, and document that this example invokes `score()` once in PRE and once in POST.

# Standard library example

Never synthesize `std::` classes from DWARF. Use real standard library headers and matching application compiler settings. For libstdc++ dual-ABI types, check target DWARF linkage `__cxx11`/`cxx11` ABI-tag evidence against the effective `_GLIBCXX_USE_CXX11_ABI` macro; reject mismatches or unresolved required ABI evidence before injection.

For a target whose signature is `std::string(int)`:

```cpp
#include <string>

template <>
class Interceptor<std::string(int)> {
public:
  void pre(const Call& call) {
    const auto id = call.argument<0>(); // int
    call.field("id", id);
  }

  void post(const Call& call) {
    const auto& text = call.result();   // const std::string&
    call.field("length",
               static_cast<unsigned long long>(text.size()));
  }
};
```

The callback borrows the returned string during POST. Recording its length uses the existing scalar field API; recording string contents would require a separate supported string-value interface.

For the previously suggested `bool(const std::string&)` signature:

```cpp
#include <string>

template <>
class Interceptor<bool(const std::string&)> {
public:
  void pre(const Call& call) {
    const auto& text = call.argument<0>(); // const std::string&
    call.field("length",
               static_cast<unsigned long long>(text.size()));
  }

  void post(const Call& call) {
    const bool accepted = call.result();
    call.field("accepted", static_cast<int>(accepted));
  }
};
```

Here `std::string` is an argument and the result is `bool`. The same signature mechanism covers a class returned by value.

# What the signature supplies

The engine generates original result and parameter types from DWARF or real headers, then checks the signature named by `Interceptor<R(Args...)>`. Original types and observation types remain distinct: `MyClass&` can be observed as `const MyClass&` without changing the original signature checked by `static_assert`.

| Signature component | Proposed callback access |
| --- | --- |
| Scalar argument such as `int` | `call.argument<0>()` returns the scalar value |
| Reference argument such as `const T&` | `call.argument<0>()` preserves read-only reference access |
| Class argument passed by value | `call.argument<0>()` exposes a borrowed `const T&` |
| Scalar result such as `bool` | `call.result()` returns the scalar value in POST |
| Class result passed by value | `call.result()` exposes a borrowed `const R&` in POST |
| Void result | No result value is exposed |

The existing `call.field(name, value)` and supported inferred-member field form remain available. Object references must remain within their hook invocation.

# Automatic type recovery

The template argument names the types once; the engine supplies the declarations. Resolve the selected callable's types across the executable, loaded libraries and matching separate debug files, following available type references and aliases. Recover namespaces, layouts and calling-convention metadata needed by the wrapper and the callback. Validate the reconstructed Clang declarations rather than assuming that a type name proves a layout match.

Compiler settings can omit definitions or place them in another compilation unit, so a missing definition in the first debug entry is a reason to search the matching debug sources. It is not proof that the class is unavailable. [Clang debug information options](https://clang.llvm.org/docs/CommandGuide/clang.html#cmdoption-fstandalone-debug)

Require information according to the operation:

| Operation | Required recovered information |
| --- | --- |
| Forward `MyClass&` or `MyClass*` | Correct class identity and reference or pointer passing mode; an opaque declaration can suffice |
| Read `object.value` | Complete usable layout and member declarations, checked against DWARF |
| Pass or return `MyClass` by value | Layout plus sufficient C++ calling-convention evidence |
| Call `object.score()` | Validated receiver and method declaration, compatible argument and result ABIs, and an exact callable implementation bound into the JIT |

If required debug information is still missing, try matching existing headers through the configured type-input route. Do not require users to recreate the class. If neither source provides enough evidence, reject the specific operation with the missing type, field, method implementation or ABI detail; keep opaque reference or pointer forwarding available where it remains valid. A recovered method declaration is usable only when its implementation can be bound or compiled from an available matching definition.

# Compiler options and application headers

Allow each validation or injection request to carry `compile_options` as an ordered list of compiler arguments. Support application include paths, preprocessor definitions and undefinitions, forced includes and the requested C++ language standard, for example `-I/app/include`, `-DAPP_FEATURE=1`, `-include` followed by a header path, and `-std=c++23`. Pass arguments directly to the compiler without shell interpretation.

Headers and include paths refer to files on the server host. A remote client's local path is not automatically available there. An optional compilation database can supply application build settings; combine the selected entry with the engine's required settings and explicit request options, and report the effective options. Diagnose options that conflict with the target ABI or required wrapper configuration.

When a callback includes the real application header, use its declarations instead of generating a second definition of the same type from DWARF. Compare available debug layout and ABI evidence against the header definition. Continue recovering other required types from DWARF where that does not create conflicting definitions.

## Callback using an application header

Assume `/app/include/my_class.hpp` contains the existing application definition of `MyClass` and declares its accessible `int score() const` method:

```cpp
#include "my_class.hpp"

template <>
class Interceptor<int(MyClass&)> {
public:
  void post(const Call& call) {
    const auto& object = call.argument<0>();
    call.field("score", object.score());
    const int result = call.result();
    call.field("result", result);
  }
};
```

The header can provide inline method definitions for compilation. Out-of-line methods still need bindings to compatible existing target implementations.

## Proposed Python request flow

The client additions below are implemented in this worktree; use matching updated client and server versions.

```python
compile_options = [
    "-I/app/include",
    "-DAPP_FEATURE=1",
    "-std=c++23",
]

report = client.validate_probe(
    session_id,
    "process(MyClass&)",
    callback_source,
    compile_options=compile_options,
    level="compile",
)

if report.success:
    probe = client.inject(
        session_id,
        "process(MyClass&)",
        callback_source,
        compile_options=compile_options,
    )
else:
    for diagnostic in report.diagnostics:
        print(diagnostic)
```

Use `level="syntax"` for an explicit syntax-only check, or `level="compile"` for full preparation checks. Injection performs full validation itself even if the user skips the standalone validation call.

# Validation before injection

Provide a server validation operation for the same selector, callback source, headers, compilation database and compiler options accepted by injection. Return the validation level, success state, effective options and structured diagnostics with stage, severity, source file, line, column and message when available.

1. **Syntax validation:** preprocess the headers, recover required declarations, parse the signature specialization and hooks, and run Clang semantic checks with the signature-bound facade. Report missing headers and types, syntax errors, invalid member access, overload or const errors, and hook-shape errors.

2. **Compilation validation:** run syntax validation, check signature and available ABI evidence, generate the actual callback and original-call wrapper, compile them, and resolve the external methods and symbols required for JIT linking. Syntax-only success does not establish that method implementations or wrapper bindings exist.

3. **Installation:** only after full validation succeeds, recheck the selected target and module identities and perform the existing probe installation flow. A standalone validation result is not permission to reuse stale target addresses or a changed source/configuration.

Validation must not install a probe or execute hooks, application methods or static initializers in the target. If a requirement can only be checked during loading or installation, report that boundary rather than marking it as already verified. Compilation success does not prove runtime object lifetime or behavior.

Validation and injection must use the same preparation implementation and effective configuration. If compiled preparation is cached, reuse it only when the callback, generated declarations, headers, compiler options, toolchain and target module identities still match; otherwise prepare again. A failed check returns actionable diagnostics before probe installation.

# Engine changes

1. Resolve the selected target and assemble the effective compiler options and type inputs. Use included application definitions when supplied, recover remaining required declarations from matching DWARF without duplicates, and retain opaque declarations where reference or pointer forwarding needs only class identity.

2. Recognize the `Interceptor<Signature>` specialization and keep `Call` non-templated. Generate per-probe argument/result traits including original and read-only observation types; compile a `static_assert` comparing the selected signature with the recovered original signature. Preserve existing plain `Interceptor` callbacks.

3. Compare the declared signature with the selected callable's available debug information, preserving class identity, namespaces, qualifiers and reference modes. Reject known mismatches before patching the target, and reject by-value operations when required ABI evidence cannot be obtained.

4. Compile the original-call wrapper with the resolved C++ function type and the validated recovered or imported declarations. Let Clang lower register returns, memory returns and arguments consistently with the target configuration; forwarding reference arguments must preserve the original address. Recover referenced method declarations, validate their call ABIs and bind exact callable target implementations before installing the probe.

5. Expose the completed result to POST without introducing an extra object copy or destruction. Keep its lifetime valid throughout POST, including shared-context dispatch.

6. Expose server syntax-only and full-compilation validation through the same preparation path used by injection. Full validation must pass before probe installation. Keep exception propagation, callback failure handling and probe cleanup consistent with the existing interception contract.

# ABI conditions

The signature template is the public API, with automatic DWARF type recovery as the primary type-input route. DWARF locates the callable, supplies type declarations and provides available signature, layout and calling-convention evidence.

For class results, complete matching types let Clang determine the C++ return convention. Compatibility still depends on the target ABI, compilation settings and library ABI; a matching type spelling alone is insufficient.

Non-trivial result forwarding remains an implementation problem. Declaring `Interceptor<std::string(int)>` does not by itself prove that the generated wrapper preserves the caller's result storage. Guaranteed elision, optional NRVO and POST timing must be checked in generated IR and runtime tests before claiming support.

Headerless operation is part of this approach: `Interceptor<int(MyClass&)>` can use an opaque recovered declaration when forwarding is sufficient, or a validated reconstructed definition when fields are read. By-value operations need additional ABI evidence. A reference signature does not require the special-member evidence needed for a by-value signature.

# Acceptance criteria

- The user supplies a signature specialization and hooks; sufficient DWARF enables compilation without target headers, manual class declarations or ABI metadata.

- Validation and injection accept the same ordered compiler options, including application include paths and build definitions; header-provided types are not duplicated by DWARF reconstruction.

- Syntax validation reports missing headers, syntax errors, invalid member access and signature-bound hook errors with source locations.

- Full compilation validation additionally catches unsupported ABIs, wrapper code-generation failures and unresolved method or symbol bindings.

- Standalone validation installs no probe and executes no user code in the target; injection repeats or safely reuses full preparation and checks target identity before installation.

- A syntax-only success cannot authorize installation when full compilation validation fails; changed source, headers, options or target modules invalidate prepared results.

- Scalar, reference and class argument access follows the declared signature.

- `Interceptor<int(MyClass&)>` with recoverable layout records the correct member values before and after the target call, records its scalar result, and forwards the original reference without copies or destructor calls.

- A headerless `Interceptor<int(MyClass&)>` can invoke a recovered accessible nonvirtual `int score() const` through its borrowed argument and record the correct PRE and POST values from the existing target implementation.

- Method binding selects the exact overload, qualifier set and loaded module; unresolved or incompatible methods fail before installation.

- Calling a nonconst method through the read-only view fails normal const checking. A layout-only type or inline-only debug entry without callable code produces a precise method-availability diagnostic.

- Method calls follow the existing callback failure and reentrancy rules without extending the receiver's lifetime or introducing receiver copies.

- A reference signature with only class identity available still supports opaque forwarding and scalar result observation; attempting field access produces a precise missing-layout diagnostic.

- Type definitions found through matching separate debug files or referenced compilation units are usable.

- Missing required debug information falls back to matching existing headers; unresolved by-value ABI information or field layouts are rejected before installation.

- Small INTEGER, mixed INTEGER/SSE and large memory-class record returns preserve application results and expose correct POST values.

- Non-trivial results introduce no extra copies or destructor calls and preserve the caller's object.

- Result access during PRE is rejected; borrowed result access is limited to POST.

- Declared signature mismatches are diagnosed before installation wherever the available evidence permits validation.

- Existing plain Interceptor callbacks continue to work.

- The native test suite passes on the supported Linux toolchain.

# Sources

- User clarification on 3 November 2026: the function signature belongs in the Interceptor template argument.

- User follow-up on 3 November 2026: recover types automatically from DWARF, retain opaque reference forwarding where possible, and use matching headers for required gaps.

- User request on 3 November 2026: add method binding and an example calling a const method on `MyClass` recovered from DWARF.

- User requirement on 3 November 2026: accept compiler options when injecting so callbacks can include application headers, and allow the server to verify syntax and validate compilation before injection.

- Wiki proposal `pages/planning/clang-tracer-object-return-abi`, based on `main` at `0dca368`, for the existing object-return rejection and ABI constraints.

- `include/tracer/callback.hpp` at `0dca368`, for existing argument, result and field access.

- `src/agent.cpp` and `examples/cpp-interceptors/free_function.cpp` at `0dca368`, for generated argument aliases and the existing hook form.

- [C++ draft on incomplete types](https://eel.is/c++draft/basic.types.general) and [class member access](https://eel.is/c++draft/expr.ref#5), for opaque reference types and complete-type requirements for field access.

- [Clang debug information options](https://clang.llvm.org/docs/CommandGuide/clang.html#cmdoption-fstandalone-debug), for omitted, distributed and externally referenced type definitions.

- [DWARF linkage-name discussion](https://wiki.dwarfstd.org/DW_AT_linkage_name.md), for resolving recorded linker names to existing code; the method-binding steps above are proposed engine behavior.

This is an API and implementation proposal; the signature template, inferred accessors, DWARF method-binding path, per-request compiler options and standalone validation additions are not implemented.
