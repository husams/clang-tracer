"""One grammar provides command parsing and token-based live highlighting."""

import ast
from dataclasses import dataclass, field
from importlib.resources import files
from typing import Any

from lark import Lark, Token, Tree, UnexpectedInput
from prompt_toolkit.document import Document
from prompt_toolkit.lexers import Lexer
from prompt_toolkit.validation import ValidationError, Validator

PARSER = Lark(
    files("clang_tracer").joinpath("commands.lark").read_text(),
    parser="lalr",
    propagate_positions=True,
)


class CommandError(ValueError):
    """A syntax or command argument error, suitable for interactive display."""


@dataclass(frozen=True)
class Command:
    name: str
    arguments: tuple[str | int | float, ...] = ()
    options: dict[str, Any] = field(default_factory=dict)


def _value(token: Token) -> str | int | float:
    if token.type == "STRING":
        try:
            return str(ast.literal_eval(str(token)))
        except (SyntaxError, ValueError) as error:
            raise CommandError(
                "invalid string escape; use doubled backslashes in paths"
            ) from error
    if token.type == "INT":
        return int(token)
    if token.type == "NUMBER":
        return float(token)
    if str(token).startswith("--"):
        raise CommandError("quote values beginning with --")
    return str(token)


def parse_command(text: str) -> Command:
    try:
        tree = PARSER.parse(text)
    except UnexpectedInput as error:
        raise CommandError(
            f"invalid command at column {error.column}; type help"
        ) from error
    if not isinstance(tree, Tree):
        raise CommandError("expected a command")
    arguments = []
    options: dict[str, Any] = {}
    for child in tree.children:
        if isinstance(child, Token):
            arguments.append(_value(child))
            continue
        key = str(child.data)
        token = child.children[0] if child.children else None
        if token is not None and not isinstance(token, Token):
            raise CommandError("expected an option value")
        value = _value(token) if token is not None else True
        if key in {"include_headers", "session_ids"}:
            options.setdefault(key, []).append(value)
        elif key in options:
            raise CommandError(f"duplicate option: {key}")
        else:
            options[key] = value
    if tree.data == "inject" and "callback" not in options:
        raise CommandError("probe inject requires --callback FILE")
    return Command(str(tree.data), tuple(arguments), options)


class CommandValidator(Validator):
    def validate(self, document: Document) -> None:
        if not document.text.strip() or document.text.lstrip().startswith("#"):
            return
        try:
            parse_command(document.text)
        except CommandError as error:
            raise ValidationError(
                message=str(error), cursor_position=len(document.text)
            ) from error


class CommandLexer(Lexer):
    def lex_document(self, document: Document):  # type: ignore[no-untyped-def]
        lines = document.lines

        def get_line(index: int) -> list[tuple[str, str]]:
            if index >= len(lines):
                return []
            line = lines[index]
            fragments = []
            position = 0
            try:
                for token in PARSER.lex(line):
                    start = token.start_pos or 0
                    if start > position:
                        fragments.append(("", line[position:start]))
                    style = "class:keyword"
                    if token.type == "STRING":
                        style = "class:string"
                    elif token.type in {"INT", "NUMBER"}:
                        style = "class:number"
                    elif str(token).startswith("--"):
                        style = "class:option"
                    elif token.type == "BARE":
                        style = ""
                    fragments.append((style, str(token)))
                    position = token.end_pos or position
            except UnexpectedInput:
                # A half-typed quote remains highlighted while the user edits it.
                fragments.append(("class:string", line[position:]))
                return fragments
            if position < len(line):
                fragments.append(
                    ("class:comment" if "#" in line[position:] else "", line[position:])
                )
            return fragments

        return get_line
