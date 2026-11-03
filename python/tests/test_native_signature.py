"""Real DWARF class import and exact const method binding regressions."""

import os
import platform
import shutil
import subprocess
import tempfile
import time
from pathlib import Path

import grpc
import pytest

from clang_tracer import Client
from clang_tracer import messages as pb

pytestmark = pytest.mark.native

SELECTOR = "signature_fixture::signature_target(signature_fixture::Record&, int)"
CALLBACK = """
template <>
class Interceptor<int(signature_fixture::Record&, int)> {
public:
  void pre(const Call& call) {
    const auto& object = call.argument<0>();
    call.field("score_before", object.score());
  }
  void post(const Call& call) {
    const auto& object = call.argument<0>();
    call.field("score_after", object.score());
    call.field("result", call.result());
  }
};
"""


def assert_success(report):
    assert report.success, report
    assert not any(d.severity in ("error", "fatal") for d in report.diagnostics), report


@pytest.fixture
def signature_client(tmp_path, request):
    configured = os.getenv("CLANG_TRACER_NATIVE_BUILD")
    if not configured or platform.system() != "Linux":
        pytest.skip("requires the Linux native build")
    build = Path(configured).resolve()
    target_binary = build / "signature-interceptor-fixture"
    if getattr(request, "param", None) == "separate_debug":
        objcopy = shutil.which("objcopy")
        assert objcopy, "separate-debug regression requires objcopy"
        target_binary = tmp_path / "signature-interceptor-fixture"
        shutil.copy2(build / "signature-interceptor-fixture", target_binary)
        companion = tmp_path / "signature-interceptor-fixture.debug"
        subprocess.run(
            [objcopy, "--only-keep-debug", str(target_binary), str(companion)],
            check=True,
        )
        subprocess.run([objcopy, "--strip-debug", str(target_binary)], check=True)
        subprocess.run(
            [objcopy, f"--add-gnu-debuglink={companion}", str(target_binary)],
            check=True,
        )
    with (tmp_path / "signature-target.log").open("w+b") as log:
        target = subprocess.Popen(
            [target_binary], stdout=log, stderr=log
        )
        try:
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                assert target.poll() is None, "signature fixture exited"
                if b"SIGNATURE_FIXTURE pid=" in os.pread(log.fileno(), 4096, 0):
                    break
                time.sleep(0.02)
            else:
                pytest.fail("signature fixture did not become ready")
            with tempfile.TemporaryDirectory(prefix="ct-signature-") as sockets:
                with Client(f"unix://{sockets}/control.sock", timeout=60) as client:
                    client.start_server(
                        str(build / "clang-tracer-server"),
                        agent=str(build / "libclang_tracer_agent.so"),
                    )
                    session = client.attach(target.pid)
                    try:
                        yield client, session
                    finally:
                        client.detach(session.id)
        finally:
            target.terminate()
            target.wait(timeout=5)


def test_dwarf_const_method_uses_original_receiver_and_exact_overload(signature_client):
    client, session = signature_client
    for level in ("syntax", "compile"):
        report = client.validate_probe(session.id, SELECTOR, CALLBACK, level=level)
        assert_success(report)
    assert not next(s for s in client.list_sessions() if s.id == session.id).probes
    probe = client.inject(session.id, SELECTOR, CALLBACK)
    events = client.collect_traces(8, session_ids=[session.id], timeout=15)
    pre_by_call = {}
    checked = 0
    for event in events:
        values = {v.name: v.value.signed_value for v in event.object_fields}
        if event.phase == pb.PRE:
            pre_by_call[event.call_id] = values["score_before"]
        elif event.phase == pb.POST and event.call_id in pre_by_call:
            after = values["score_after"]
            assert after == pre_by_call[event.call_id] + 3
            assert values["result"] == ((after - 1) // 3) * 7 + 3
            checked += 1
    assert checked >= 2
    assert client.remove_probe(session.id, probe.id).original_bytes_restored


@pytest.mark.parametrize("signature_client", ["separate_debug"], indirect=True)
def test_matching_separate_debug_file_recovers_class_and_methods(signature_client):
    test_dwarf_const_method_uses_original_receiver_and_exact_overload(signature_client)


def test_single_record_pointer_signature_preserves_pointer_type(signature_client):
    client, session = signature_client
    source = """
template <> class Interceptor<int(signature_fixture::Record*)> {
public:
  void post(const Call& call) {
    const auto* object = call.argument<0>();
    call.field("value", object->value);
    call.field("result", call.result());
  }
};
"""
    probe = client.inject(
        session.id,
        "signature_fixture::record_pointer_single_target(signature_fixture::Record*)",
        source,
    )
    events = client.collect_traces(6, session_ids=[session.id], timeout=15)
    posts = [event for event in events if event.phase == pb.POST]
    assert posts
    for event in posts:
        values = {v.name: v.value.signed_value for v in event.object_fields}
        assert values["value"] == values["result"]
    assert client.remove_probe(session.id, probe.id).original_bytes_restored


def test_inferred_reference_result_borrows_original_object(signature_client):
    client, session = signature_client
    source = """
template <>
class Interceptor<signature_fixture::Record&(signature_fixture::Record&)> {
public:
  void post(const Call& call) {
    static_assert(std::is_same_v<decltype(call.result()),
                               const signature_fixture::Record&>);
    const auto& result = call.result();
    const auto& argument = call.argument<0>();
    call.field("same", &result == &argument ? 1 : 0);
  }
};
"""
    probe = client.inject(
        session.id,
        "signature_fixture::record_reference_return(signature_fixture::Record&)",
        source,
    )
    events = client.collect_traces(6, session_ids=[session.id], timeout=15)
    posts = [event for event in events if event.phase == pb.POST]
    assert posts
    assert all(event.object_fields[0].value.signed_value == 1 for event in posts)
    assert client.remove_probe(session.id, probe.id).original_bytes_restored


@pytest.mark.parametrize(
    "operation", ["object->value = 99;", "object->nonconst_only();"],
)
def test_inferred_pointer_argument_preserves_read_only_observation(
    signature_client, operation,
):
    client, session = signature_client
    source = """
template <> class Interceptor<int(signature_fixture::Record*)> {
public:
  void post(const Call& call) {
    auto object = call.argument<0>();
    OPERATION
  }
};
""".replace("OPERATION", operation)
    report = client.validate_probe(
        session.id,
        "signature_fixture::record_pointer_single_target(signature_fixture::Record*)",
        source,
        level="syntax",
    )
    assert not report.success
    assert any("const" in d.message for d in report.diagnostics), report
    assert not next(s for s in client.list_sessions() if s.id == session.id).probes


def test_recovered_unresolved_method_only_passes_syntax_validation(signature_client):
    client, session = signature_client
    source = CALLBACK.replace("object.score()", "object.unresolved_score()")
    syntax = client.validate_probe(session.id, SELECTOR, source, level="syntax")
    assert_success(syntax)
    compiled = client.validate_probe(session.id, SELECTOR, source, level="compile")
    assert not compiled.success
    assert any("unresolved_score" in d.message for d in compiled.diagnostics), compiled
    assert not next(s for s in client.list_sessions() if s.id == session.id).probes


def test_inferred_pointer_reference_keeps_original_signature_and_const_view(
    signature_client,
):
    client, session = signature_client
    source = """
template <> class Interceptor<int(signature_fixture::Record*&)> {
public:
  void post(const Call& call) {
    static_assert(std::is_same_v<decltype(call.argument<0>()),
                               const signature_fixture::Record* const&>);
    call.field("value", call.argument<0>()->value);
  }
};
"""
    selector = (
        "signature_fixture::record_pointer_reference_target("
        "signature_fixture::Record*&)"
    )
    assert_success(client.validate_probe(session.id, selector, source))
    rejected = client.validate_probe(
        session.id, selector,
        source.replace('call.field("value", call.argument<0>()->value);',
                       "call.argument<0>() = nullptr;"),
        level="syntax",
    )
    assert not rejected.success
    assert any("const" in d.message for d in rejected.diagnostics), rejected
    probe = client.inject(session.id, selector, source)
    events = client.collect_traces(6, session_ids=[session.id], timeout=15)
    assert any(event.phase == pb.POST and event.object_fields for event in events)
    assert client.remove_probe(session.id, probe.id).original_bytes_restored


def test_recovered_nonconst_method_is_rejected_through_borrowed_const_view(
    signature_client,
):
    client, session = signature_client
    source = CALLBACK.replace("object.score()", "object.nonconst_only()")
    report = client.validate_probe(session.id, SELECTOR, source, level="syntax")
    assert not report.success
    assert not next(s for s in client.list_sessions() if s.id == session.id).probes


@pytest.mark.parametrize("reference", [False, True])
def test_inferred_pointer_result_preserves_const_observation(
    signature_client, reference,
):
    client, session = signature_client
    record = "signature_fixture::Record"
    result = f"{record}*{'&' if reference else ''}"
    argument = f"{record}{'*&' if reference else '&'}"
    name = "record_pointer_reference_return_target" if reference else (
        "record_pointer_return_target"
    )
    observed = f"const {record}*{' const&' if reference else ''}"
    source = f"""
template <> class Interceptor<{result}({argument})> {{
public:
  void post(const Call& call) {{
    static_assert(std::is_same_v<decltype(call.result()), {observed}>);
    call.field("value", call.result()->value);
  }}
}};
"""
    selector = f"signature_fixture::{name}({argument})"
    assert_success(client.validate_probe(session.id, selector, source))
    rejected = client.validate_probe(
        session.id, selector,
        source.replace('call.field("value", call.result()->value);',
                       "call.result()->value = 99;"),
        level="syntax",
    )
    assert not rejected.success
    assert any("const" in d.message for d in rejected.diagnostics), rejected
    if reference:
        rejected = client.validate_probe(
            session.id, selector,
            source.replace('call.field("value", call.result()->value);',
                           "call.result() = nullptr;"),
            level="syntax",
        )
        assert not rejected.success
    probe = client.inject(session.id, selector, source)
    events = client.collect_traces(6, session_ids=[session.id], timeout=15)
    assert any(event.phase == pb.POST and event.object_fields for event in events)
    assert client.remove_probe(session.id, probe.id).original_bytes_restored


def test_nonstandard_target_calling_convention_is_rejected(signature_client):
    client, session = signature_client
    with pytest.raises(grpc.RpcError) as failure:
        client.validate_probe(
            session.id, "signature_fixture::AbiRecord::score() const",
            "class Interceptor { public: void post(const Call&) {} };",
        )
    assert "calling convention" in failure.value.details().lower()
    assert not next(s for s in client.list_sessions() if s.id == session.id).probes


@pytest.mark.parametrize("qualifier", ["", "const "])
def test_by_value_record_parameter_ignores_top_level_const(
    signature_client, qualifier,
):
    client, session = signature_client
    source = f"""
template <> class Interceptor<int({qualifier}signature_fixture::Record)> {{
public:
  void post(const Call& call) {{
    static_assert(std::is_same_v<decltype(call.argument<0>()),
                               const signature_fixture::Record&>);
    call.field("value", call.argument<0>().value);
    call.field("result", call.result());
  }}
}};
"""
    candidates = client.list_symbols(
        session.id, query="const_record_value_target",
    ).symbols
    assert len(candidates) == 1
    selector = candidates[0].selector
    assert_success(client.validate_probe(session.id, selector, source))
    probe = client.inject(session.id, selector, source)
    events = client.collect_traces(6, session_ids=[session.id], timeout=15)
    posts = [event for event in events if event.phase == pb.POST]
    assert posts
    for event in posts:
        fields = {field.name: field.value.signed_value for field in event.object_fields}
        assert fields["value"] == fields["result"]
    assert client.remove_probe(session.id, probe.id).original_bytes_restored


def test_unsupported_record_pointer_chain_has_precise_diagnostic(signature_client):
    client, session = signature_client
    source = """
template <> class Interceptor<int(signature_fixture::Record**)> {
public:
  void post(const Call& call) { call.field("result", call.result()); }
};
"""
    report = client.validate_probe(
        session.id,
        "signature_fixture::record_pointer_target(signature_fixture::Record**)",
        source,
    )
    assert not report.success
    assert any("pointer" in d.message.lower() for d in report.diagnostics), report
    assert not next(s for s in client.list_sessions() if s.id == session.id).probes


def test_public_string_target_uses_header_path_and_cpp17_options(signature_client):
    client, session = signature_client
    build = Path(os.environ["CLANG_TRACER_NATIVE_BUILD"]).resolve()
    header = (
        Path(__file__).resolve().parents[2]
        / "tests/fixtures/signature_fixture_api.hpp"
    )
    candidates = client.list_symbols(session.id, query="string_target").symbols
    assert len(candidates) == 1
    selector = candidates[0].selector
    callback = f'''
#include "{header.name}"
template <> class Interceptor<bool(const std::string&)> {{
public:
  void post(const Call& call) {{
    call.field("empty", call.result());
    call.field("size", static_cast<int>(call.argument<0>().size()));
  }}
}};
'''
    options = [
        "-std=c++17",
        f"-I{header.parent}",
        "-D_GLIBCXX_USE_CXX11_ABI=1",
    ]
    report = client.validate_probe(
        session.id,
        selector,
        callback,
        include_headers=[str(header)],
        compilation_database=str(build / "compile_commands.json"),
        compile_options=options,
    )
    assert_success(report)
    assert list(report.effective_options) == options
    assert not next(s for s in client.list_sessions() if s.id == session.id).probes
    wrong_abi = client.validate_probe(
        session.id,
        selector,
        callback,
        include_headers=[str(header)],
        compilation_database=str(build / "compile_commands.json"),
        compile_options=[
            "-std=c++17",
            f"-I{header.parent}",
            "-D_GLIBCXX_USE_CXX11_ABI=0",
        ],
    )
    assert not wrong_abi.success, "validation accepted a mismatched std::string ABI"
    probe = client.inject(
        session.id,
        selector,
        callback,
        include_headers=[str(header)],
        compilation_database=str(build / "compile_commands.json"),
        compile_options=options,
    )
    try:
        events = client.collect_traces(1, session_ids=[session.id], timeout=15)
        assert events
        fields = {field.name: field.value.signed_value
                  for field in events[0].object_fields}
        assert fields["empty"] == 0
        assert fields["size"] == len("signature fixture")
    finally:
        client.remove_probe(session.id, probe.id)


@pytest.mark.parametrize(
    ("selector", "result_type", "fields", "expected"),
    [
        ("signature_fixture::small_return(int)", "SmallReturn",
         ("first", "second"), (11, 12)),
        ("signature_fixture::mixed_return(int)", "MixedReturn",
         ("ratio", "count"), (13.5, 26)),
        ("signature_fixture::large_return(unsigned long)", "LargeReturn",
         ("first", "second", "third"), (17, 18, 19)),
    ],
)
def test_record_return_abi_preserves_post_fields(
    signature_client, selector, result_type, fields, expected
):
    client, session = signature_client
    argument = "uint64_t" if result_type == "LargeReturn" else "int32_t"
    record_fields = "\n".join(
        f'call.field("{name}", value.{name});' for name in fields
    )
    source = f"""
template <>
class Interceptor<signature_fixture::{result_type}({argument})> {{
public:
  void post(const Call& call) {{
    const auto& value = call.result();
    {record_fields}
  }}
}};
"""
    probe = client.inject(session.id, selector, source)
    events = client.collect_traces(6, session_ids=[session.id], timeout=15)
    observed = 0
    for event in events:
        if event.phase != pb.POST:
            continue
        values = {}
        for field in event.object_fields:
            kind = field.value.WhichOneof("data")
            values[field.name] = getattr(field.value, kind)
        assert tuple(values[name] for name in fields) == expected
        observed += 1
    assert observed >= 2
    assert client.remove_probe(session.id, probe.id).original_bytes_restored


def test_boolean_signature_uses_boolean_abi_and_inference(signature_client):
    client, session = signature_client
    source = """
template <> class Interceptor<bool(bool)> {
public:
  void pre(const Call& call) { (void)call.argument<0>(); }
  void post(const Call& call) {
    const bool argument = call.argument<0>();
    const bool result = call.result();
    call.field("argument", static_cast<int>(argument));
    call.field("result", static_cast<int>(result));
  }
};
"""
    probe = client.inject(
        session.id,
        "signature_fixture::boolean_target(bool)",
        source,
        print_arguments=True,
        print_result=True,
    )
    events = client.collect_traces(6, session_ids=[session.id], timeout=15)
    posts = [event for event in events if event.phase == pb.POST]
    assert posts
    pre_events = [event for event in events if event.phase == pb.PRE]
    assert pre_events
    for event in pre_events:
        assert len(event.arguments) == 1
        argument = event.arguments[0]
        assert argument.type == pb.BOOL
        assert argument.WhichOneof("data") == "signed_value"
        assert argument.signed_value in (0, 1)
    for event in posts:
        values = {v.name: v.value.signed_value for v in event.object_fields}
        assert values["result"] == 1 - values["argument"]
        assert event.result.type == pb.BOOL
        assert event.result.WhichOneof("data") == "signed_value"
        assert event.result.signed_value in (0, 1)
    assert client.remove_probe(session.id, probe.id).original_bytes_restored


def test_nontrivial_return_preserves_caller_storage_without_nrvo(signature_client):
    client, session = signature_client
    source = """
template <> class Interceptor<signature_fixture::NontrivialReturn(int)> {
public:
  void post(const Call& call) {
    const auto& object = call.result();
    call.field("value", object.value);
  }
};
"""
    owned = []
    try:
        result_probe = client.inject(
            session.id, "signature_fixture::nontrivial_return(int)", source,
            compile_options=["-std=c++17", "-fno-elide-constructors"],
        )
        owned.append(result_probe)
        counter_source = """
template <> class Interceptor<int()> {
public:
  void post(const Call& call) { call.field("count", call.result()); }
};
"""
        counters = {}
        for name in ("copy", "move", "destruction"):
            probe = client.inject(
                session.id, f"signature_fixture::nontrivial_{name}_count()",
                counter_source,
            )
            owned.append(probe)
            counters[probe.id] = name
        # Earlier probes emit while the remaining probes compile; inspect only
        # invocations after every counter probe has been installed.
        cursor = client.read_events().through_sequence
        events = client.collect_traces(
            64, session_ids=[session.id], after_sequence=cursor, timeout=20
        )
        counts = {"copy": [], "move": [], "destruction": []}
        results = []
        for event in events:
            if event.phase != pb.POST:
                continue
            values = {v.name: v.value.signed_value for v in event.object_fields}
            if event.probe_id == result_probe.id:
                results.append(values["value"])
            elif event.probe_id in counters:
                counts[counters[event.probe_id]].append(values["count"])
        assert results and all(value == 19 for value in results)
        assert counts["copy"] and all(value == 0 for value in counts["copy"])
        assert counts["move"] and all(value == 0 for value in counts["move"])
        destructions = counts["destruction"]
        assert len(destructions) >= 2
        assert all(b - a == 1 for a, b in zip(destructions, destructions[1:]))
    finally:
        for probe in reversed(owned):
            client.remove_probe(session.id, probe.id)
