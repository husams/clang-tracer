"""End-to-end preparation must not activate probes or execute callback code."""

import grpc
import pytest
from test_native import native as native

from clang_tracer import Client

pytestmark = pytest.mark.native

CALLBACK = """
template <> class Interceptor<int(int, int)> {
public:
  void post(const Call& call) {
    call.field("sum", call.result());
  }
};
"""


@pytest.fixture
def proposal_client(native):
    build, target, temporary = native
    with Client(f"unix://{temporary}/proposal.sock", timeout=60) as client:
        client.start_server(
            str(build / "clang-tracer-server"),
            agent=str(build / "libclang_tracer_agent.so"),
        )
        session = client.attach(target.pid)
        try:
            yield client, session, temporary
        finally:
            client.detach(session.id)


def assert_no_probes(client, session):
    current = next(s for s in client.list_sessions() if s.id == session.id)
    assert not current.probes


def assert_success(report):
    assert report.success, report
    assert not any(d.severity in ("error", "fatal") for d in report.diagnostics), report


def test_validate_syntax_and_compile_leave_target_uninstrumented(proposal_client):
    client, session, _temporary = proposal_client
    sources = (CALLBACK, CALLBACK.replace("Interceptor<", "Interceptor /* gap */ <"))
    for level in ("syntax", "compile"):
        for source in sources:
            report = client.validate_probe(
                session.id, "add(int, int)", source, level=level
            )
            assert_success(report)
            assert_no_probes(client, session)


def test_shared_context_validation_does_not_activate_a_probe(proposal_client):
    client, session, _temporary = proposal_client
    source = """
class Interceptor {
public:
  void pre(const Call& call) { call.context().event("preflight.only"); }
};
"""
    for level in ("syntax", "compile"):
        report = client.validate_probe(
            session.id, "add(int, int)", source, level=level
        )
        assert_success(report)
        assert_no_probes(client, session)
        assert not client.read_events().events


def test_validate_reports_locations_and_rejects_signature_mismatch(proposal_client):
    client, session, _temporary = proposal_client
    report = client.validate_probe(
        session.id, "add(int, int)", CALLBACK.replace('call.field("sum",', "??"),
        level="syntax",
    )
    assert not report.success
    assert any(d.file and d.line and d.column for d in report.diagnostics)
    mismatch = CALLBACK.replace("int(int, int)", "long long(int, int)")
    report = client.validate_probe(session.id, "add(int, int)", mismatch)
    assert not report.success
    with pytest.raises(grpc.RpcError):
        client.inject(session.id, "add(int, int)", mismatch)
    assert_no_probes(client, session)


def test_validate_compile_detects_unresolved_external(proposal_client):
    client, session, _temporary = proposal_client
    source = "extern int missing_proposal_symbol_7941();\n" + CALLBACK.replace(
        "call.result()", "missing_proposal_symbol_7941()"
    )
    syntax = client.validate_probe(
        session.id, "add(int, int)", source, level="syntax"
    )
    assert_success(syntax)
    compiled = client.validate_probe(
        session.id, "add(int, int)", source, level="compile"
    )
    assert not compiled.success
    assert any(
        "missing_proposal_symbol_7941" in d.message for d in compiled.diagnostics
    ), compiled
    assert_no_probes(client, session)


def test_validate_compile_does_not_execute_global_initializers(proposal_client):
    client, session, temporary = proposal_client
    marker = temporary / "initializer-ran"
    source = f"""
#include <cstdio>
int proposal_initializer = []() {{
  auto* file = std::fopen("{marker}", "w");
  if (file) std::fclose(file);
  return 1;
}}();
""" + CALLBACK
    report = client.validate_probe(session.id, "add(int, int)", source)
    assert_success(report)
    assert not marker.exists(), "validation executed a user initializer"
    assert_no_probes(client, session)


def test_validate_forced_header_does_not_execute_initializers(proposal_client):
    client, session, temporary = proposal_client
    marker = temporary / "forced-initializer-ran"
    header = temporary / "forced_config.hpp"
    header.write_text(f"""
#pragma once
#include <cstdio>
int forced_initializer = []() {{
  auto* file = std::fopen("{marker}", "w");
  if (file) std::fclose(file);
  return 1;
}}();
""")
    for level in ("syntax", "compile"):
        report = client.validate_probe(
            session.id, "add(int, int)", CALLBACK,
            compile_options=["-include", str(header)], level=level,
        )
        assert not marker.exists(), "forced header executed during validation"
        assert_success(report)
        assert_no_probes(client, session)


def test_forced_header_abi_applies_before_cached_standard_headers(proposal_client):
    client, session, temporary = proposal_client
    # Warm the ordinary common-header profile before choosing a different ABI
    # through a forced header, whose text precedes all tracer API declarations.
    assert_success(client.validate_probe(session.id, "add(int, int)", CALLBACK))
    header = temporary / "old_string_abi.hpp"
    header.write_text("#define _GLIBCXX_USE_CXX11_ABI 0\n")
    source = (
        """
#include <string>
static_assert(sizeof(std::string) == sizeof(void*),
              "forced header was applied after standard headers");
"""
        + CALLBACK
    )
    options = ["-include", str(header)]
    for level in ("syntax", "compile"):
        assert_success(
            client.validate_probe(
                session.id,
                "add(int, int)",
                source,
                compile_options=options,
                level=level,
            )
        )
        assert_no_probes(client, session)
    probe = client.inject(session.id, "add(int, int)", source, compile_options=options)
    events = client.collect_traces(4, session_ids=[session.id], timeout=10)
    assert events and all(event.probe_id == probe.id for event in events)
    assert client.remove_probe(session.id, probe.id).original_bytes_restored


def test_validate_and_inject_share_cpp17_include_options(proposal_client):
    client, session, temporary = proposal_client
    (temporary / "proposal_config.hpp").write_text(
        "#if PROPOSAL_FEATURE != 17\n#error Missing build definition\n#endif\n"
    )
    source = '#include "proposal_config.hpp"\n' + CALLBACK
    options = [f"-I{temporary}", "-DPROPOSAL_FEATURE=17", "-std=c++17"]
    report = client.validate_probe(
        session.id, "add(int, int)", source, compile_options=options
    )
    assert_success(report)
    assert "-std=c++17" in report.effective_options
    probe = client.inject(
        session.id, "add(int, int)", source, compile_options=options
    )
    assert probe.active
    events = client.collect_traces(2, session_ids=[session.id], timeout=10)
    assert events
    client.remove_probe(session.id, probe.id)
