import pytest
from prompt_toolkit.document import Document
from prompt_toolkit.validation import ValidationError

from clang_tracer.command import (
    CommandError,
    CommandLexer,
    CommandValidator,
    parse_command,
)


@pytest.mark.parametrize(
    "text,name,arguments,options",
    [
        (
            "server start --agent '/path with spaces/agent.so' --timeout 2.5",
            "server_start",
            (),
            {"agent": "/path with spaces/agent.so", "timeout": 2.5},
        ),
        ("server status", "server_status", (), {}),
        ("server stop --timeout 9", "server_stop", (), {"timeout": 9.0}),
        ("attach 123", "attach", (123,), {}),
        ("sessions", "sessions", (), {}),
        ("modules", "modules", (), {}),
        (
            'probe resolve "add(int, int)" --module target '
            "--debug-max-entries 2000001 --debug-max-depth 256",
            "resolve",
            ("add(int, int)",),
            {"module": "target", "debug_max_entries": 2000001, "debug_max_depth": 256},
        ),
        ("use s1", "use", ("s1",), {}),
        ("detach", "detach", (), {}),
        ("detach 's 1'", "detach", ("s 1",), {}),
        (
            'probe resolve "Calculator::add(int, int)" '
            "--include /srv/a.hpp --include /srv/b.hpp",
            "resolve",
            ("Calculator::add(int, int)",),
            {"include_headers": ["/srv/a.hpp", "/srv/b.hpp"]},
        ),
        (
            'probe inject "add(int, int)" --callback "λ test.cpp" '
            "--stack 24 --print-result",
            "inject",
            ("add(int, int)",),
            {"callback": "λ test.cpp", "stack_depth": 24, "print_result": True},
        ),
        (
            'probe validate "add(int, int)" --callback interceptor.cpp '
            '--level syntax --compile-option "-std=c++17" '
            '--compile-option "-DORDER=1" --include /srv/add.hpp',
            "validate",
            ("add(int, int)",),
            {
                "callback": "interceptor.cpp",
                "validation_level": "syntax",
                "compile_options": ["-std=c++17", "-DORDER=1"],
                "include_headers": ["/srv/add.hpp"],
            },
        ),
        ("probe add spec.pbtxt", "add_spec", ("spec.pbtxt",), {}),
        ("probe remove 7", "remove", (7,), {}),
        (
            "events --session s1 --session s2 --after 42 --count 3 --pid 123",
            "events",
            (),
            {"session_ids": ["s1", "s2"], "after_sequence": 42, "count": 3, "pid": 123},
        ),
        (
            'events --name "add(int, int)" --name trace.add --pid 123',
            "events",
            (),
            {"names": ["add(int, int)", "trace.add"], "pid": 123},
        ),
        (
            'collect 2 --name "add(int, int)" --name trace.add',
            "collect",
            (2,),
            {"names": ["add(int, int)", "trace.add"]},
        ),
        (
            'record capture.pb 2 --name "add(int, int)" --name trace.add',
            "record",
            ("capture.pb", 2),
            {"names": ["add(int, int)", "trace.add"]},
        ),
        ("collect 10 --timeout 3", "collect", (10,), {"timeout": 3.0}),
        ('record "events file.pb" 10', "record", ("events file.pb", 10), {}),
        ("quit # done", "exit", (), {}),
        ("help probe inject", "help", ("probe", "inject"), {}),
        (
            "! printf '%s\\n' \"a b\" | sed 's/b/c/' # keep this comment",
            "shell",
            ("printf '%s\\n' \"a b\" | sed 's/b/c/' # keep this comment",),
            {},
        ),
        ("!  echo $HOME", "shell", (" echo $HOME",), {}),
        (
            "symbols --filter Calculator --module libtarget.so --limit 25 "
            '--page "next token"',
            "symbols",
            (),
            {
                "query": "Calculator",
                "module": "libtarget.so",
                "page_size": 25,
                "page_token": "next token",
            },
        ),
    ],
)
def test_grammar(text, name, arguments, options):
    command = parse_command(text)
    assert (command.name, command.arguments, command.options) == (
        name,
        arguments,
        options,
    )


@pytest.mark.parametrize(
    "text",
    [
        "",
        "unknown",
        "attach -1",
        "attach 12 trailing",
        "attach 1; sessions",
        'probe inject "add(int)"',
        'probe validate "add(int)"',
        'probe inject "unterminated',
        "events --typo 3",
        "events --pid 1 --pid 2",
        "collect 1 --count 3",
        "server stop --agent x",
        "server start --agent --timeout 3",
        r'probe add "C:\Users\invalid"',
        "!",
        "!   ",
    ],
)
def test_invalid_command_has_no_fallback_execution(text):
    with pytest.raises(CommandError):
        parse_command(text)


def test_escaped_quotes():
    assert parse_command(r'probe add "a\"b.pbtxt"').arguments == ('a"b.pbtxt',)


def test_validator_reports_error():
    with pytest.raises(ValidationError):
        CommandValidator().validate(Document("attach nope"))
    CommandValidator().validate(Document("# comment"))


def test_live_highlighting_includes_tokens_and_incomplete_strings():
    line = 'probe inject "add(int)" --callback test.cpp --stack 24 # demo'
    fragments = CommandLexer().lex_document(Document(line))(0)
    assert "".join(text for _, text in fragments) == line
    assert {style for style, _ in fragments} >= {
        "class:keyword",
        "class:option",
        "class:string",
        "class:number",
        "class:comment",
    }
    incomplete = 'probe inject "add('
    assert (
        "".join(
            text for _, text in CommandLexer().lex_document(Document(incomplete))(0)
        )
        == incomplete
    )


def test_shell_highlighting_preserves_source_and_highlights_shell_tokens():
    line = "! printf '%s' \"$HOME\" # demo"
    fragments = CommandLexer().lex_document(Document(line))(0)
    assert "".join(text for _, text in fragments) == line
    assert {style for style, _ in fragments} >= {
        "class:keyword",
        "class:shell-string",
        "class:shell-variable",
        "class:comment",
    }
