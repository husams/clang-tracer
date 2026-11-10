"""Exercise application-header imports and generated record conversions."""
import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("repl")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="tracer-auto-records-") as directory:
        headers = Path(directory) / "application include"
        headers.mkdir()
        (headers / "types.hpp").write_text('''#pragma once
#ifndef REPL_APP_VERSION
#error application compiler definitions are missing
#endif
#include <string>
#include <vector>
namespace application {
inline int header_runs = 0;
inline int initialize_header() { return ++header_runs; }
inline int initialized = initialize_header();
struct Address { std::string city; int postal_code = 0; static constexpr int version = REPL_APP_VERSION; };
class Order { public: Address address; std::vector<Address> history; };
template<class T> struct Box { T value{}; };
using IntBox = Box<int>;
extern "C++" { struct Linked { int number = 0; }; }
class Private {
    struct Hidden { int number = 0; };
    Private() = default;
    int secret = 1;
    Hidden hidden;
protected:
    std::string label;
};
class DeletedAssignment {
    [[maybe_unused]] int number = 0;
public:
    DeletedAssignment& operator=(const DeletedAssignment&) = delete;
};
class PrivateUnassignableField { [[maybe_unused]] DeletedAssignment field; };
struct NoDefault { explicit NoDefault(int); int number; };
struct Constant { const int number = 1; };
struct Base { int number = 1; };
struct Manual { int number = 0; };
struct HandMapped { int number = 0; };
struct Derived : Base { int extra = 2; };
union Union { int integer; double real; };
}
''')
        (headers / "bad-codec.hpp").write_text('''#pragma once
#include <set>
namespace decoder_check {
struct Good { int number = 0; };
struct Key { int number = 0; };
struct Table { std::set<Key> values; };
}
''')
        settings = f'%cpp options add "-I{headers}" "-DREPL_APP_VERSION=7"\n'
        code = '''using tracer_repl::EventValue;
EventValue address_value{EventValue::Object{{"city",EventValue{std::string("Paris")}}, {"postal_code",EventValue{std::int64_t{75001}}}}, "application::Address"};
EventValue order_value{EventValue::Object{{"address",address_value},{"history",EventValue{EventValue::Array{address_value}}}}, "application::Order"};
auto order = order_value.as<application::Order>();
if(order.address.city!="Paris" || order.history.at(0).postal_code!=75001 || application::Address::version!=7) throw std::runtime_error("generated record conversion failed");
std::cout << "automatic-nested-records-ok" << std::endl;
'''

        def run(commands, fail_fast=False, compiler_arguments=()):
            command = [args.repl, "--endpoint", "127.0.0.1:1"]
            for argument in compiler_arguments:
                command.extend(("--cpp-arg", argument))
            if fail_fast:
                command.append("--fail-fast")
            return subprocess.run(command, input=commands + "%quit\n", text=True,
                                  capture_output=True, timeout=150)

        good = run('%help cpp\n%complete "tracer.pr"\n' + settings + '%cpp options\n%cpp import "types.hpp"\n' + code + '''%complete "order.address."
%cpp import "types.hpp"
EventValue box_value{EventValue::Object{{"value",EventValue{std::int64_t{42}}}}, "application::Box<int>"};
EventValue linked_value{EventValue::Object{{"number",EventValue{std::int64_t{24}}}}, "application::Linked"};
if(linked_value.as<application::Linked>().number!=24) throw std::runtime_error("linkage record missing");
if(box_value.as<application::IntBox>().value!=42 || application::header_runs!=1) throw std::runtime_error("template/reimport failure");
std::cout << "automatic-template-and-once-ok" << std::endl;
''', True)
        assert good.returncode == 0, (good.stdout, good.stderr)
        for marker in ("automatic-nested-records-ok", "automatic-template-and-once-ok"):
            assert marker in good.stdout, (good.stdout, good.stderr)
        assert "application::Private" in good.stdout, good.stdout
        assert "skipped application::PrivateUnassignableField" in good.stdout, good.stdout
        assert "city" in good.stdout, good.stdout

        rejected = run(settings + '%cpp import "types.hpp"\n' + code + '''%cpp import "types.hpp" --type application::Derived
%cpp import "types.hpp" --type application::NoDefault
%cpp import "types.hpp" --type application::Constant
%cpp import "types.hpp" --type application::Union
%cpp import "missing-header.hpp"
%cpp options add "-DREPL_APP_VERSION=99"
if(order.address.city!="Paris" || application::Address::version!=7 || application::header_runs!=1) throw std::runtime_error("failed import changed live state");
std::cout << "failed-import-state-retained" << std::endl;
''')
        assert rejected.returncode != 0, (rejected.stdout, rejected.stderr)
        assert "failed-import-state-retained" in rejected.stdout, (rejected.stdout, rejected.stderr)
        assert "options are locked" in rejected.stderr, rejected.stderr
        private_mapping = Path(directory) / "private-mapping.hpp"
        private = run(settings + f'''%cpp generate "types.hpp" --type application::Private --output "{private_mapping}"
#include "{private_mapping}"
''' + '''using tracer_repl::EventValue;
EventValue hidden{EventValue::Object{{"number", EventValue{std::int64_t{17}}}}, "application::Private::Hidden"};
EventValue private_value{EventValue::Object{{"secret", EventValue{std::int64_t{42}}}, {"hidden", hidden}, {"label", EventValue{std::string("owned label")}}}, "application::Private"};
auto private_record = private_value.as<application::Private>();
if (private_record.secret != 42 || private_record.hidden.number != 17 || private_record.label != "owned label") throw std::runtime_error("private/protected field decoding failed");
%complete "private_record."
std::cout << "automatic-private-record-ok" << std::endl;
''', True, ("-faccess-control",))
        assert private.returncode == 0, (private.stdout, private.stderr)
        assert "automatic-private-record-ok" in private.stdout, private.stdout
        completions = {line.strip() for line in private.stdout.splitlines()}
        assert {"hidden", "label", "secret"} <= completions, private.stdout

        selected = run(settings + '''%cpp import "types.hpp" --type application::Order --type application::Linked
''' + code, True)
        assert selected.returncode == 0, (selected.stdout, selected.stderr)
        assert "automatic-nested-records-ok" in selected.stdout
        manual = run(settings + '''#include "types.hpp"
template<> struct tracer_repl::EventCodec<application::Manual> { static application::Manual decode(const tracer_repl::EventValue& value) { return {value.field("wire").as<int>() + 100}; } };
template<> struct tracer_repl::EventRecord<application::HandMapped> { static constexpr const char* type_name="application::HandMapped"; static auto fields() { return std::make_tuple(tracer_repl::event_member("wire", &application::HandMapped::number)); } };
%cpp import "types.hpp"
tracer_repl::EventValue custom{tracer_repl::EventValue::Object{{"wire",tracer_repl::EventValue{std::int64_t{23}}}}, "application::HandMapped"};
if(custom.as<application::HandMapped>().number!=23 || custom.as<application::Manual>().number!=123 || application::header_runs!=1) throw std::runtime_error("manual codecs were not preserved");
std::cout << "manual-codecs-preserved" << std::endl;
''', True)
        assert manual.returncode == 0, (manual.stdout, manual.stderr)
        assert "manual-codecs-preserved" in manual.stdout

        invalid = run(settings + '''%cpp options add "-invalid-tracer-compiler-option"
%cpp options add "-std=not-a-cpp-standard"
%cpp options add "-I"
%cpp options add "-fsyntax-only"
%cpp options add "-E"
%cpp import "types.hpp" --type application::Address
if(application::Address::version!=7) throw std::runtime_error("invalid options persisted");
std::cout << "invalid-options-retained-profile" << std::endl;
''')
        assert invalid.returncode != 0
        assert "invalid-options-retained-profile" in invalid.stdout, (invalid.stdout, invalid.stderr)
        implicit = run(settings + '''#include "types.hpp"
static_assert(sizeof(tracer_repl::EventCodec<application::Address>)>0);
%cpp import "types.hpp" --type application::Address
std::cout << "implicit-codec-protected" << std::endl;
''')
        assert implicit.returncode != 0
        assert "already instantiated" in implicit.stderr, (implicit.stdout, implicit.stderr)
        assert "implicit-codec-protected" in implicit.stdout
        decoder = run(settings + '''int retained = 19;
%cpp import "bad-codec.hpp"
if(retained!=19) throw std::runtime_error("decoder error lost state");
%cpp import "bad-codec.hpp" --type decoder_check::Good
tracer_repl::EventValue good_value{tracer_repl::EventValue::Object{{"number",tracer_repl::EventValue{std::int64_t{23}}}}, "decoder_check::Good"};
if(good_value.as<decoder_check::Good>().number!=23) throw std::runtime_error("selected decoder failed");
std::cout << "decoder-error-state-and-selection-ok" << std::endl;
''')
        assert decoder.returncode != 0
        assert "decoder-error-state-and-selection-ok" in decoder.stdout, (decoder.stdout, decoder.stderr)
        assert "--type" in decoder.stderr, decoder.stderr
        startup = run('''%cpp import "types.hpp" --type application::Order
''' + code, True, (f"-I{headers}", "-DREPL_APP_VERSION=7", "-Wall", "-Wextra"))
        assert startup.returncode == 0, (startup.stdout, startup.stderr)
        assert "automatic-nested-records-ok" in startup.stdout
        print("automatic header mappings, compiler options, templates and recovery passed")


if __name__ == "__main__":
    main()
