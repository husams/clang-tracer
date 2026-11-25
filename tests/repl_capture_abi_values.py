"""Automatic capture of unions, long double, __int128, wide enumerations, member pointers,
std::nullptr_t and volatile scalars (issue #181).

Each case is one record passed by const reference to `int abi::f_<case>(const T&)` of a shared
library, which a small program calls in a loop. Every case runs on both capture routes:

  direct    %probe inject "abi::f_<case>" --callback ... --include abi.hpp
  prepared  %probe prepare "abi::f_<case>(const abi::T &)" --include abi.hpp --output DIR
            %probe inject ... --prepared DIR --prepared-mode require

Checked for each: the probe installs, the snapshot is complete, and the decoded values are the
ones the program wrote, through the generic reader (EventValue fields), through the typed
conversions (as<__int128>(), as<long double>(), as<std::nullptr_t>(), as<int abi::Key::*>())
and through the record conversions `%cpp import` generates (argument_as<abi::Int128>()).

  * A union is its object representation, its size, the members it could hold and an active
    member that is reported as unknown; the members beside it stay readable. Its int is decoded
    only where the record has a tag that says the int is active.
  * A long double keeps the bits a double cannot: a value one ulp from 1, a negative zero, an
    infinity and a NaN.
  * A 128-bit integer keeps all its bits, signed and unsigned, including the top one.
  * A wide enumeration keeps its exact value: above INT64_MAX, unnamed, INT64_MIN, UINT32_MAX.
  * A member pointer is the parts its ABI gives it: null state, offset, function address or
    vtable offset, adjustment of `this`.
  * A volatile scalar is captured; volatile class storage and std::mutex are reported as
    unsupported with the reason, and their probes still install.

The fixture is built by the compiler given with --compiler. With a GCC it is the toolchain the
issue was reported on; the test is skipped (exit 77) when --require-gcc is given and it is not one.
"""
import argparse
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import tempfile

SKIP = 77

HEADER = r"""#pragma once
#include <cstddef>
#include <mutex>
#include <optional>
#include <pthread.h>
#include <vector>

namespace abi {
struct Key { int a = 1; int b = 2; };
struct Left { int l = 1; virtual ~Left() = default; virtual int left(int n) { return n + l; } };
struct Right {
  int r = 2;
  virtual ~Right() = default;
  virtual int right(int n) { return n + r; }
  int plain(int n) { return n * r; }
};
struct Both : Left, Right { int own = 3; int direct(int n) { return n + own; } };

union Number { int i; float f; };
union Mixed { struct { short low, high; }; int whole; };
enum class Kind : int { integer = 0, real = 1 };
enum Small { one = 1, two = 2 };
enum class Big : unsigned long long { small = 1, also_small = 1, large = 1ull << 40 };
enum class Huge : unsigned long long { top = ~0ull };
enum class Low : long long { low = -9223372036854775807LL - 1, mid = 5 };
enum class Top32 : unsigned { top = 0xffffffffu };
// Every enumerator fits a 32-bit number; the object does not have to.
enum class Wide64 : long { a = 1, b = 2 };

struct Plain { int a = 1; double d = 2.5; Small e = two; };
struct UnionMember { int tag = 7; Number n{5}; int after = 9; };
struct AnonUnion { int tag = 7; union { int i = 5; float f; }; int after = 9; };
struct UnnamedUnion {
  union { long l; char c[8]; } u{0x0102030405060708L};
  union { short s; char pad[2]; } second{3}, third{4};
  int after = 9;
};
struct NestedUnion { Mixed m; int after = 9; NestedUnion() { m.whole = 0x00020001; } };
struct RawMutex { pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER; int after = 9; };
struct StdMutex { std::mutex lock; int after = 9; };
struct Tagged { Kind kind = Kind::integer; Number n{5}; };
struct LongDouble {
  long double x = 1.5L;
  long double precise = 1.0L + 0x1p-60L;
  long double negative_zero = -0.0L;
  long double infinity = __builtin_infl();
  long double nan = __builtin_nanl("");
};
struct Int128 {
  __int128 small = 5;
  __int128 big = ((__int128)1 << 100) + 5;
  __int128 negative = -(((__int128)1 << 100) + 5);
  unsigned __int128 top = ~(unsigned __int128)0 - 1;
};
struct BigEnum {
  Big e = Big::large;
  Big unnamed = static_cast<Big>((1ull << 63) + 3);
  Huge h = Huge::top;
  Low low = Low::low;
  Top32 t = Top32::top;
  Wide64 w = static_cast<Wide64>(1L << 40);
  Small s = two;
};
struct MemberPointer {
  int Key::*p = &Key::b;
  int Key::*none = nullptr;
  int (Both::*direct)(int) = &Both::direct;
  int (Both::*adjusted)(int) = &Right::plain;
  int (Both::*virt)(int) = &Right::right;
  int (Both::*no_function)(int) = nullptr;
};
struct DataPointers { int Key::*p = &Key::b; int Key::*none = nullptr; };
struct NullPtr { std::nullptr_t n = nullptr; int after = 4; };
struct VolatileMember {
  volatile int x = 5;
  volatile double d = 2.5;
  volatile Small e = two;
  volatile long double wide = 1.5L;
  int after = 9;
};
struct VolatileClass { volatile Plain p; int after = 9; };
struct Containers {
  std::vector<Number> numbers{Number{5}, Number{6}};
  std::optional<long double> maybe = 2.5L;
  std::vector<__int128> wide{((__int128)1 << 100) + 5};
  std::vector<Big> enums{Big::large};
};
// Members the direct route cannot name or does not capture: private ones, built-in arrays
// and pointers.
class Hidden {
  Number n{5};
  __int128 w = 7;
  long double x = 2.5L;
  int Key::*p = &Key::b;
  std::nullptr_t z = nullptr;
  volatile int v = 3;
  Big e = Big::large;
  union { int i = 6; float f; };

public:
  int tag = 1;
};
struct VolatileMore {
  volatile int values[3] = {1, 2, 3};
  int *volatile pointer = nullptr;
  Number numbers[2] = {Number{7}, Number{8}};
  int Key::*members[2] = {&Key::a, &Key::b};
  union { int i; float f; } pairs[2] = {{9}, {10}};
};

#define ABI_CASES(X)                                                                       \
  X(plain, Plain) X(union_member, UnionMember) X(anon_union, AnonUnion)                    \
  X(unnamed_union, UnnamedUnion) X(nested_union, NestedUnion) X(raw_mutex, RawMutex)       \
  X(std_mutex, StdMutex) X(tagged, Tagged) X(long_double, LongDouble) X(int128, Int128)    \
  X(big_enum, BigEnum) X(member_pointer, MemberPointer) X(data_pointers, DataPointers)     \
  X(null_ptr, NullPtr)                                                                     \
  X(volatile_member, VolatileMember) X(volatile_class, VolatileClass)                      \
  X(containers, Containers) X(hidden, Hidden) X(volatile_more, VolatileMore)
#define ABI_DECLARE(name, Type) int f_##name(const Type &value);
ABI_CASES(ABI_DECLARE)
#undef ABI_DECLARE
}
"""

LIBRARY = r"""#include "abi.hpp"
namespace abi {
__attribute__((noinline)) int touch(const void *p) { return p != nullptr; }
#define ABI_DEFINE(name, Type) \
  __attribute__((noinline)) int f_##name(const Type &value) { return touch(&value); }
ABI_CASES(ABI_DEFINE)
}
"""

PROGRAM = r"""#include "abi.hpp"
#include <iostream>
#include <unistd.h>
int main() {
  std::cout << "ready" << std::endl;
  for (;;) {
#define ABI_CALL(name, Type) { abi::Type value; if (abi::f_##name(value) != 1) return 1; }
    ABI_CASES(ABI_CALL)
    usleep(100000);
  }
}
"""

CALLBACK = "struct Interceptor {\n  void pre(const Call &) {}\n  void post(const Call &) {}\n};\n"

# Helpers the checks share. `le` reads a little-endian integer out of captured bytes.
PRELUDE = r"""#include <cmath>
#include <stdexcept>
#include <string>
auto need = [](bool ok, const char* what) { if (!ok) throw std::runtime_error(what); };
auto le = [](const std::string& bytes, std::size_t offset, std::size_t count) { unsigned long long value = 0; for (std::size_t i = 0; i < count; ++i) value |= static_cast<unsigned long long>(static_cast<unsigned char>(bytes.at(offset + i))) << (8 * i); return value; };
auto text = [](const tracer_repl::EventValue& value) { return value.as<std::string>(); };
auto is_union = [](const tracer_repl::EventValue& value, const char* type, std::size_t size) { need(value.cpp_type() == type, "union type"); need(value.field("size").as<unsigned long long>() == size, "union size"); need(value.field("bytes").as<std::string>().size() == size, "union bytes"); need(value.field("active_member").as<std::string>() == "unknown", "union active member"); };
auto unavailable = [](const tracer_repl::Event& event, const char* reason) { try { (void)event.argument_snapshot(0); } catch (const std::exception& error) { if (std::string(error.what()).find(reason) != std::string::npos) return; throw std::runtime_error(std::string("unexpected reason: ") + error.what()); } throw std::runtime_error("the snapshot was expected to be unavailable"); };
"""

BIG = "(((__int128)1 << 100) + 5)"


def checks(machine):
    """{case: C++ statements over `e`, the POST event}. Statements throw on a wrong value."""
    x86 = machine in ("x86_64", "AMD64")
    long_double_format, long_double_size = (("x87-extended-80", 10) if x86
                                            else ("ieee-binary128", 16))
    member_abi = "itanium" if x86 else "itanium-arm"
    return {
        "plain": 'const auto a = e.argument_snapshot(0); need(a.field("a").as<int>() == 1 && '
                 'a.field("d").as<double>() == 2.5 && a.field("e").as<int>() == 2, "plain"); '
                 'const auto v = e.argument_as<abi::Plain>(0); '
                 'need(v.a == 1 && v.d == 2.5 && v.e == abi::two, "typed plain");',
        "union_member":
            'const auto a = e.argument_snapshot(0); const auto n = a.field("n"); '
            'need(a.field("tag").as<int>() == 7 && a.field("after").as<int>() == 9, "siblings"); '
            'is_union(n, "abi::Number", 4); '
            'need(text(n.field("alternatives")) == "int i; float f", "alternatives"); '
            'need(le(text(n.field("bytes")), 0, 4) == 5, "representation");',
        "anon_union":
            'const auto a = e.argument_snapshot(0); const auto n = a.field("__anonymous_union_0"); '
            'need(a.field("tag").as<int>() == 7 && a.field("after").as<int>() == 9, "siblings"); '
            'is_union(n, "abi::AnonUnion::(anonymous union)", 4); '
            'need(text(n.field("alternatives")) == "int i; float f", "alternatives"); '
            'need(le(text(n.field("bytes")), 0, 4) == 5, "representation");',
        "unnamed_union":
            'const auto a = e.argument_snapshot(0); const auto u = a.field("u"); '
            'is_union(u, "abi::UnnamedUnion::(unnamed union)", 8); '
            'need(text(u.field("alternatives")) == "long l; char[8] c", "alternatives"); '
            'need(le(text(u.field("bytes")), 0, 8) == 0x0102030405060708ULL, "representation"); '
            'need(a.field("second").field("size").as<int>() == 2 && '
            'le(text(a.field("second").field("bytes")), 0, 2) == 3, "second union"); '
            'need(le(text(a.field("third").field("bytes")), 0, 2) == 4 && '
            'a.field("third").cpp_type() == a.field("second").cpp_type(), "shared union type"); '
            'need(a.field("after").as<int>() == 9, "sibling");',
        "nested_union":
            'const auto a = e.argument_snapshot(0); const auto m = a.field("m"); '
            'is_union(m, "abi::Mixed", 4); '
            'need(text(m.field("alternatives")) == "abi::Mixed::(anonymous struct); int whole", '
            '"alternatives"); '
            'need(le(text(m.field("bytes")), 0, 4) == 0x00020001 && a.field("after").as<int>() == 9, '
            '"representation");',
        "raw_mutex":
            'const auto a = e.argument_snapshot(0); const auto m = a.field("m"); '
            f'is_union(m, "pthread_mutex_t", {"40" if x86 else "48"}); '
            'need(text(m.field("alternatives")).find("__data; ") != std::string::npos, '
            '"alternatives"); '
            'need(a.field("after").as<int>() == 9, "sibling");',
        "std_mutex": 'unavailable(e, "unsupported standard type: std::mutex");',
        # The record's own tag says which member is active; nothing else may.
        "tagged":
            'const auto a = e.argument_snapshot(0); need(a.field("kind").as<int>() == 0, "tag"); '
            'need(static_cast<int>(le(text(a.field("n").field("bytes")), 0, 4)) == 5, "active int");',
        "long_double":
            'const auto a = e.argument_snapshot(0); const auto x = a.field("x"); '
            f'need(x.cpp_type() == "long double" && text(x.field("format")) == "{long_double_format}" '
            f'&& text(x.field("bytes")).size() == {long_double_size}, "format"); '
            'need(x.as<long double>() == 1.5L && x.as<double>() == 1.5, "value"); '
            'const auto precise = a.field("precise").as<long double>(); '
            'need(precise == 1.0L + 0x1p-60L && precise != 1.0L, "precision"); '
            'need(a.field("precise").as<double>() == 1.0, "double approximation"); '
            'const auto zero = a.field("negative_zero").as<long double>(); '
            'need(zero == 0 && std::signbit(zero), "negative zero"); '
            'need(std::isinf(a.field("infinity").as<long double>()) && '
            'std::isnan(a.field("nan").as<long double>()), "infinity and NaN"); '
            'const auto v = e.argument_as<abi::LongDouble>(0); '
            'need(v.x == 1.5L && v.precise == 1.0L + 0x1p-60L && v.negative_zero == 0 && '
            'std::signbit(v.negative_zero) && std::isinf(v.infinity) && std::isnan(v.nan), '
            '"typed long double");',
        "int128":
            'const auto a = e.argument_snapshot(0); '
            'need(a.field("small").cpp_type() == "__int128" && '
            'a.field("top").cpp_type() == "unsigned __int128", "types"); '
            f'need(a.field("small").as<__int128>() == 5 && a.field("big").as<__int128>() == {BIG} && '
            f'a.field("negative").as<__int128>() == -{BIG}, "signed"); '
            'need(a.field("big").field("high").as<long long>() == (1LL << 36) && '
            'a.field("big").field("low").as<unsigned long long>() == 5, "halves"); '
            'need(a.field("negative").field("high").as<long long>() < 0, "sign"); '
            'need(a.field("top").as<unsigned __int128>() == ~(unsigned __int128)0 - 1 && '
            'a.field("top").field("high").as<unsigned long long>() == ~0ULL, "unsigned"); '
            'bool refused = false; try { (void)a.field("top").as<__int128>(); } '
            'catch (const std::out_of_range&) { refused = true; } need(refused, "range"); '
            'const auto v = e.argument_as<abi::Int128>(0); '
            f'need(v.small == 5 && v.big == {BIG} && v.negative == -{BIG} && '
            'v.top == ~(unsigned __int128)0 - 1, "typed 128-bit integers");',
        "big_enum":
            'const auto a = e.argument_snapshot(0); '
            'need(a.field("e").as<unsigned long long>() == (1ULL << 40) && '
            'a.field("e").cpp_type() == "abi::Big", "large"); '
            'need(a.field("unnamed").as<unsigned long long>() == (1ULL << 63) + 3, "unnamed"); '
            'need(a.field("h").as<unsigned long long>() == ~0ULL && '
            'a.field("h").cpp_type() == "abi::Huge", "above INT64_MAX"); '
            'need(a.field("low").as<long long>() == -9223372036854775807LL - 1 && '
            'a.field("low").cpp_type() == "abi::Low", "INT64_MIN"); '
            'need(a.field("t").as<unsigned>() == 0xffffffffu && '
            'a.field("t").cpp_type() == "abi::Top32", "UINT32_MAX"); '
            'need(a.field("w").as<long long>() == (1LL << 40), "wide object"); '
            'need(a.field("s").as<int>() == 2, "narrow enum"); '
            'const auto v = e.argument_as<abi::BigEnum>(0); '
            'need(v.e == abi::Big::large && v.unnamed == static_cast<abi::Big>((1ULL << 63) + 3) && '
            'v.h == abi::Huge::top && v.low == abi::Low::low && v.t == abi::Top32::top && '
            'v.w == static_cast<abi::Wide64>(1L << 40) && v.s == abi::two, "typed enumerations");',
        "member_pointer":
            'const auto a = e.argument_snapshot(0); const auto p = a.field("p"); '
            f'need(p.cpp_type() == "int abi::Key::*" && text(p.field("abi")) == "itanium" && '
            '!p.field("null").as<bool>() && p.field("offset").as<long long>() == 4, "data member"); '
            'need(a.field("none").field("null").as<bool>() && '
            'a.field("none").field("offset").as<long long>() == -1, "null data member"); '
            'const auto direct = a.field("direct"); '
            f'need(direct.cpp_type() == "int (abi::Both::*)(int)" && text(direct.field("abi")) == '
            f'"{member_abi}" && !direct.field("null").as<bool>() && '
            '!direct.field("is_virtual").as<bool>() && '
            'direct.field("address").as<unsigned long long>() != 0 && '
            'direct.field("vtable_offset").as<unsigned long long>() == 0 && '
            'direct.field("this_adjustment").as<long long>() == 0, "member function"); '
            'const auto adjusted = a.field("adjusted"); '
            'need(!adjusted.field("is_virtual").as<bool>() && '
            'adjusted.field("address").as<unsigned long long>() != 0 && '
            'adjusted.field("this_adjustment").as<long long>() == 16, "base adjustment"); '
            'const auto virt = a.field("virt"); '
            'need(virt.field("is_virtual").as<bool>() && !virt.field("null").as<bool>() && '
            'virt.field("address").as<unsigned long long>() == 0 && '
            'virt.field("vtable_offset").as<unsigned long long>() == 16 && '
            'virt.field("this_adjustment").as<long long>() == 16, "virtual member function"); '
            'need(a.field("no_function").field("null").as<bool>() && '
            '!a.field("no_function").field("is_virtual").as<bool>(), "null member function"); '
            'need(p.as<int abi::Key::*>() == &abi::Key::b && '
            'a.field("none").as<int abi::Key::*>() == nullptr && '
            'a.field("no_function").as<int (abi::Both::*)(int)>() == nullptr, "typed"); '
            'bool refused = false; try { (void)direct.as<int (abi::Both::*)(int)>(); } '
            'catch (const std::invalid_argument&) { refused = true; } need(refused, "target code");',
        "data_pointers":
            'const auto a = e.argument_snapshot(0); '
            'need(a.field("p").field("offset").as<long long>() == 4 && '
            'a.field("none").field("null").as<bool>(), "data members"); '
            'const auto v = e.argument_as<abi::DataPointers>(0); '
            'need(v.p == &abi::Key::b && v.none == nullptr, "typed data members");',
        "null_ptr":
            'const auto a = e.argument_snapshot(0); const auto n = a.field("n"); '
            'need(n.cpp_type() == "std::nullptr_t" && n.field("null").as<bool>() && '
            'n.as<std::nullptr_t>() == nullptr && a.field("after").as<int>() == 4, "nullptr"); '
            'const auto v = e.argument_as<abi::NullPtr>(0); '
            'need(v.n == nullptr && v.after == 4, "typed nullptr");',
        "volatile_member":
            'const auto a = e.argument_snapshot(0); '
            'need(a.field("x").as<int>() == 5 && a.field("d").as<double>() == 2.5 && '
            'a.field("e").as<int>() == 2 && a.field("wide").as<long double>() == 1.5L && '
            'a.field("after").as<int>() == 9, "volatile"); '
            'const auto v = e.argument_as<abi::VolatileMember>(0); '
            'need(v.x == 5 && v.d == 2.5 && v.e == abi::two && v.wide == 1.5L && v.after == 9, '
            '"typed volatile");',
        "volatile_class": 'unavailable(e, "volatile class storage cannot be captured");',
        "containers":
            'const auto a = e.argument_snapshot(0); const auto numbers = a.field("numbers"); '
            'need(numbers.size() == 2 && le(text(numbers.at(1).field("bytes")), 0, 4) == 6 && '
            'text(numbers.at(0).field("alternatives")) == "int i; float f" && '
            'text(numbers.at(0).field("active_member")) == "unknown", "unions"); '
            'need(a.field("maybe").as<long double>() == 2.5L, "optional long double"); '
            f'need(a.field("wide").size() == 1 && a.field("wide").at(0).as<__int128>() == {BIG}, '
            '"wide integers"); '
            'need(a.field("enums").at(0).as<unsigned long long>() == (1ULL << 40), "enumerations");',
        "hidden":
            'const auto a = e.argument_snapshot(0); is_union(a.field("n"), "abi::Number", 4); '
            'need(a.field("w").as<__int128>() == 7 && a.field("x").as<long double>() == 2.5L && '
            'a.field("p").field("offset").as<long long>() == 4 && '
            'a.field("z").field("null").as<bool>() && a.field("v").as<int>() == 3 && '
            'a.field("e").as<unsigned long long>() == (1ULL << 40) && '
            'le(text(a.field("__anonymous_union_0").field("bytes")), 0, 4) == 6 && '
            'a.field("tag").as<int>() == 1, "private members");',
        "volatile_more":
            'const auto a = e.argument_snapshot(0); const auto values = a.field("values"); '
            'need(values.size() == 3 && values.at(0).as<int>() == 1 && values.at(2).as<int>() == 3, '
            '"volatile array"); need(a.field("pointer").as<unsigned long long>() == 0, "pointer"); '
            'need(a.field("numbers").size() == 2 && '
            'le(text(a.field("numbers").at(1).field("bytes")), 0, 4) == 8, "union array"); '
            'need(a.field("members").at(1).field("offset").as<long long>() == 4 && '
            'a.field("members").at(0).field("offset").as<long long>() == 0, "member pointer array"); '
            'need(a.field("pairs").size() == 2 && '
            'le(text(a.field("pairs").at(1).field("bytes")), 0, 4) == 10 && '
            'a.field("pairs").at(0).field("size").as<int>() == 4, "unnamed union array");',
    }


TYPES = {
    "plain": "Plain", "union_member": "UnionMember", "anon_union": "AnonUnion",
    "unnamed_union": "UnnamedUnion", "nested_union": "NestedUnion", "raw_mutex": "RawMutex",
    "std_mutex": "StdMutex", "tagged": "Tagged", "long_double": "LongDouble", "int128": "Int128",
    "big_enum": "BigEnum", "member_pointer": "MemberPointer", "data_pointers": "DataPointers",
    "null_ptr": "NullPtr",
    "volatile_member": "VolatileMember", "volatile_class": "VolatileClass",
    "containers": "Containers", "hidden": "Hidden", "volatile_more": "VolatileMore",
}
# The direct route reads public members only and has no built-in arrays or pointers (#178).
PREPARED_ONLY = ("hidden", "volatile_more")
# Records the REPL maps to local values: `%cpp import` writes their conversions, which
# Event::argument_as<T>() then uses.
MAPPED = ("Plain", "LongDouble", "Int128", "BigEnum", "DataPointers", "NullPtr", "VolatileMember")
# Records that have no local value, and why.
UNMAPPED = {
    "UnionMember": "union snapshots require an active member discriminator",
    "MemberPointer": "a member function pointer of the target has no local value",
}


def main():
    parser = argparse.ArgumentParser()
    for name in ("repl", "server", "agent", "compiler"):
        parser.add_argument("--" + name, required=True)
    parser.add_argument("--require-gcc", action="store_true")
    parser.add_argument("--keep", help="directory to keep the fixture and the transcripts in")
    args = parser.parse_args()
    if args.require_gcc:
        if not args.compiler or not os.access(args.compiler, os.X_OK):
            return SKIP
        version = subprocess.run([args.compiler, "--version"], capture_output=True, text=True)
        if "Free Software Foundation" not in version.stdout:
            return SKIP
    env = dict(os.environ)
    env.pop("CLANG_TRACER_ENDPOINT", None)
    env["TRACER_PREPARE_EXECUTABLE"] = str(
        Path(args.repl).with_name("clang-tracer-prepare-injectors-native"))
    expected = checks(platform.machine())
    assert set(expected) == set(TYPES)

    with tempfile.TemporaryDirectory(prefix="tracer-abi-values-") as path:
        root = Path(args.keep) if args.keep else Path(path)
        root.mkdir(parents=True, exist_ok=True)
        # The REPL keeps its history and caches in HOME.
        env["HOME"] = str(root / "home")
        (root / "home").mkdir(exist_ok=True)
        (root / "abi.hpp").write_text(HEADER)
        (root / "library.cpp").write_text(LIBRARY)
        (root / "program.cpp").write_text(PROGRAM)
        (root / "interceptor.cpp").write_text(CALLBACK)
        flags = ["-std=gnu++17", "-g", "-O0", "-Wno-pedantic"]
        for command in (
                [args.compiler, *flags, "-fPIC", "-shared", "library.cpp", "-o", "libabi.so"],
                [args.compiler, *flags, "program.cpp", "-L.", "-labi", "-pthread",
                 "-Wl,-rpath,$ORIGIN", "-o", "program"]):
            built = subprocess.run(command, cwd=root, capture_output=True, text=True)
            assert built.returncode == 0, built.stdout + built.stderr
        # %probe prepare reads the flags of the library from the directory it runs in.
        (root / "compile_commands.json").write_text(json.dumps([{
            "directory": str(root), "file": "library.cpp",
            "arguments": [args.compiler, *flags, "-fPIC", "-c", "library.cpp", "-o",
                          "library.o"]}]))

        header, callback = root / "abi.hpp", root / "interceptor.cpp"
        (root / "packages").mkdir(exist_ok=True)
        for route in ("direct", "prepared"):
            cases = [name for name in TYPES if route == "prepared" or name not in PREPARED_ONLY]
            script = (f'%cpp import "{header}" '
                      + " ".join(f"--type abi::{name}" for name in MAPPED) + "\n" + PRELUDE)
            for name in cases:
                selector = f"abi::f_{name}"
                if route == "prepared":
                    selector += f"(const abi::{TYPES[name]} &)"
                    package = root / "packages" / name
                    script += (f'%probe prepare "{selector}" --include {header} --output {package}\n'
                               f'%probe inject "{selector}" --callback {callback} '
                               f"--prepared {package} --prepared-mode require\n")
                else:
                    script += (f'%probe inject "{selector}" --callback {callback} '
                               f"--include {header}\n")
                # An event carries the demangled name on the direct route and the selector of
                # the package on the prepared one.
                script += (
                    f'%collect --name "abi::f_{name}(const abi::{TYPES[name]}&)" '
                    f'--name "abi::f_{name}(const abi::{TYPES[name]} &)" --phase POST --count 1 '
                    f"--timeout 40 --as got_{name}\n"
                    f'{{ auto got = tracer.batch("got_{name}"); '
                    f'if (got.size() < 1) throw std::runtime_error("no event: {name}"); '
                    f"const auto& e = got.at(0); {expected[name]} "
                    f'std::cout << "CASE {name} VALUES_OK\\n"; }}\n')
            with subprocess.Popen([root / "program"], cwd=root, stdout=subprocess.PIPE,
                                  text=True) as target:
                try:
                    assert target.stdout and target.stdout.readline().strip() == "ready"
                    result = subprocess.run(
                        [args.repl, "--fail-fast", "--server", args.server, "--agent", args.agent,
                         "--attach", str(target.pid), "--timeout", "120"],
                        input=script + "%detach\n%quit\n", cwd=root, env=env,
                        capture_output=True, text=True, timeout=1500)
                    transcript = result.stdout + result.stderr
                    (root / f"{route}.script").write_text(script)
                    (root / f"{route}.log").write_text(transcript)
                    assert result.returncode == 0, f"{route} route:\n{transcript[-6000:]}"
                    for name in cases:
                        assert f"CASE {name} VALUES_OK" in transcript, (route, name,
                                                                         transcript[-6000:])
                    assert target.poll() is None, f"the program stopped on the {route} route"
                finally:
                    if target.poll() is None:
                        target.terminate()
                        target.wait(timeout=20)

        # A union and a member function pointer have no local value: the import says why.
        for name, reason in UNMAPPED.items():
            refused = subprocess.run(
                [args.repl, "--fail-fast"], cwd=root, env=env, capture_output=True, text=True,
                input=f'%cpp import "{header}" --type abi::{name}\n%quit\n', timeout=300)
            assert refused.returncode and reason in refused.stdout + refused.stderr, \
                (name, refused.stdout + refused.stderr)

        # What the prepared adapter declares for the types the issue names.
        def adapter(name):
            return (root / "packages" / name / "generated" / "adapter.hpp").read_text()

        assert "union alignas(4) {\nint i;\nfloat f;\n};" in adapter("anon_union"), \
            adapter("anon_union")
        assert "union alignas(4) Number {" in adapter("union_member"), adapter("union_member")
        # Two members declared with one unnamed union keep one type.
        assert "union alignas(2) {\nshort s;\nchar pad[2];\n} second, third;" in \
            adapter("unnamed_union"), adapter("unnamed_union")
        assert "int ::abi::Key::* p;" in adapter("member_pointer"), adapter("member_pointer")
        assert "std::nullptr_t n;" in adapter("null_ptr"), adapter("null_ptr")
    return 0


if __name__ == "__main__":
    sys.exit(main())
