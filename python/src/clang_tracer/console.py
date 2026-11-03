"""Interactive console; tracing operations delegate to the public Python API."""

import argparse
import json
import os
import shutil
import sqlite3
import subprocess
import sys
from pathlib import Path
from typing import Any

import grpc
from google.protobuf import text_format
from prompt_toolkit import PromptSession, print_formatted_text
from prompt_toolkit.completion import NestedCompleter
from prompt_toolkit.data_structures import Size
from prompt_toolkit.formatted_text import FormattedText
from prompt_toolkit.history import InMemoryHistory
from prompt_toolkit.output import create_output
from prompt_toolkit.output.color_depth import ColorDepth
from prompt_toolkit.output.plain_text import PlainTextOutput
from prompt_toolkit.output.vt100 import Vt100_Output
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
from .help import help_for
from .proto import tracer_pb2 as pb

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
        "shell-string": "ansigreen",
        "shell-variable": "ansimagenta",
        "help.heading": "bold ansicyan",
        "help.label": "bold ansiyellow",
        "help.text": "",
        "help.usage": "ansicyan",
        "help.option": "ansiyellow",
        "help.example": "ansigreen",
    }
)


def _style_for(color: str) -> Style:
    if color not in {"auto", "always", "never"}:
        raise ValueError("color must be auto, always, or never")
    enabled = color == "always" or (
        color == "auto" and "NO_COLOR" not in os.environ and sys.stdout.isatty()
    )
    return STYLE if enabled else Style.from_dict({})


def _color_depth_for(color: str) -> ColorDepth | None:
    if color == "always":
        return ColorDepth.DEPTH_8_BIT
    if color == "never" or (color == "auto" and "NO_COLOR" in os.environ):
        return ColorDepth.DEPTH_1_BIT
    return None


def _output_for_color(color: str, output: Any) -> Any:
    if output is not None:
        return output
    if color == "always":
        if not sys.stdout.isatty():
            size = shutil.get_terminal_size(fallback=(80, 24))
            return Vt100_Output(
                sys.stdout,
                get_size=lambda: Size(rows=size.lines, columns=size.columns),
                term=os.environ.get("TERM") or "xterm",
                default_color_depth=ColorDepth.DEPTH_8_BIT,
                enable_cpr=False,
            )
    elif not sys.stdout.isatty():
        return PlainTextOutput(sys.stdout)
    return create_output()


def build_completer() -> NestedCompleter:
    return NestedCompleter.from_nested_dict(
        {
            "server": {"start": None, "status": None, "stop": None},
            "probe": {
                "inject": None,
                "resolve": None,
                "validate": None,
                "add": None,
                "remove": None,
            },
            "symbols": {
                "--filter": None,
                "--module": None,
                "--limit": None,
                "--page": None,
            },
            "modules": None,
            "help": {
                "server": {"start": None, "status": None, "stop": None},
                "probe": {
                    "inject": None,
                    "resolve": None,
                    "validate": None,
                    "add": None,
                    "remove": None,
                },
                **dict.fromkeys(
                    [
                        "attach",
                        "sessions",
                        "symbols",
                        "modules",
                        "use",
                        "detach",
                        "events",
                        "collect",
                        "record",
                        "drain",
                        "colors",
                        "!",
                        "help",
                        "shell",
                        "exit",
                        "quit",
                    ]
                ),
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
                    "drain",
                    "exit",
                    "quit",
                ]
            ),
        }
    )


class Console:
    def __init__(
        self,
        client: Client,
        *,
        output: Any = None,
        color: str = "auto",
    ) -> None:
        self.client = client
        self.session_id: str | None = None
        self.style = _style_for(color)
        self.color_depth = _color_depth_for(color)
        self.output = _output_for_color(color, output)

    def emit(self, value: Any, *, error: bool = False) -> None:
        formatted = (
            value if isinstance(value, FormattedText) else render(value, error=error)
        )
        print_formatted_text(
            formatted,
            style=self.style,
            output=self.output,
            color_depth=self.color_depth,
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
            self.emit(help_for(arguments))
        elif name == "shell":
            with subprocess.Popen(["/bin/sh", "-c", str(arguments[0])]) as child:
                interrupted = False
                while True:
                    try:
                        status = child.wait()
                        break
                    except KeyboardInterrupt:
                        interrupted = True
            if interrupted:
                self.emit(f"shell interrupted (status {status})")
            elif status:
                self.emit(f"shell exited with status {status}", error=True)
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
        elif name == "modules":
            modules = self.client.list_modules(self.selected())
            for module in modules:
                self.emit(module)
            if not modules:
                self.emit("no mapped application modules")
        elif name == "symbols":
            page = self.client.list_symbols(self.selected(), **options)
            self.emit(page)
            if page.next_page_token:
                next_command = ["symbols"]
                for option, key in (("--filter", "query"), ("--module", "module")):
                    value = options.get(key, "")
                    if value:
                        quoted = json.dumps(value, ensure_ascii=False)
                        next_command.extend((option, quoted))
                next_command.extend(("--limit", str(options.get("page_size", 0))))
                next_command.extend(
                    ("--page", json.dumps(page.next_page_token, ensure_ascii=False))
                )
                self.emit("next page: " + " ".join(next_command))
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
        elif name == "validate":
            selected = self.selected()
            source = Path(options.pop("callback")).read_text()
            options["level"] = options.pop("validation_level", "compile")
            self.emit(
                self.client.validate_probe(
                    selected, str(arguments[0]), source, **options
                )
            )
        elif name == "add_spec":
            selected = self.selected()
            spec = text_format.Parse(
                Path(str(arguments[0])).read_text(), pb.ProbeSpec()
            )
            self.emit(self.client.add_probe(selected, spec))
        elif name == "remove":
            self.emit(self.client.remove_probe(self.selected(), int(arguments[0])))
        elif name in {"events", "collect", "record", "drain"}:
            if "session_ids" not in options and self.session_id is not None:
                options["session_ids"] = [self.session_id]
            if name == "drain":
                count = self.client.drain_traces(str(arguments[0]), **options)
                self.emit(f"drained {count} events to SQLite")
            elif name == "collect":
                count = int(arguments[0]) if arguments else None
                if count is not None and count < 1:
                    raise CommandError("count must be positive")
                with self.client.watch_events(**options) as stream:
                    for i, event in enumerate(stream, 1):
                        self.emit(event)
                        if count is not None and i >= count:
                            break
            elif name == "record":
                record = (
                    self.client.record_sqlite
                    if options.pop("sqlite", False)
                    else self.client.record_traces
                )
                count = record(str(arguments[0]), int(arguments[1]), **options)
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
                style=self.style,
                color_depth=self.color_depth,
                history=InMemoryHistory(),
                completer=build_completer(),
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
                    sqlite3.Error,
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
    parser.add_argument(
        "--color",
        choices=("auto", "always", "never"),
        default="auto",
        help="color policy (auto respects TTY and NO_COLOR)",
    )
    options = parser.parse_args(argv)
    try:
        return Console(
            Client(options.endpoint, timeout=options.timeout), color=options.color
        ).run()
    except (ValueError, OSError, RuntimeError) as error:
        print(f"clang-tracer-console: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
