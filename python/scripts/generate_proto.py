"""Regenerate checked-in bindings; run from any working directory."""

from pathlib import Path

from grpc_tools import protoc


def generate() -> None:
    root = Path(__file__).resolve().parents[2]
    output = root / "python/src/clang_tracer/proto"
    result = protoc.main(
        [
            "grpc_tools.protoc",
            f"-I{root / 'proto'}",
            f"--python_out={output}",
            f"--pyi_out={output}",
            f"--grpc_python_out={output}",
            str(root / "proto/context_transport.proto"),
            str(root / "proto/tracer.proto"),
        ]
    )
    if result:
        raise RuntimeError(f"protoc failed: {result}")
    for path in output.iterdir():
        if path.suffix not in {".py", ".pyi"}:
            continue
        text = path.read_text()
        for module in ("context_transport_pb2", "tracer_pb2"):
            text = text.replace(f"import {module} as ", f"from . import {module} as ")
            text = text.replace(
                f"DESCRIPTOR, '{module}',",
                f"DESCRIPTOR, 'clang_tracer.proto.{module}',",
            )
        path.write_text(text)


if __name__ == "__main__":
    generate()
