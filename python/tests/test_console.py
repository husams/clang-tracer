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


def test_emit_formats_protobuf_messages_for_people(monkeypatch):
    from clang_tracer import console as console_module

    captured = {}
    monkeypatch.setattr(
        console_module,
        "print_formatted_text",
        lambda formatted, **_kwargs: captured.update(
            text="".join(t for _, t in formatted)
        ),
    )
    console = Console(Mock(), output=DummyOutput())
    console.emit(
        pb.TraceEvent(
            phase=pb.POST, sequence=8, result=pb.Value(type=pb.INT32, signed_value=0)
        )
    )
    assert "trace POST" in captured["text"]
    assert "sequence=8" in captured["text"]
    assert "result int32 0" in captured["text"]
    assert "phase: POST" not in captured["text"]


def test_bad_use_preserves_selection():
    client = Mock()
    client.list_sessions.return_value = [pb.Session(id="s2", state=pb.Session.DETACHED)]
    console = Console(client, output=DummyOutput())
    console.session_id = "s1"
    with pytest.raises(CommandError, match="no attached session"):
        console.execute(parse_command("use s2"))
    assert console.session_id == "s1"


@pytest.mark.parametrize(
    "topic,expected",
    [
        ("server", "server start"),
        ("probe inject", "--callback FILE"),
        ("events", "--after CURSOR"),
        ("shell", "/bin/sh -c"),
    ],
)
def test_detailed_help_topics_are_formatted_and_colored(monkeypatch, topic, expected):
    from clang_tracer import console as console_module

    output = {}
    monkeypatch.setattr(
        console_module,
        "print_formatted_text",
        lambda formatted, **_kwargs: output.update(
            text="".join(text for _, text in formatted),
            styles={style for style, _ in formatted},
        ),
    )
    console = Console(Mock(), output=DummyOutput(), color="always")
    assert console.execute(parse_command(f"help {topic}"))
    assert expected in output["text"]
    assert "Examples:" in output["text"]
    assert any(style.startswith("class:help.") for style in output["styles"])


def test_help_examples_are_valid_console_commands():
    from clang_tracer.help import HELP_TOPICS

    for topic in HELP_TOPICS.values():
        for example in topic.examples:
            assert parse_command(example)


def test_help_topics_complete_after_help_prefix():
    from prompt_toolkit.completion import CompleteEvent
    from prompt_toolkit.document import Document

    from clang_tracer.console import build_completer

    completions = {
        item.text
        for item in build_completer().get_completions(
            Document("help "), CompleteEvent()
        )
    }
    assert completions >= {
        "server",
        "probe",
        "attach",
        "events",
        "colors",
        "!",
        "help",
        "quit",
    }
    probe_completions = {
        item.text
        for item in build_completer().get_completions(
            Document("help probe "), CompleteEvent()
        )
    }
    assert probe_completions >= {"inject", "resolve", "add", "remove"}


def test_shell_escape_runs_exact_payload_in_inherited_terminal(monkeypatch):
    from clang_tracer import console as console_module

    calls = []

    class Child:
        def __enter__(self):
            return self

        def __exit__(self, *_args):
            return None

        def wait(self):
            return 0

    monkeypatch.setattr(
        console_module.subprocess,
        "Popen",
        lambda args, **kwargs: calls.append((args, kwargs)) or Child(),
    )
    console = Console(Mock(), output=DummyOutput())
    payload = "printf '%s' \"$HOME\" | sed 's/a/b/' # keep comment"
    assert console.execute(parse_command(f"! {payload}"))
    assert calls == [(["/bin/sh", "-c", payload], {})]


def test_shell_child_output_is_inherited_and_nonzero_status_is_reported(
    capfd, monkeypatch
):
    from clang_tracer import console as console_module

    messages = []
    monkeypatch.setattr(
        console_module,
        "print_formatted_text",
        lambda formatted, **_kwargs: messages.append("".join(t for _, t in formatted)),
    )
    console = Console(Mock(), output=DummyOutput())
    assert console.execute(parse_command("! printf 'shell-output'"))
    assert "shell-output" in capfd.readouterr().out
    assert console.execute(parse_command("! exit 7"))
    assert any("status 7" in message for message in messages)


def test_shell_interrupt_reaps_child_without_forced_kill(monkeypatch):
    from clang_tracer import console as console_module

    output = []

    class Child:
        waits = 0

        def __enter__(self):
            return self

        def __exit__(self, *_args):
            return None

        def wait(self):
            self.waits += 1
            if self.waits == 1:
                raise KeyboardInterrupt
            return 130

        def terminate(self):
            raise AssertionError("shell child must not be force-terminated")

        def kill(self):
            raise AssertionError("shell child must not be killed")

    monkeypatch.setattr(console_module.subprocess, "Popen", lambda *_a, **_k: Child())
    monkeypatch.setattr(
        console_module,
        "print_formatted_text",
        lambda formatted, **_kwargs: output.append("".join(t for _, t in formatted)),
    )
    console = Console(Mock(), output=DummyOutput())
    assert console.execute(parse_command("! sleep 1"))
    assert any("interrupted" in message for message in output)


def test_shell_input_and_cli_color_policy(monkeypatch):
    from clang_tracer.console import STYLE

    monkeypatch.setenv("NO_COLOR", "1")
    assert (
        Console(Mock(), color="auto")
        .style.get_attrs_for_style_str("class:heading")
        .color
        == ""
    )
    assert (
        Console(Mock(), color="never")
        .style.get_attrs_for_style_str("class:heading")
        .color
        == ""
    )
    assert (
        Console(Mock(), color="always")
        .style.get_attrs_for_style_str("class:heading")
        .color
        == STYLE.get_attrs_for_style_str("class:heading").color
    )


def test_color_always_emits_ansi_when_stdout_is_redirected(monkeypatch):
    import io

    stream = io.StringIO()
    monkeypatch.setattr("sys.stdout", stream)
    console = Console(Mock(), color="always")
    from prompt_toolkit.formatted_text import FormattedText

    console.emit(FormattedText([("class:heading", "colored")]))
    assert "\x1b[" in stream.getvalue()


def test_color_always_overrides_no_color_on_a_terminal(monkeypatch):
    import io

    from prompt_toolkit.formatted_text import FormattedText

    class TTY(io.StringIO):
        def isatty(self):
            return True

    stream = TTY()
    monkeypatch.setattr("sys.stdout", stream)
    monkeypatch.setenv("NO_COLOR", "1")
    monkeypatch.setenv("TERM", "xterm-256color")
    console = Console(Mock(), color="always")
    console.emit(FormattedText([("class:heading", "colored")]))
    assert "\x1b[" in stream.getvalue()
    assert "colored" in stream.getvalue()


def test_color_never_keeps_terminal_output_capabilities(monkeypatch):
    import io

    class TTY(io.StringIO):
        def isatty(self):
            return True

    stream = TTY()
    monkeypatch.setattr("sys.stdout", stream)
    console = Console(Mock(), color="never")
    assert "PlainTextOutput" not in type(console.output).__name__
    console.emit("plain")
    assert "plain" in stream.getvalue()
    assert "\x1b[0;36;1m" not in stream.getvalue()
