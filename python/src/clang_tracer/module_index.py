"""Offline ELF and DWARF indexing with the native Clang Tracer tools."""

from __future__ import annotations

import json
import os
from dataclasses import dataclass
from pathlib import Path
import subprocess
from typing import Generic, Iterable, TypeVar

PathLike = str | os.PathLike[str]


@dataclass(frozen=True)
class ModuleSource:
    image: Path
    debug_file: Path | None = None

    @classmethod
    def file(cls, image: PathLike, debug_file: PathLike | None = None) -> ModuleSource:
        return cls(Path(image), None if debug_file is None else Path(debug_file))


@dataclass(frozen=True)
class ModuleIdentity:
    image: str
    sha256: str
    build_id: str
    debug_file: str
    debug_sha256: str
    diagnostics: tuple[str, ...]
    abi: str


@dataclass(frozen=True)
class ModuleField:
    name: str
    type_id: str
    byte_offset: int | None
    bit_offset: int | None
    bit_size: int | None
    base: bool


@dataclass(frozen=True)
class ModuleType:
    id: str
    name: str
    kind: str
    target_id: str
    file: str
    line: int
    size: int | None
    alignment: int | None
    count: int | None
    encoding: int | None
    declaration: bool
    namespace_scope: bool
    fields: tuple[ModuleField, ...]


@dataclass(frozen=True)
class ModuleParameter:
    name: str
    type_id: str


@dataclass(frozen=True)
class ModuleSymbol:
    id: str
    name: str
    selector: str
    linkage: str
    kind: str
    version: str
    type_id: str
    file: str
    diagnostic: str
    address: int
    size: int
    line: int
    exported: bool
    default_version: bool
    has_debug: bool
    member: bool
    parameters: tuple[ModuleParameter, ...]


Entry = TypeVar("Entry", ModuleType, ModuleSymbol)


@dataclass(frozen=True)
class ModulePage(Generic[Entry]):
    entries: tuple[Entry, ...]
    next_page: str


def _type(data: dict) -> ModuleType:
    return ModuleType(**{**data, "fields": tuple(ModuleField(**entry) for entry in data["fields"])})


def _symbol(data: dict) -> ModuleSymbol:
    return ModuleSymbol(
        **{**data, "parameters": tuple(ModuleParameter(**entry) for entry in data["parameters"])}
    )


def _run(executable: str, *args: str) -> str:
    try:
        result = subprocess.run([executable, *args], capture_output=True, text=True, check=True)
    except FileNotFoundError as error:
        raise RuntimeError(f"{executable} was not found in PATH") from error
    except subprocess.CalledProcessError as error:
        raise RuntimeError(error.stderr.strip() or f"{executable} exited with {error.returncode}") from error
    return result.stdout


def _module(*args: str) -> str:
    return _run(os.environ.get("TRACER_MODULE_EXECUTABLE", "clang-tracer-module"), *args)


class ModuleIndex:
    """A reusable index of one Linux x86-64 executable or shared library."""

    def __init__(self, path: Path, identity: ModuleIdentity) -> None:
        self.path = path
        self.identity = identity

    @classmethod
    def build(
        cls, source: ModuleSource, output: PathLike | None = None, *,
        max_entries: int = 0, max_depth: int = 0,
    ) -> ModuleIndex:
        path = Path(f"{source.image}.tracer.sqlite" if output is None else output)
        args = ["index", "--file", str(source.image), "--output", str(path),
                "--max-entries", str(max_entries), "--max-depth", str(max_depth)]
        if source.debug_file is not None:
            args += ["--debug-file", str(source.debug_file)]
        return cls._read_identity(path, _module(*args))

    @classmethod
    def open(cls, path: PathLike = "module-index.sqlite") -> ModuleIndex:
        path = Path(path).resolve(strict=True)
        return cls._read_identity(path, _module("info", "--index", str(path)))

    @classmethod
    def _read_identity(cls, path: Path, output: str) -> ModuleIndex:
        data = json.loads(output)
        data["diagnostics"] = tuple(data["diagnostics"])
        return cls(path.resolve(strict=True), ModuleIdentity(**data))

    def _query(self, command: str, *args: str) -> dict:
        return json.loads(_module(command, "--index", str(self.path), *args))

    def types(self, *, filter: str = "", limit: int = 0, page: str = "") -> ModulePage[ModuleType]:
        data = self._query("types", "--filter", filter, "--limit", str(limit), "--page", page)
        return ModulePage(tuple(_type(entry) for entry in data["entries"]), data["next_page"])

    def symbols(
        self, *, filter: str = "", limit: int = 0, page: str = "", exported_only: bool = False,
    ) -> ModulePage[ModuleSymbol]:
        args = ("--exported",) if exported_only else ()
        data = self._query("symbols", "--filter", filter, "--limit", str(limit), "--page", page, *args)
        return ModulePage(tuple(_symbol(entry) for entry in data["entries"]), data["next_page"])

    def find_type(self, name: str) -> ModuleType:
        return _type(self._query("show", "--type", name))

    def find_symbol(self, selector: str) -> ModuleSymbol:
        return _symbol(self._query("show", "--symbol", selector))

    def get_type(self, id: str) -> ModuleType:
        if id.startswith("s"):
            raise ValueError("expected a type ID")
        return _type(self._query("show", "--id", id))

    def get_symbol(self, id: str) -> ModuleSymbol:
        if not id.startswith("s"):
            raise ValueError("expected a symbol ID")
        return _symbol(self._query("show", "--id", id))

    def emit_headers(
        self, *, symbols: Iterable[str] = (), types: Iterable[str] = (), output: PathLike,
    ) -> Path:
        path = Path(output).absolute()
        args = ["emit", "--index", str(self.path), "--output", str(path)]
        for id in symbols:
            args += ["--symbol", id]
        for id in types:
            args += ["--type", id]
        _module(*args)
        return path

    def prepare_probe(
        self, symbol_id: str, *, output: PathLike, module: PathLike | None = None,
        generated: PathLike | None = None, metadata_only: bool = False,
    ) -> Path:
        symbol = self.get_symbol(symbol_id)
        path = Path(output).absolute()
        args = ["--selector", symbol.selector, "--id", symbol.id,
                "--module-index", str(self.path), "--output", str(path)]
        if module is not None:
            args += ["--module", os.fspath(module)]
        if generated is not None:
            args += ["--generated", os.fspath(generated)]
        if metadata_only:
            args += ["--metadata-only"]
        _run(os.environ.get("TRACER_PREPARE_EXECUTABLE", "clang-tracer-prepare-injectors-native"), *args)
        return path
