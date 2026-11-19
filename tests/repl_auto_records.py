"""Exercise application-header imports and generated record conversions."""
import argparse
from pathlib import Path
import re
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
#include <memory>
#include <mutex>
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
struct Base { int number = 1; protected: int inherited = 0; };
struct Manual { int number = 0; };
struct HandMapped { int number = 0; };
struct Derived : private Base { int number = 2; };
struct VirtualBase { virtual ~VirtualBase() = default; int shared = 0; };
struct Left : virtual VirtualBase { int left = 0; };
struct Right : virtual VirtualBase { int right = 0; };
struct Polymorphic : Left, Right { int leaf = 0; };
struct AbstractBase { virtual ~AbstractBase() = default; virtual void action() = 0; int base = 0; };
struct Concrete : AbstractBase { void action() override {} int value = 0; };
struct UndefinedVirtual { virtual void action(); int value = 0; };
struct RepeatedLeft : Base {};
struct RepeatedRight : Base {};
struct Repeated : RepeatedLeft, RepeatedRight {};
struct ConstructorBase {
    explicit ConstructorBase(int input) : key(input) {}
    virtual ~ConstructorBase() = default;
    const int key;
};
class Constructed : public ConstructorBase {
public:
    Constructed(int input, std::string text) : ConstructorBase(input), label(std::move(text)) {}
private:
    std::string label;
    int extra;
};
struct ChangedValue { explicit ChangedValue(int input) : value(input + 1) {} int value; };
struct AmbiguousConstructor {
    explicit AmbiguousConstructor(int input) : value(input) {}
    explicit AmbiguousConstructor(const int& input) : value(input) {}
    int value;
};
struct DefaultArgumentOverload {
    explicit DefaultArgumentOverload(const int& input) : value(input) {}
    explicit DefaultArgumentOverload(int&& input = 0) : value(input + 99) {}
    const int value;
};
struct InheritedArgumentBase {
    InheritedArgumentBase() = default;
    explicit InheritedArgumentBase(int&& input) : base(input + 99) {}
    int base;
};
struct InheritedArgumentOverload : InheritedArgumentBase {
    using InheritedArgumentBase::InheritedArgumentBase;
    explicit InheritedArgumentOverload(const int& input) : InheritedArgumentBase(), value(input) {}
    const int value = 77;
};
struct Borrowed { explicit Borrowed(int* input) : address(input) {} int* address; };
union Union { int integer; double real; };
struct Pointee { int number = 0; std::string text; };
struct PointeeBase { virtual ~PointeeBase() = default; int base = 0; };
struct PointeeDerived : PointeeBase { int extra = 0; };
struct DropS;
struct DropRef { DropS& s; std::mutex m; };
struct DropS { DropRef& ref; };
struct DropZ { int id; std::shared_ptr<DropS> s; };
struct PrunedLeaf { int n; };
struct PrunedOwner;
struct PrunedRef { PrunedLeaf* leaf; std::unique_ptr<PrunedOwner> next; };
struct PrunedOwner { PrunedLeaf* l; PrunedRef r; std::mutex m; };
struct Pinned { Pinned(int v, std::string s) : value(v), text(std::move(s)) {} Pinned(const Pinned&) = delete; int value; std::string text; };
struct PinnedHandle { int id; Pinned* pinned; };
struct Leaf { int number = 0; };
struct TreeNode;
struct Tree { std::vector<TreeNode> nodes; };
struct TreeNode { Leaf* leaf; std::vector<Tree> subtrees; };
struct Anchor { Anchor() = delete; explicit Anchor(int v); virtual ~Anchor(); int anchor_id; };
struct Holder { int holder_id; Anchor anchor; };
struct AnchorOwner { int owner_id; std::unique_ptr<Anchor> anchor; };
struct Target { int target_value = 0; std::string target_text; };
class Referrer { public: explicit Referrer(Target& t) : target(t) {} int referrer_id = 0; private: Target& target; };
struct Payload { int payload_id = 0; std::string payload_text; };
struct SmartHolder { int smart_id = 0; std::unique_ptr<Payload> single; std::shared_ptr<Payload> shared; std::weak_ptr<Payload> weak; std::vector<std::shared_ptr<Payload>> many; };
struct VirtualHandle : virtual VirtualBase { virtual void action(); Target* ptr; };
struct Texted { int tag; const char* text; int* number; void* opaque; };
struct Link { int link_id = 0; Link* next = nullptr; };
struct Chain { Link* head; };
struct NestedPointer { int tag; Pointee* pointee; };
struct OuterNested { int outer_id; NestedPointer inner; };
class Handle { public: Handle() = delete; virtual ~Handle(); int id; private: Pointee* target; PointeeBase* dynamic; const char* text; };
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
auto integer = [](int value) { return EventValue{std::int64_t{value}}; };
EventValue derived_value{EventValue::Object{{"application::Base::number", integer(31)}, {"application::Derived::number", integer(32)}, {"inherited", integer(33)}}, "application::Derived"};
auto derived = derived_value.as<application::Derived>();
if (derived.Base::number != 31 || derived.number != 32 || derived.inherited != 33) throw std::runtime_error("inherited field conversion failed");
EventValue poly_value{EventValue::Object{{"shared", integer(41)}, {"left", integer(42)}, {"right", integer(43)}, {"leaf", integer(44)}}, "application::Polymorphic"};
auto poly = poly_value.as<application::Polymorphic>();
if (poly.shared != 41 || poly.left != 42 || poly.right != 43 || poly.leaf != 44) throw std::runtime_error("virtual base conversion failed");
EventValue concrete_value{EventValue::Object{{"base", integer(51)}, {"value", integer(52)}}, "application::Concrete"};
auto concrete = concrete_value.as<application::Concrete>();
if (concrete.base != 51 || concrete.value != 52) throw std::runtime_error("abstract base field conversion failed");
std::cout << "automatic-inherited-records-ok" << std::endl;
EventValue constructed_value{EventValue::Object{{"key", integer(61)}, {"label", EventValue{std::string("constructor input")}}, {"extra", integer(62)}}, "application::Constructed"};
auto constructed = constructed_value.as<application::Constructed>();
if (constructed.key != 61 || constructed.label != "constructor input" || constructed.extra != 62) throw std::runtime_error("constructor factory conversion failed");
std::cout << "automatic-constructor-factory-ok" << std::endl;
auto address_of = [](std::uint64_t value) { return EventValue{value}; };
EventValue target_snapshot{EventValue::Object{{"number", integer(81)}, {"text", EventValue{std::string("target text")}}}, "application::Pointee"};
EventValue dynamic_snapshot{EventValue::Object{{"base", integer(82)}, {"extra", integer(83)}}, "application::PointeeDerived"};
EventValue handle_value{EventValue::Object{{"id", integer(80)}, {"target", address_of(4096)}, {"target_snapshot", target_snapshot}, {"dynamic", address_of(8192)}, {"dynamic_snapshot", dynamic_snapshot}, {"text", address_of(0)}}, "application::Handle"};
auto handle = handle_value.as<application::Handle>();
auto* dynamic = static_cast<application::PointeeDerived*>(handle->dynamic);
if (handle->id != 80 || !handle->target || handle->target->number != 81 || handle->target->text != "target text" || !dynamic || dynamic->base != 82 || dynamic->extra != 83 || handle->text != nullptr) throw std::runtime_error("reconstructed handle conversion failed");
EventValue bare_value{EventValue::Object{{"id", integer(84)}, {"target", address_of(4096)}, {"dynamic", address_of(8192)}, {"text", address_of(0)}}, "application::Handle"};
auto bare = bare_value.as<application::Handle>();
if (bare->id != 84 || bare->target || bare->dynamic) throw std::runtime_error("reconstructed handle without snapshots failed");
EventValue pinned_snapshot{EventValue::Object{{"value", integer(91)}, {"text", EventValue{std::string("pinned text")}}}, "application::Pinned"};
EventValue pinned_value{EventValue::Object{{"id", integer(90)}, {"pinned", address_of(4096)}, {"pinned_snapshot", pinned_snapshot}}, "application::PinnedHandle"};
auto pinned = pinned_value.as<application::PinnedHandle>();
if (pinned->id != 90 || !pinned->pinned || pinned->pinned->value != 91 || pinned->pinned->text != "pinned text") throw std::runtime_error("non-movable pointee conversion failed");
std::cout << "automatic-reconstructed-handle-ok" << std::endl;
auto text_of = [](const char* text) { return EventValue{std::string(text)}; };
EventValue anchor_value{EventValue::Object{{"anchor_id", integer(101)}}, "application::Anchor"};
auto anchor = anchor_value.as<application::Anchor>();
if (anchor->anchor_id != 101) throw std::runtime_error("standalone reconstructed record failed");
EventValue holder_value{EventValue::Object{{"holder_id", integer(102)}, {"anchor", EventValue{EventValue::Object{{"anchor_id", integer(103)}}, "application::Anchor"}}}, "application::Holder"};
auto holder = holder_value.as<application::Holder>();
if (holder->holder_id != 102 || holder->anchor.anchor_id != 103) throw std::runtime_error("nested reconstructed value failed");
EventValue owner_value{EventValue::Object{{"owner_id", integer(104)}, {"anchor", EventValue{EventValue::Object{{"address", address_of(4096)}, {"value", anchor_value}}}}}, "application::AnchorOwner"};
auto owner = owner_value.as<application::AnchorOwner>();
if (owner->owner_id != 104 || !owner->anchor || owner->anchor->anchor_id != 101) throw std::runtime_error("unique_ptr to a reconstructed record failed");
EventValue empty_owner_value{EventValue::Object{{"owner_id", integer(105)}, {"anchor", EventValue{EventValue::Object{{"address", address_of(0)}}}}}, "application::AnchorOwner"};
if (empty_owner_value.as<application::AnchorOwner>()->anchor) throw std::runtime_error("empty unique_ptr was not preserved");
EventValue target_value{EventValue::Object{{"target_value", integer(106)}, {"target_text", text_of("referent")}}, "application::Target"};
EventValue referrer_value{EventValue::Object{{"referrer_id", integer(107)}, {"target", address_of(4096)}, {"target_snapshot", target_value}}, "application::Referrer"};
auto referrer = referrer_value.as<application::Referrer>();
if (referrer->referrer_id != 107 || referrer->target.target_value != 106 || referrer->target.target_text != "referent") throw std::runtime_error("reference member failed");
EventValue loose_value{EventValue::Object{{"referrer_id", integer(108)}, {"target", address_of(4096)}}, "application::Referrer"};
bool missing_referent = false;
try { (void)loose_value.as<application::Referrer>(); } catch (const std::runtime_error& error) { missing_referent = std::string(error.what()).find("referent") != std::string::npos; }
if (!missing_referent) throw std::runtime_error("a reference without a referent must not decode");
auto payload = [](int id, const char* text) { return EventValue{EventValue::Object{{"payload_id", integer(id)}, {"payload_text", text_of(text)}}, "application::Payload"}; };
auto pointer = [](EventValue value) { return EventValue{EventValue::Object{{"address", address_of(4096)}, {"value", std::move(value)}}}; };
EventValue smart_value{EventValue::Object{{"smart_id", integer(109)}, {"single", pointer(payload(110, "single"))}, {"shared", pointer(payload(111, "shared"))}, {"weak", EventValue{EventValue::Object{{"expired", EventValue{false}}}}}, {"many", EventValue{EventValue::Array{pointer(payload(112, "first")), pointer(payload(113, "second"))}}}}, "application::SmartHolder"};
auto smart = smart_value.as<application::SmartHolder>();
if (smart.smart_id != 109 || !smart.single || smart.single->payload_id != 110 || !smart.shared || smart.shared->payload_text != "shared" || smart.many.size() != 2 || smart.many.at(1)->payload_id != 113 || !smart.weak.expired()) throw std::runtime_error("smart pointer conversion failed");
EventValue texted_value{EventValue::Object{{"tag", integer(120)}, {"text", address_of(4096)}, {"text_snapshot", text_of("hello")}, {"number", address_of(4096)}, {"number_snapshot", integer(77)}, {"opaque", address_of(4096)}}, "application::Texted"};
auto texted = texted_value.as<application::Texted>();
if (texted->tag != 120 || !texted->text || std::string(texted->text) != "hello" || !texted->number || *texted->number != 77 || texted->opaque != nullptr) throw std::runtime_error("C string and scalar pointers failed");
EventValue bare_texted{EventValue::Object{{"tag", integer(121)}, {"text", address_of(4096)}, {"number", address_of(4096)}, {"opaque", address_of(0)}}, "application::Texted"};
auto bare_text = bare_texted.as<application::Texted>();
if (bare_text->text || bare_text->number) throw std::runtime_error("uncaptured pointers must be null");
auto link = [](int id, EventValue next_snapshot) { EventValue::Object fields{{"link_id", integer(id)}, {"next", address_of(4096)}}; if (!next_snapshot.is_null()) fields.emplace("next_snapshot", std::move(next_snapshot)); return EventValue{std::move(fields), "application::Link"}; };
EventValue chain_value{EventValue::Object{{"head", address_of(4096)}, {"head_snapshot", link(1, link(2, link(3, EventValue{})))}}, "application::Chain"};
auto chain = chain_value.as<application::Chain>();
if (!chain->head || chain->head->link_id != 1 || !chain->head->next || chain->head->next->link_id != 2 || !chain->head->next->next || chain->head->next->next->link_id != 3 || chain->head->next->next->next != nullptr) throw std::runtime_error("multi-level pointers failed");
EventValue nested_value{EventValue::Object{{"outer_id", integer(122)}, {"inner", EventValue{EventValue::Object{{"tag", integer(123)}, {"pointee", address_of(4096)}, {"pointee_snapshot", payload(124, "inner")}}, "application::NestedPointer"}}}, "application::OuterNested"};
auto outer = nested_value.as<application::OuterNested>();
if (outer->outer_id != 122 || outer->inner.tag != 123) throw std::runtime_error("pointer in a nested record failed");
std::cout << "automatic-reconstruction-rules-ok" << std::endl;
''', True)
        assert good.returncode == 0, (good.stdout, good.stderr)
        for marker in ("automatic-nested-records-ok", "automatic-template-and-once-ok",
                       "automatic-inherited-records-ok", "automatic-constructor-factory-ok",
                       "automatic-reconstructed-handle-ok", "automatic-reconstruction-rules-ok"):
            assert marker in good.stdout, (good.stdout, good.stderr)
        assert "application::Private" in good.stdout, good.stdout
        assert "reconstructed application::Handle" in good.stdout, good.stdout
        # Records that only fail to construct are rebuilt field by field.
        for name in ("PrivateUnassignableField", "UndefinedVirtual", "ChangedValue", "Borrowed",
                     "Texted", "Link", "Chain", "OuterNested",
                     "AmbiguousConstructor", "DefaultArgumentOverload",
                     "InheritedArgumentOverload", "Anchor", "Holder", "AnchorOwner", "Referrer"):
            assert f"reconstructed application::{name}" in good.stdout, good.stdout
        # A pointee that fails after being adopted must not reject the import.
        assert "reconstructed application::PrunedRef" in good.stdout, good.stdout
        assert "skipped application::PrunedOwner:" in good.stdout, good.stdout
        # A holder of a record dropped after planning is dropped with it.
        for name in ("DropRef", "DropS", "DropZ"):
            assert f"skipped application::{name}:" in good.stdout, good.stdout
        assert "it uses application::DropS, which could not be mapped" in good.stdout, good.stdout
        for name in ("AbstractBase", "Repeated", "Union", "Tree", "TreeNode", "VirtualHandle"):
            assert f"skipped application::{name}:" in good.stdout, good.stdout
        for name in ("SmartHolder", "Payload", "Pinned"):
            assert not re.search(rf"reconstructed application::{name}(;|\s|$)", good.stdout), good.stdout
            assert f"skipped application::{name}:" not in good.stdout, good.stdout
        assert "city" in good.stdout, good.stdout

        rejected = run(settings + '%cpp import "types.hpp"\n' + code + '''%cpp import "types.hpp" --type application::UndefinedVirtual
%cpp import "types.hpp" --type application::NoDefault
%cpp import "types.hpp" --type application::Constant
%cpp import "types.hpp" --type application::VirtualHandle
%cpp import "types.hpp" --type application::Tree
%cpp import "types.hpp" --type application::Union
%cpp import "missing-header.hpp"
%cpp options add "-DREPL_APP_VERSION=99"
if(order.address.city!="Paris" || application::Address::version!=7 || application::header_runs!=1) throw std::runtime_error("failed import changed live state");
std::cout << "failed-import-state-retained" << std::endl;
''')
        assert rejected.returncode != 0, (rejected.stdout, rejected.stderr)
        assert "failed-import-state-retained" in rejected.stdout, (rejected.stdout, rejected.stderr)
        assert "options are locked" in rejected.stderr, rejected.stderr
        # Records that still cannot be mapped keep a precise reason.
        assert "virtual bases cannot be reconstructed" in rejected.stderr, rejected.stderr
        assert "cyclic value field requires an EventCodec" in rejected.stderr, rejected.stderr
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
template<> struct tracer_repl::ReconstructedRecord<application::Handle> { static tracer_repl::Reconstructed<application::Handle> decode(const tracer_repl::EventValue&) { tracer_repl::Reconstructed<application::Handle> record; record->id = 99; return record; } };
template<> struct tracer_repl::EventRecord<application::HandMapped> { static constexpr const char* type_name="application::HandMapped"; static auto fields() { return std::make_tuple(tracer_repl::event_member("wire", &application::HandMapped::number)); } };
%cpp import "types.hpp"
tracer_repl::EventValue custom{tracer_repl::EventValue::Object{{"wire",tracer_repl::EventValue{std::int64_t{23}}}}, "application::HandMapped"};
tracer_repl::EventValue handle_custom{tracer_repl::EventValue::Object{}, "application::Handle"};
if(handle_custom.as<application::Handle>()->id!=99) throw std::runtime_error("manual reconstructed codec was not preserved");
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
