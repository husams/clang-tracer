"""Interactive console; every operation delegates to the public Python API."""

import argparse
import sys
from pathlib import Path
from typing import Any

import grpc
from google.protobuf import text_format
from prompt_toolkit import PromptSession, print_formatted_text
from prompt_toolkit.completion import NestedCompleter
from prompt_toolkit.formatted_text import FormattedText
from prompt_toolkit.history import InMemoryHistory
from prompt_toolkit.styles import Style

from .client import Client
from .command import (
    Command,
    CommandError,
    CommandLexer,
    CommandValidator,
    parse_command,
)
from .display import render
from .proto import tracer_pb2 as pb

HELP = """server start [--executable PATH] [--agent PATH] [--timeout SECONDS]
             [--operation-timeout 1..300] [--durable-directory PATH]
server status | server stop [--timeout SECONDS]
attach PID                    attach and select the returned session
sessions                      list sessions and probes
use SESSION                   select an attached session
detach [SESSION]              detach a session and restore its probes
probe resolve "C++ selector" [--include HEADER ...] [--compdb PATH]
probe inject "C++ selector" --callback FILE [--include HEADER ...] [--compdb PATH]
             [--stack 0..64] [--internal-frames] [--print-arguments] [--print-result]
             [--package SERVER_PATH]
probe add SPEC.pbtxt           install a full Protobuf ProbeSpec
probe remove PROBE_ID          remove a probe and restore original bytes
events [--count N] [--session ID ...] [--pid PID] [--after CURSOR] [--timeout SECONDS]
collect N [stream options]     collect and display at most N events (default: 30s)
record FILE N [stream options] write binary length-prefixed Protobuf (default: 30s)
help | exit | quit
Quote selectors and paths containing spaces. Ctrl-C cancels a stream or clears
the current input. EOF exits. Exit gracefully stops a server this console launched.
Connected external servers remain running; explicitly detach their sessions.
"""

STYLE = Style.from_dict(
    {
        "keyword": "bold ansicyan",
        "option": "ansiyellow",
        "string": "ansigreen",
        "number": "ansimagenta",
        "comment": "italic ansibrightblack",
        "error": "ansired",
        "prompt": "bold ansicyan",
        "heading": "bold ansicyan",
        "label": "bold",
        "ok": "ansigreen",
        "warning": "ansiyellow",
        "muted": "ansibrightblack",
    }
)


class Console:
    def __init__(self, client: Client, *, output: Any = None) -> None:
        self.client = client
        self.session_id: str | None = None
        self.output = output

    def emit(self, value: Any, *, error: bool = False) -> None:
        print_formatted_text(
            render(value, error=error),
            style=STYLE,
            output=self.output,
        )

    def selected(self) -> str:
        if self.session_id is None:
            raise CommandError("attach a process or use SESSION first")
        return self.session_id

    def execute(self, command: Command) -> bool:
        name, arguments = command.name, command.arguments
        options = dict(command.options)
        if name == "exit":
            return False
        if name == "help":
            self.emit(HELP)
        elif name == "server_start":
            self.emit(self.client.start_server(**options))
        elif name == "server_status":
            self.emit(self.client.status())
        elif name == "server_stop":
            self.client.shutdown_server(**options)
            self.emit("server stopped")
        elif name == "attach":
            session = self.client.attach(int(arguments[0]))
            self.session_id = session.id
            self.emit(session)
        elif name == "sessions":
            sessions = self.client.list_sessions()
            if not sessions:
                self.emit("no sessions")
            for session in sessions:
                self.emit(session)
        elif name == "use":
            selected = str(arguments[0])
            if not any(
                s.id == selected and s.state == pb.Session.ATTACHED
                for s in self.client.list_sessions()
            ):
                raise CommandError("no attached session with that ID")
            self.session_id = selected
            self.emit(f"selected {selected}")
        elif name == "detach":
            selected = str(arguments[0]) if arguments else self.selected()
            self.emit(self.client.detach(selected))
            if selected == self.session_id:
                self.session_id = None
        elif name == "resolve":
            self.emit(
                self.client.resolve_probe(self.selected(), str(arguments[0]), **options)
            )
        elif name == "inject":
            selected = self.selected()
            source = Path(options.pop("callback")).read_text()
            options["include_internal_stack_frames"] = options.pop(
                "internal_frames", False
            )
            self.emit(
                self.client.inject(selected, str(arguments[0]), source, **options)
            )
        elif name == "add_spec":
            selected = self.selected()
            spec = text_format.Parse(
                Path(str(arguments[0])).read_text(), pb.ProbeSpec()
            )
            self.emit(self.client.add_probe(selected, spec))
        elif name == "remove":
            self.emit(self.client.remove_probe(self.selected(), int(arguments[0])))
        elif name in {"events", "collect", "record"}:
            if "session_ids" not in options and self.session_id is not None:
                options["session_ids"] = [self.session_id]
            if name == "collect":
                for event in self.client.collect_traces(int(arguments[0]), **options):
                    self.emit(event)
            elif name == "record":
                count = self.client.record_traces(
                    str(arguments[0]), int(arguments[1]), **options
                )
                self.emit(f"recorded {count} events")
            else:
                count = options.pop("count", None)
                if count is not None and count < 1:
                    raise CommandError("count must be positive")
                with self.client.watch_events(**options) as stream:
                    for i, event in enumerate(stream, 1):
                        self.emit(event)
                        if count is not None and i >= count:
                            break
        else:
            raise CommandError(f"unknown command: {name}")
        return True

    def run(self, prompt: Any = None) -> int:
        if prompt is None:
            prompt = PromptSession(
                lexer=CommandLexer(),
                validator=CommandValidator(),
                validate_while_typing=False,
                style=STYLE,
                history=InMemoryHistory(),
                completer=NestedCompleter.from_nested_dict(
                    {
                        "server": {"start": None, "status": None, "stop": None},
                        "probe": {
                            "inject": None,
                            "resolve": None,
                            "add": None,
                            "remove": None,
                        },
                        **dict.fromkeys(
                            [
                                "attach",
                                "sessions",
                                "use",
                                "detach",
                                "events",
                                "collect",
                                "record",
                                "help",
                                "exit",
                                "quit",
                            ]
                        ),
                    }
                ),
                output=self.output,
            )
        self.emit("Clang Tracer — type help for commands")
        try:
            while True:
                try:
                    selected = f"[{self.session_id}]" if self.session_id else ""
                    line = prompt.prompt(
                        FormattedText([("class:prompt", f"tracer{selected}> ")])
                    )
                    if not line.strip() or line.lstrip().startswith("#"):
                        continue
                    if not self.execute(parse_command(line)):
                        break
                except KeyboardInterrupt:
                    self.emit("cancelled")
                except EOFError:
                    break
                except grpc.RpcError as error:
                    self.emit(f"{error.code().name}: {error.details()}", error=True)
                except (
                    ValueError,
                    OSError,
                    RuntimeError,
                    text_format.ParseError,
                ) as error:
                    self.emit(error, error=True)
        finally:
            self.client.close()
        return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Clang Tracer interactive Python console"
    )
    parser.add_argument("--endpoint", help="server endpoint or CLANG_TRACER_ENDPOINT")
    parser.add_argument(
        "--timeout", type=float, default=30, help="unary RPC deadline in seconds"
    )
    options = parser.parse_args(argv)
    try:
        return Console(Client(options.endpoint, timeout=options.timeout)).run()
    except (ValueError, OSError, RuntimeError) as error:
        print(f"clang-tracer-console: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
