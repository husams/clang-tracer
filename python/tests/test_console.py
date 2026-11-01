from unittest.mock import Mock

import pytest
from prompt_toolkit import PromptSession
from prompt_toolkit.input import create_pipe_input
from prompt_toolkit.output import DummyOutput

from clang_tracer import messages as pb
from clang_tracer.command import CommandError, CommandLexer, parse_command
from clang_tracer.console import Console


def test_repl_uses_api_and_binary_transport(peer, tmp_path):
    client, service = peer
    callback = tmp_path / "interceptor.cpp"
    callback.write_text("struct Interceptor {};")
    console = Console(client, output=DummyOutput())
    for command in [
        "attach 123",
        "sessions",
        "use s1",
        'probe resolve "add(int,int)"',
        f'probe inject "add(int,int)" --callback "{callback}" --stack 12',
        "events --count 2",
        "collect 2",
        f'record "{tmp_path / "e.pb"}" 2',
        "probe remove 7",
        "detach",
    ]:
        assert console.execute(parse_command(command))
    assert console.session_id is None
    assert any(isinstance(request, pb.AddProbeRequest) for request in service.requests)


def test_real_prompt_session_accepts_commands_and_eof(peer):
    client, _ = peer
    with create_pipe_input() as pipe:
        prompt = PromptSession(input=pipe, output=DummyOutput(), lexer=CommandLexer())
        pipe.send_text("attach 123\nsessions\nexit\n")
        assert Console(client, output=DummyOutput()).run(prompt) == 0


def test_prompt_error_and_interrupt_recovery_and_cleanup():
    client = Mock()
    prompt = Mock()
    prompt.prompt.side_effect = ["nonsense", KeyboardInterrupt(), "help", EOFError()]
    assert Console(client, output=DummyOutput()).run(prompt) == 0
    client.close.assert_called_once()
    client.attach.assert_not_called()


def test_requires_selected_session_before_reading_callback(tmp_path):
    client = Mock()
    console = Console(client, output=DummyOutput())
    with pytest.raises(CommandError, match="attach"):
        console.execute(parse_command('probe inject "add(int)" --callback missing.cpp'))
    client.inject.assert_not_called()


def test_server_commands_share_python_runtime():
    client = Mock()
    console = Console(client, output=DummyOutput())
    console.execute(
        parse_command("server start --executable /srv/server --agent /srv/agent.so")
    )
    client.start_server.assert_called_once_with(
        executable="/srv/server", agent="/srv/agent.so"
    )
    console.execute(parse_command("server status"))
    client.status.assert_called_once()
    console.execute(parse_command("server stop --timeout 4"))
    client.shutdown_server.assert_called_once_with(timeout=4.0)


def test_bad_use_preserves_selection():
    client = Mock()
    client.list_sessions.return_value = [pb.Session(id="s2", state=pb.Session.DETACHED)]
    console = Console(client, output=DummyOutput())
    console.session_id = "s1"
    with pytest.raises(CommandError, match="no attached session"):
        console.execute(parse_command("use s2"))
    assert console.session_id == "s1"
