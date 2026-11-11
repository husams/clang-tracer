"""Verify that a handle record keeps its private implementation visible to the REPL."""

import argparse
import json
import os
from pathlib import Path
import select
import shutil
import subprocess
import tempfile
import time


SOURCES = ("item.hpp", "item-impl.hpp", "heap.cpp", "target.cpp", "widget.cpp", "snapshot-policy.hpp",
           "map-policy.hpp", "shared-callback.cpp")
GET = "HFW::HfwManager::getItem(int)"
MAKE = "HFW::HfwManager::makeItem(int)"
INSPECT = "HFW::HfwManager::inspectItem(const HFW::HfwItem &)"
MAKE_BIG = ("HFW::HfwManager::makeBig(const HFW::HfwBig &, const HFW::HfwBig &, "
            "const HFW::HfwBig &, const HFW::HfwBig &)")
LOAD = "HFW::HfwManager::load(HFW::HfwItem &)"
RECURSE = "HFW::HfwManager::recurse(const HFW::HfwItem &, int)"
WIDGET = "HFW::HfwManager::widgetId(const HFW::Widget &)"
GADGET = "HFW::HfwManager::gadgetId(const HFW::Gadget &)"
POLY = "HFW::HfwManager::polyId(const HFW::PolyWidget &)"
BUDGET = ("HFW::HfwManager::budget(const HFW::HfwBig &, const HFW::HfwBig &, "
          "const HFW::HfwBig &, const HFW::HfwBig &)")

# name, compile definitions, selector, phase, how the event exposes the record
SCENARIOS = (
    ("owned", ("HANDLE_VIRTUAL_IMPL",), GET, "POST", {}),
    ("external", ("HANDLE_VIRTUAL_IMPL",), GET, "POST", {"external": True}),
    # A non-polymorphic implementation needs no policy: its static type is exact.
    ("plain", (), GET, "POST", {}),
    ("policy", (), GET, "POST", {"policy": True}),
    ("reference", ("HANDLE_REFERENCE",), GET, "POST", {"reference": True}),
    ("unique", ("HANDLE_UNIQUE",), GET, "POST", {"smart": True}),
    ("shared", ("HANDLE_SHARED",), GET, "POST", {"smart": True}),
    ("base", ("HANDLE_BASE_MEMBER",), GET, "POST", {}),
    ("value", (), MAKE, "POST", {}),
    ("argument", (), INSPECT, "PRE", {"argument": True}),
    # Strings, scalars, nested records, rings, receivers and unreadable pointers.
    ("deep", ("HANDLE_DEEP",), GET, "POST", {"deep": True}),
    ("deep_unique", ("HANDLE_DEEP", "HANDLE_UNIQUE"), GET, "POST", {"smart": True, "deep": True}),
    ("shared_weak", ("HANDLE_SHARED",), GET, "POST", {"smart": True, "weak": True}),
    # Four large roots fill the snapshot storage; followed members must not displace them.
    ("budget", ("HANDLE_BUDGET",), BUDGET, "PRE", {"budget": True, "record": "HFW::HfwBig"}),
    # Followed members captured at entry must leave room for the result root.
    ("budget_result", ("HANDLE_BUDGET",), MAKE_BIG, "POST",
     {"budget": True, "record": "HFW::HfwBig", "result": True}),
    # Objects the target left stale: the capture must neither crash nor hang it.
    ("stale", ("HANDLE_STALE",), LOAD, "PRE", {"stale": True}),
    ("map_auto", ("HANDLE_MAP",), GET, "POST", {"map": False}),
    ("map_policy", ("HANDLE_MAP",), GET, "POST", {"map": True, "include": ["map-policy.hpp"]}),
    ("scalars", ("HANDLE_SCALARS",), GET, "POST", {"scalars": True}),
    ("thread", ("HANDLE_SCALARS", "HANDLE_THREAD"), GET, "POST", {"scalars": True}),
    ("recurse", ("HANDLE_RECURSE",), RECURSE, "PRE", {"survive": True}),
    ("widget", ("HANDLE_WIDGET",), WIDGET, "PRE", {"widget": True, "record": "HFW::Widget",
                                                    "include": ["widget.cpp"]}),
    ("gadget", ("HANDLE_WIDGET",), GADGET, "PRE", {"gadget": True, "record": "HFW::Gadget",
                                                    "include": ["widget.cpp"]}),
    ("poly", ("HANDLE_WIDGET",), POLY, "PRE", {"poly": True, "record": "HFW::PolyWidget",
                                                "include": ["widget.cpp"]}),
    # The same through a callback with shared state, which installs through the
    # shared-probe path: the probe must still be installed.
    ("fallback_shared", (), GET, "POST", {"shared": True, "break": True}),
    # The test-only switch breaks the generated code that follows members: the
    # probe must still be installed, capturing without them.
    ("fallback", (), GET, "POST", {"fallback": True, "break": True}),
)

# The largest --follow-depth (snapshot_limits.hpp kMaxFollowDepth).
MAX_FOLLOW_DEPTH = 40
SCALARS = ("HANDLE_SCALARS",)
LIMITS = ("HANDLE_LIMITS",)
DEEP_DEFINES = ("HANDLE_DEEP",)

# Every knob in both directions: name, defines, selector, phase, options.
KNOBS = (
    ("depth_default", LIMITS, GET, "POST",
     {"body": "chain", "levels": 3, "cut": True}),
    ("depth_five", LIMITS, GET, "POST",
     {"body": "chain", "levels": 5, "prepare_extra": "--follow-depth 5"}),
    # The longest chain the option accepts, through the whole pipeline; the fixture
    # chain is longer, so the cut at the maximum is visible.
    ("depth_unlimited", LIMITS, GET, "POST",
     {"body": "chain", "levels": MAX_FOLLOW_DEPTH, "cut": True,
      "prepare_extra": "--follow-depth unlimited"}),
    ("depth_env", LIMITS, GET, "POST",
     {"body": "chain", "levels": 5, "cut": True, "repl_env": {"TRACER_PREPARE_FOLLOW_DEPTH": "5"}}),
    ("depth_lowered", LIMITS, GET, "POST",
     {"body": "chain", "levels": 2, "prepare_extra": "--follow-depth 5",
      "inject_extra": "--follow-depth 2"}),
    ("depth_off", LIMITS, GET, "POST",
     {"body": "chain_off", "prepare_extra": "--follow-depth 0"}),
    ("string_default", SCALARS, GET, "POST", {"body": "string", "chars": 4000, "cut": True}),
    ("string_raised", SCALARS, GET, "POST",
     {"body": "string", "chars": 5000, "cut": False,
      "inject_extra": "--string-bytes 6000 --snapshot-slice 8192 --snapshot-arena 65536"}),
    ("string_lowered", SCALARS, GET, "POST",
     {"body": "string", "chars": 100, "cut": True, "inject_extra": "--string-bytes 100"}),
    ("string_clamped", SCALARS, GET, "POST",
     {"body": "string", "chars": 4000, "cut": True, "note_has": "--snapshot-slice",
      "inject_extra": "--string-bytes 6000"}),
    ("members_default", DEEP_DEFINES, GET, "POST", {"body": "members", "min": 5, "max": 1000}),
    ("members_capped", DEEP_DEFINES, GET, "POST",
     {"body": "members", "min": 1, "max": 3, "root_cut": True, "inject_extra": "--follow-members 3"}),
    ("members_unlimited", DEEP_DEFINES, GET, "POST",
     {"body": "members", "min": 5, "max": 1000, "prepare_extra": "--follow-members unlimited"}),
    ("records_capped", DEEP_DEFINES, GET, "POST",
     {"body": "members", "min": 1, "max": 2, "prepare_extra": "--record-members 2",
      "expect_output": ["--record-members"]}),
    ("nested_default", DEEP_DEFINES, GET, "POST", {"body": "nested", "count": 1}),
    ("nested_off", DEEP_DEFINES, GET, "POST",
     {"body": "nested", "count": 0, "prepare_extra": "--nested-depth 0",
      "expect_output": ["--nested-depth"]}),
    ("vector_default", LIMITS, GET, "POST", {"body": "vector", "size": 10}),
    ("vector_capped", LIMITS, GET, "POST",
     {"body": "vector", "size": 0, "cut": "--vector-elements",
      "inject_extra": "--vector-elements 5"}),
    # More elements than the default limit, and more bytes than the default slice.
    ("wide_default", LIMITS, GET, "POST",
     {"body": "vector", "field": "_wide", "size": 0, "cut": "--vector-elements"}),
    ("wide_raised", LIMITS, GET, "POST",
     {"body": "vector", "field": "_wide", "size": 5000,
      "inject_extra": "--vector-elements 6000 --snapshot-slice 32768"}),
    ("reads_default", LIMITS, GET, "POST", {"body": "reads", "ok": True}),
    ("reads_capped", LIMITS, GET, "POST",
     {"body": "reads", "ok": False, "cut": "--checked-reads",
      "inject_extra": "--checked-reads 2"}),
    ("none_automatic", ("HANDLE_VIRTUAL_IMPL",), GET, "POST",
     {"body": "none", "prepare_extra": "--no-automatic-members"}),
    # Without the reserve, members captured at entry take the result root's storage.
    ("budget_noreserve", ("HANDLE_BUDGET",), MAKE_BIG, "POST",
     {"body": "budget_lost", "record": "HFW::HfwBig", "inject_extra": "--snapshot-reserve 0"}),
    ("budget_arena", ("HANDLE_BUDGET",), MAKE_BIG, "POST",
     {"budget": True, "record": "HFW::HfwBig", "result": True,
      "inject_extra": "--snapshot-reserve 0 --snapshot-arena 65536"}),
    ("derived_none", ("HANDLE_WIDGET",), POLY, "PRE",
     {"body": "derived", "record": "HFW::PolyWidget", "include": ["widget.cpp"],
      "repl_env": {"TRACER_REPL_DERIVED_CANDIDATES": "0"},
      "expect_output": ["TRACER_REPL_DERIVED_CANDIDATES"]}),
)
SCENARIOS = SCENARIOS + KNOBS

BODIES = {
    "chain": '''auto item = events.at(0).result_as<HFW::HfwItem>();
int levels = 0; for (auto *node = item->_chain; node; node = node->next) ++levels;
if (levels != @LEVELS@) throw std::runtime_error("the chain was captured to " + std::to_string(levels) + " levels");
@CHAINCUT@''',
    "chain_off": '''auto item = events.at(0).result_as<HFW::HfwItem>();
if (item->_chain != nullptr) throw std::runtime_error("nothing should be followed at depth 0");
''',
    "string": '''auto root = events.at(0).result_snapshot();
auto item = events.at(0).result_as<HFW::HfwItem>();
if (!item->_long || std::string(item->_long).size() != @CHARS@) throw std::runtime_error("wrong C string length");
const bool cut = root.fields().count("_long_snapshot_cut") != 0;
if (cut != @CUT@) throw std::runtime_error("wrong cut note");
if (cut && root.field("_long_snapshot_cut").as<std::string>().find("@NOTEHAS@") == std::string::npos) throw std::runtime_error("the note must name the option");
''',
    "members": '''auto root = events.at(0).result_snapshot();
int count = 0; for (const auto &f : root.fields()) if (f.first.size() > 9 && f.first.compare(f.first.size() - 9, 9, "_snapshot") == 0) ++count;
if (count < @MIN@ || count > @MAX@) throw std::runtime_error("wrong number of followed members: " + std::to_string(count));
@ROOTCUT@''',
    "nested": '''auto root = events.at(0).result_snapshot();
if (root.field("_nested").fields().count("impl_snapshot") != @COUNT@) throw std::runtime_error("wrong nested capture");
''',
    "vector": '''auto root = events.at(0).result_snapshot();
auto item = events.at(0).result_as<HFW::HfwItem>();
if (@SIZE@ ? (!item->@VEC@ || item->@VEC@->values.size() != @SIZE@) : item->@VEC@ != nullptr) throw std::runtime_error("wrong vector capture");
@CUTNOTE@''',
    "reads": '''auto root = events.at(0).result_snapshot();
auto item = events.at(0).result_as<HFW::HfwItem>();
if (@OK@ != (item->_strings != nullptr && item->_strings->d == "d")) throw std::runtime_error("wrong strings capture");
@CUTNOTE@''',
    "none": '''auto root = events.at(0).result_snapshot();
for (const auto &f : root.fields()) if (f.first.size() > 9 && f.first.compare(f.first.size() - 9, 9, "_snapshot") == 0) throw std::runtime_error("a member was followed");
''',
    "budget_lost": '''bool lost = false;
try { (void)events.at(0).result_snapshot(); } catch (const std::exception &) { lost = true; }
if (!lost) throw std::runtime_error("without the reserve the result root should have lost its storage");
''',
    "derived": '''auto widget = events.at(0).argument_as<HFW::PolyWidget>(0);
if (widget->impl_ != nullptr || !widget->plain_) throw std::runtime_error("derived records must not be tried");
''',
}
CUT_NOTE = '''if (root.fields().count("@FIELD@_snapshot_cut") != 1 || root.field("@FIELD@_snapshot_cut").as<std::string>().find("@KNOB@") == std::string::npos) throw std::runtime_error("the note must name the option");
'''


def knob_body(options):
    body = BODIES[options["body"]]
    cut = ""
    if options.get("cut") and options["body"] in ("vector", "reads"):
        field = options.get("field", "_vec") if options["body"] == "vector" else "_strings"
        cut = CUT_NOTE.replace("@FIELD@", field).replace("@KNOB@", options["cut"])
    body = body.replace("@CUTNOTE@", cut).replace("@VEC@", options.get("field", "_vec"))
    body = body.replace("@NOTEHAS@", options.get("note_has", "--string-bytes"))
    root_cut = ""
    if options["body"] == "members":
        root_cut = ('if (root.fields().count("snapshot_cut") != 1 || root.field("snapshot_cut").as<std::string>().find("--follow-members") == std::string::npos) '
                    'throw std::runtime_error("the dropped members must be reported on the root");\n'
                    if options.get("root_cut") else
                    'if (root.fields().count("snapshot_cut") != 0) throw std::runtime_error("nothing was cut");\n')
    body = body.replace("@ROOTCUT@", root_cut)
    chain_cut = ""
    if options["body"] == "chain" and options.get("cut"):
        hops = "".join('.field("next_snapshot")' for _ in range(options["levels"] - 2))
        chain_cut = ('{ const auto whole = events.at(0).result_snapshot(); const auto &holder = whole.field("_chain_snapshot")'
                     + hops + '; if (holder.fields().count("next_snapshot_cut") != 1 || '
                     'holder.field("next_snapshot_cut").as<std::string>().find("--follow-depth") == std::string::npos) '
                     'throw std::runtime_error("the depth cut must be visible"); }\n')
    body = body.replace("@CHAINCUT@", chain_cut)
    for key, value in (("@LEVELS@", options.get("levels")), ("@CHARS@", options.get("chars")),
                       ("@CUT@", "true" if options.get("cut") else "false"),
                       ("@MAX@", options.get("max")), ("@MIN@", options.get("min")), ("@COUNT@", options.get("count")),
                       ("@SIZE@", options.get("size")),
                       ("@OK@", "true" if options.get("ok") else "false")):
        if value is not None:
            body = body.replace(key, str(value))
    return body


CUSTOM = {
    "stale": '''auto root = events.at(0).argument_snapshot(0);
if (root.field("_impl").as<std::uint64_t>() == 0) throw std::runtime_error("missing implementation address");
if (root.fields().count("_impl_snapshot") != 0 || root.fields().count("_stale_snapshot") != 0 || root.fields().count("_vbase_snapshot") != 0 || root.fields().count("_big_snapshot") != 0) throw std::runtime_error("a stale pointee must stay address-only");
auto item = events.at(0).argument_as<HFW::HfwItem>(0);
if (item->_impl != nullptr || item->_stale != nullptr || item->_vbase != nullptr || item->_big != nullptr) throw std::runtime_error("a stale pointee must be null locally");
''',
    "scalars": '''auto item = events.at(0).result_as<HFW::HfwItem>();
if (!item->_flag || !*item->_flag) throw std::runtime_error("bool pointer was not captured");
if (!item->_signed || std::string(reinterpret_cast<const char *>(item->_signed)).size() != 2 || item->_signed[1] != 'a') throw std::runtime_error("signed char string was not captured");
if (!item->_byte || *item->_byte != 200) throw std::runtime_error("unsigned char pointer was not captured");
if (!item->_mode || *item->_mode != HFW::high) throw std::runtime_error("enum pointer was not captured");
if (!item->_long || std::string(item->_long).size() != 4000) throw std::runtime_error("a long C string must be delivered truncated");
if (!item->_first || std::string(item->_first) != "first" || std::string(item->_second) != "second" || std::string(item->_third) != "third") throw std::runtime_error("C strings were not captured");
''',
    "widget": '''auto root = events.at(0).argument_snapshot(0);
if (root.field("id").as<int>() != 3 || root.field("impl_").as<std::uint64_t>() == 0) throw std::runtime_error("widget fields were not captured");
if (root.fields().count("impl__snapshot") != 1 || root.field("impl__snapshot").field("secret").as<int>() != 5) throw std::runtime_error("the private nested implementation was not captured");
if (root.field("owned_").fields().count("value") != 1) throw std::runtime_error("the owned implementation was not captured");
auto widget = events.at(0).argument_as<HFW::Widget>(0);
if (widget->id != 3 || !widget->impl_ || widget->impl_->secret != 5 || !widget->owned_ || widget->owned_->secret != 5) throw std::runtime_error("widget was not rebuilt");
''',
    "poly": '''auto root = events.at(0).argument_snapshot(0);
if (root.fields().count("impl__snapshot") != 1 || root.field("impl__snapshot").field("extra").as<int>() != 8 || root.field("impl__snapshot").field("secret").as<int>() != 7) throw std::runtime_error("the derived private nested implementation was not captured");
if (root.fields().count("plain__snapshot") != 1 || root.field("plain__snapshot").field("secret").as<int>() != 7 || root.field("plain__snapshot").fields().count("extra") != 0) throw std::runtime_error("the base private nested implementation was not captured");
auto widget = events.at(0).argument_as<HFW::PolyWidget>(0);
if (widget->id != 5 || !widget->impl_ || widget->impl_->secret != 7 || static_cast<HFW::PolyWidget::Special *>(widget->impl_)->extra != 8 || !widget->plain_ || widget->plain_->secret != 7) throw std::runtime_error("poly widget was not rebuilt");
''',
    "gadget": '''auto root = events.at(0).argument_snapshot(0);
if (root.fields().count("impl__snapshot") != 1 || root.field("impl__snapshot").field("secret").as<int>() != 6) throw std::runtime_error("the referenced private nested implementation was not captured");
auto gadget = events.at(0).argument_as<HFW::Gadget>(0);
if (gadget->id != 4 || gadget->impl_.secret != 6) throw std::runtime_error("gadget was not rebuilt");
''',
    "fallback": '''auto root = events.at(0).result_snapshot();
if (root.fields().count("_impl_snapshot") != 0) throw std::runtime_error("followed members must be disabled by the fallback");
auto item = events.at(0).result_as<HFW::HfwItem>();
if (item->_impl != nullptr) throw std::runtime_error("an unfollowed pointer must be null locally");
''',
    "survive": '''auto root = events.at(0).argument_snapshot(0);
if (root.cpp_type() != "HFW::HfwItem") throw std::runtime_error("wrong root type");
''',
}

DEEP = '''if (!item->_note || std::string(item->_note) != "note text") throw std::runtime_error("C string member was not captured");
if (!item->_counter || *item->_counter != 42) throw std::runtime_error("scalar pointee was not captured");
if (item->_text != nullptr || root.fields().count("_text_snapshot") != 0) throw std::runtime_error("a std:: pointee must stay address-only");
if (item->_dangling != nullptr || root.fields().count("_dangling_snapshot") != 0 || root.field("_dangling").as<std::uint64_t>() == 0) throw std::runtime_error("an unmapped pointer must stay address-only");
if (!item->_nested.impl || item->_nested.impl->revision != 827) throw std::runtime_error("pointer inside a nested record was not captured");
if (!item->_nested.impl->_detail || item->_nested.impl->_detail->depth != 3 || item->_nested.impl->_detail->back != nullptr) throw std::runtime_error("second level or cycle guard failed");
if (!item->_head || item->_head->id != 1 || !item->_head->next || item->_head->next->id != 2 || !item->_head->next->next || item->_head->next->next->id != 3 || item->_head->next->next->next != nullptr) throw std::runtime_error("ring was not captured to the depth bound");
{impl_chain}
if (!item->_loop || item->_loop->id != 9 || item->_loop->next != nullptr) throw std::runtime_error("a node linked to itself was not cut");
auto manager = events.at(0).receiver_snapshot();
if (manager.field("tracked__snapshot").field("revision").as<int>() != 827) throw std::runtime_error("receiver pointer was not captured");
'''



def session_source(name, selector, phase, options):
    if "body" in options:
        marker = f"HANDLE_RETURN_{name.upper()}=1"
        includes = "".join(f" --include {name_}" for name_ in options.get("include", []))
        prepare_extra = (" " + options["prepare_extra"]) if options.get("prepare_extra") else ""
        inject_extra = (" " + options["inject_extra"]) if options.get("inject_extra") else ""
        source = f'''%probe prepare "{selector}" --include item.hpp --include item-impl.hpp{includes} --output prepared_{name}{prepare_extra}
%probe inject "{selector}" --prepared prepared_{name} --prepared-mode require{inject_extra}
%collect --name "{selector}" --count 1 --timeout 20 --phase {phase} --as captured_items
auto events = tracer.batch("captured_items");
if (events.size() != 1) throw std::runtime_error("expected one event");
''' + knob_body(options) + f'std::cout << "{marker}" << std::endl;\n%quit\n'
        return source, marker
    if options.get("shared"):
        marker = f"HANDLE_RETURN_{name.upper()}=1"
        return (f'''%probe prepare "{selector}" --include item.hpp --include item-impl.hpp --output prepared_{name}
%probe inject "{selector}" --callback shared-callback.cpp --prepared prepared_{name} --prepared-mode require
std::cout << "{marker}" << std::endl;
%quit
''', marker)
    custom = next((body for key, body in CUSTOM.items() if options.get(key) is True), None)
    if "map" in options:
        custom = ('''auto root = events.at(0).result_snapshot();
if (root.field("_mapped").as<std::uint64_t>() == 0) throw std::runtime_error("missing map pointer");
auto item = events.at(0).result_as<HFW::HfwItem>();
''' + ('''if (root.fields().count("_mapped_snapshot") != 1 || !item->_mapped || item->_mapped->entries.at(1) != 2) throw std::runtime_error("a named map pointee must be followed");
''' if options["map"] else '''if (root.fields().count("_mapped_snapshot") != 0 || item->_mapped != nullptr) throw std::runtime_error("a map pointee must stay address-only without a policy");
'''))
    if custom is not None:
        marker = f"HANDLE_RETURN_{name.upper()}=1"
        includes = "".join(f" --include {name_}" for name_ in options.get("include", []))
        source = f'''%probe prepare "{selector}" --include item.hpp --include item-impl.hpp{includes} --output prepared_{name}
%probe inject "{selector}" --prepared prepared_{name} --prepared-mode require
%collect --name "{selector}" --count 1 --timeout 20 --phase {phase} --as captured_items
auto events = tracer.batch("captured_items");
if (events.size() != 1) throw std::runtime_error("expected one event");
''' + custom + f'std::cout << "{marker}" << std::endl;\n%quit\n'
        return source, marker
    if options.get("budget"):
        marker = f"HANDLE_RETURN_{name.upper()}=1"
        inject_extra_budget = (" " + options["inject_extra"]) if options.get("inject_extra") else ""
        result_check = ('if (events.at(0).result_snapshot().field("text").as<std::string>().size() != 3000) '
                        'throw std::runtime_error("the result root lost its snapshot to a followed member");\n'
                        if options.get("result") else "")
        source = f'''%probe prepare "{selector}" --include item.hpp --include item-impl.hpp --output prepared_{name}
%probe inject "{selector}" --prepared prepared_{name} --prepared-mode require{inject_extra_budget}
%collect --name "{selector}" --count 1 --timeout 20 --phase {phase} --as captured_items
auto events = tracer.batch("captured_items");
if (events.size() != 1) throw std::runtime_error("expected one event");
for (std::size_t index = 0; index < 4; ++index) {{ if (events.at(0).argument_snapshot(index).field("text").as<std::string>().size() != 3000) throw std::runtime_error("a root lost its snapshot to a followed member"); }}
{result_check}std::cout << "{marker}" << std::endl;
%quit
'''
        return source, marker
    include_policy = " --include snapshot-policy.hpp" if options.get("policy") else ""
    argument = options.get("argument")
    root = "events.at(0).argument_snapshot(0)" if argument else "events.at(0).result_snapshot()"
    typed = ("events.at(0).argument_as<HFW::HfwItem>(0)" if argument
             else "events.at(0).result_as<HFW::HfwItem>()")
    member = "item->_impl." if options.get("reference") else "item->_impl->"
    source = f'''%probe prepare "{selector}" --include item.hpp --include item-impl.hpp{include_policy} --output prepared_{name}
%probe inject "{selector}" --prepared prepared_{name} --prepared-mode require
%collect --name "{selector}" --count 1 --timeout 20 --phase {phase} --as captured_items
auto events = tracer.batch("captured_items");
if (events.size() != 1) throw std::runtime_error("expected one event");
auto root = {root};
if (root.cpp_type() != "HFW::HfwItem") throw std::runtime_error("wrong root type");
'''
    if options.get("smart"):
        source += '''const auto &held = root.field("_impl");
if (held.field("address").as<std::uint64_t>() == 0 || held.fields().count("value") != 1) throw std::runtime_error("missing smart pointer snapshot");
if (held.field("value").field("revision").as<int>() != 827 || held.field("value").field("label").as<std::string>() != "implementation label") throw std::runtime_error("wrong smart pointer snapshot");
'''
    else:
        source += '''if (root.field("_impl").as<std::uint64_t>() == 0) throw std::runtime_error("missing implementation address");
if (root.fields().count("_impl_snapshot") != 1) throw std::runtime_error("missing implementation snapshot");
const auto &implementation = root.field("_impl_snapshot");
if (implementation.cpp_type() != "HFW::HfwItemImpl" || implementation.field("revision").as<int>() != 827 || implementation.field("label").as<std::string>() != "implementation label") throw std::runtime_error("wrong implementation snapshot");
auto typed = root.field("_impl_snapshot").as<HFW::HfwItemImpl>();
if (typed{dot}revision != 827 || typed{dot}label != "implementation label") throw std::runtime_error("wrong typed implementation");
'''.replace("{dot}", "->" if options.get("deep") else ".")
    source += f'''auto item = {typed};
if (!(item->_impl) || {member}revision != 827 || {member}label != "implementation label") throw std::runtime_error("wrong reconstructed handle");
'''.replace("!(item->_impl) ||", "") if options.get("reference") else f'''auto item = {typed};
if (!item->_impl || {member}revision != 827 || {member}label != "implementation label") throw std::runtime_error("wrong reconstructed handle");
'''
    if options.get("deep"):
        # A smart pointer's pointee is copied inline rather than followed, so the
        # link back to it is followed once more and cut one level later.
        chain = ('if (!item->_impl->_detail || item->_impl->_detail->depth != 3 || !item->_impl->_detail->back || item->_impl->_detail->back->_detail != nullptr) throw std::runtime_error("cyclic link was not cut");'
                 if options.get("smart") else
                 'if (!item->_impl->_detail || item->_impl->_detail->depth != 3 || item->_impl->_detail->back != nullptr) throw std::runtime_error("cyclic link was not cut");')
        source += DEEP.replace("{impl_chain}", chain)
    if options.get("weak"):
        source += '''if (!item->_weak.expired()) throw std::runtime_error("a weak_ptr cannot refer to the target's object");
if (root.field("_weak").field("expired").as<bool>()) throw std::runtime_error("weak_ptr snapshot lost its state");
'''
    marker = f"HANDLE_RETURN_{name.upper()}=1"
    return source + f'std::cout << "{marker}" << std::endl;\n%quit\n', marker


def build(args, root, defines):
    root.mkdir()
    for name in SOURCES:
        shutil.copy(args.example / name, root / name)
    flags = [str(args.compiler), "-std=c++17", "-g", "-O0", "-fno-omit-frame-pointer",
             "-fcf-protection=none", *[f"-D{define}" for define in defines]]
    subprocess.run([*flags, "-shared", "-fPIC", "-Wl,--build-id=sha1", "heap.cpp",
                    "-o", "libhandle_heap.so"], cwd=root, check=True, timeout=60)
    subprocess.run([*flags, "-pthread", "target.cpp", "widget.cpp", "-L.", "-lhandle_heap",
                    "-Wl,-rpath,$ORIGIN", "-o", "target"], cwd=root, check=True, timeout=60)
    (root / "compile_commands.json").write_text(json.dumps([
        {"directory": str(root), "file": str(root / "target.cpp"),
         "arguments": [*flags, "-c", str(root / "target.cpp")]},
    ]))


def start_external_server(args, root, env):
    socket = root / "external.sock"
    server = subprocess.Popen(
        [args.server, "--listen", f"unix://{socket}", "--agent", args.agent, "--timeout", "60"],
        cwd=root, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    deadline = time.monotonic() + 10
    while not socket.exists():
        assert server.poll() is None, "external server exited"
        assert time.monotonic() < deadline, "external server did not listen"
        time.sleep(0.05)
    return server, f"unix://{socket}"


def run_session(args, root, env, name, selector, phase, options, endpoint=None):
    source, marker = session_source(name, selector, phase, options)
    connection = (["--endpoint", endpoint] if endpoint
                  else ["--server", args.server, "--agent", args.agent])
    target_env = ({**env, "TRACER_TEST_BREAK_FOLLOWED_MEMBERS": "1"} if options.get("break")
                  else env)
    with subprocess.Popen([root / "target"], cwd=root, env=target_env, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, text=True) as target:
        try:
            started = select.select([target.stdout], [], [], 10)[0]
            assert started, "target did not start"
            ready = target.stdout.readline().strip()
            assert ready == "ready", "target failed to start"
            with (root / f"{name}.log").open("w+") as transcript:
                console = subprocess.Popen(
                    [args.repl, *connection, "--attach", str(target.pid), "--timeout", "60",
                     "--fail-fast"],
                    cwd=root, env={**env, **options.get("repl_env", {})}, stdin=subprocess.PIPE, stdout=transcript,
                    stderr=subprocess.STDOUT, text=True)
                try:
                    console.communicate(source, timeout=90)
                finally:
                    if console.poll() is None:
                        console.kill()
                        console.wait()
                transcript.seek(0)
                output = transcript.read()
            lines = output.splitlines()
            assert console.returncode == 0, output
            assert marker in lines, output
            assert target.poll() is None, output
            for expected in options.get("expect_output", []):
                assert any(expected in line for line in lines), (expected, output)
            record = options.get("record", "HFW::HfwItem")
            assert any(f"reconstructed {record}" in line for line in lines), output
            assert not any(f"skipped {record}" in line for line in lines), output
            assert not any("virtual method definition is unavailable" in line
                           for line in lines), output
        finally:
            if target.poll() is None:
                target.terminate()
                target.wait(timeout=5)
    print(marker)


# What the prepare tool answers to a value that is out of range, with the range.
BAD_LIMITS = (
    (("--follow-depth", "99"), f"--follow-depth must be between 0 and {MAX_FOLLOW_DEPTH}"),
    (("--follow-depth", "deep"), "invalid --follow-depth: deep"),
    (("--string-bytes", "70000"), "--string-bytes must be between 0 and 65536"),
    (("--snapshot-slice", "100"), "--snapshot-slice must be between 4096 and 65536"),
    (("--snapshot-arena", "off"), "invalid --snapshot-arena: off"),
    (("--snapshot-arena", "1000"), "--snapshot-arena must be between 16384 and 4294967295"),
    (("--checked-reads", "0"), "--checked-reads must be between 1 and 4294967295"),
)


def check_validation(args, env):
    for option, expected in BAD_LIMITS:
        result = subprocess.run([str(args.preparer), *option], env=env, capture_output=True,
                                text=True, timeout=30)
        assert result.returncode != 0 and expected in result.stderr, (option, result.stderr)
    # An environment variable is validated the same way.
    result = subprocess.run([str(args.preparer)], env={**env, "TRACER_PREPARE_FOLLOW_DEPTH": "99"},
                            capture_output=True, text=True, timeout=30)
    assert result.returncode != 0 and "TRACER_PREPARE_FOLLOW_DEPTH" in result.stderr, result.stderr
    print("HANDLE_RETURN_VALIDATION=1")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repl", "server", "agent", "preparer", "compiler", "example"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    parser.add_argument("--only", help="comma-separated scenario names, for development")
    args = parser.parse_args()
    only = set(args.only.split(",")) if args.only else None
    env = {key: value for key, value in os.environ.items() if key != "CLANG_TRACER_ENDPOINT"}
    env["TRACER_PREPARE_EXECUTABLE"] = str(args.preparer)
    with tempfile.TemporaryDirectory(prefix="ct-handle-return-") as directory:
        base = Path(directory)
        env["XDG_CACHE_HOME"] = str(base / "cache")
        check_validation(args, env)
        builds = {}
        for name, defines, selector, phase, options in SCENARIOS:
            if only is not None and name not in only:
                continue
            if defines not in builds:
                builds[defines] = base / f"build-{len(builds)}"
                build(args, builds[defines], defines)
            root = builds[defines]
            if not options.get("external"):
                run_session(args, root, env, name, selector, phase, options)
                continue
            server, endpoint = start_external_server(args, root, env)
            try:
                run_session(args, root, env, name, selector, phase, options, endpoint)
            finally:
                if server.poll() is None:
                    server.terminate()
                    try:
                        server.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        server.kill()
                        server.wait()


if __name__ == "__main__":
    main()
