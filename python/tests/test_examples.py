import importlib.util
import sys
from pathlib import Path
from types import SimpleNamespace

import pytest

EXAMPLES = Path(__file__).resolve().parents[2] / "examples" / "python"
EXAMPLE_NAMES = ["trace", "stream", "record", "server_lifecycle"]
if not EXAMPLES.is_dir():
    pytest.skip("requires repository examples", allow_module_level=True)


def load_example(name: str):
    spec = importlib.util.spec_from_file_location(name, EXAMPLES / f"{name}.py")
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def example_args(name: str, output: Path) -> list[str]:
    if name == "server_lifecycle":
        return [f"{name}.py"]
    if name == "record":
        return [f"{name}.py", "321", str(output)]
    return [f"{name}.py", "321"]


@pytest.mark.parametrize("name", EXAMPLE_NAMES)
@pytest.mark.parametrize("mode", ["normal", "timeout", "external"])
def test_examples_keep_temp_endpoint_until_managed_server_exits(
    name, mode, tmp_path, monkeypatch, capsys
):
    module = load_example(name)
    external = mode == "external" and name != "server_lifecycle"
    shutdown_times_out = mode == "timeout"
    temporary_directories = []
    managers = []

    class FakeTemporaryDirectory:
        def __init__(self, *, prefix, delete):
            assert delete is False
            self.name = str(tmp_path / f"{prefix}{len(temporary_directories)}")
            Path(self.name).mkdir()
            temporary_directories.append(self.name)

        def __enter__(self):
            return self.name

        def __exit__(self, *_args):
            return None

    class FakeManager:
        pid = 456

        def __init__(self, *_args, **_kwargs):
            self.running = False
            managers.append(self)

        def start(self, *_args, **_kwargs):
            self.running = True

        def shutdown(self, **_kwargs):
            if shutdown_times_out:
                raise RuntimeError("graceful shutdown timed out")
            self.running = False

    class FakeStream:
        after_sequence = 0

        def __iter__(self):
            return iter(())

        def __enter__(self):
            return self

        def __exit__(self, *_args):
            return None

    class FakeClient:
        def __init__(self, endpoint):
            self.endpoint = endpoint
            self.server = None

        def __enter__(self):
            return self

        def __exit__(self, *_args):
            self.close()

        def start_server(self):
            self.server = FakeManager()
            self.server.start()

        def close(self):
            if self.server is not None:
                self.server.shutdown()

        def attach(self, pid):
            return SimpleNamespace(id="session-1", pid=pid)

        def list_sessions(self):
            return []

        def inject(self, *_args, **_kwargs):
            return SimpleNamespace(id=7)

        def collect_traces(self, *_args, **_kwargs):
            return []

        def resolve_probe(self, *_args, **_kwargs):
            from clang_tracer import messages

            return messages.ProbeSpec()

        def add_probe(self, *_args, **_kwargs):
            return SimpleNamespace(id=8)

        def watch_events(self, *_args, **_kwargs):
            return FakeStream()

        def record_traces(self, path, *_args, **_kwargs):
            Path(path).open("xb").close()
            return 0

        def remove_probe(self, *_args, **_kwargs):
            return None

        def detach(self, *_args, **_kwargs):
            return None

        def status(self):
            return SimpleNamespace(running=True, managed=False)

    monkeypatch.setattr(module.tempfile, "TemporaryDirectory", FakeTemporaryDirectory)
    monkeypatch.setattr(module, "Client", FakeClient)
    if name == "server_lifecycle":
        monkeypatch.setattr(module, "ServerProcess", FakeManager)
    arguments = example_args(name, tmp_path / "events.pb")
    if external:
        arguments.extend(["--endpoint", "unix:///external/control.sock"])
    monkeypatch.setattr(sys, "argv", arguments)

    if shutdown_times_out:
        with pytest.raises(RuntimeError, match="timed out"):
            module.main()
        assert managers and managers[0].running
        assert Path(temporary_directories[0]).is_dir()
        error = capsys.readouterr().err
        assert "PID 456" in error
        assert "recovery endpoint:" in error
        assert temporary_directories[0] in error
    else:
        module.main()
        if external:
            assert not managers
        else:
            assert managers and not managers[0].running
        assert not Path(temporary_directories[0]).exists()


@pytest.mark.parametrize("name", EXAMPLE_NAMES)
def test_examples_remove_temp_endpoint_when_client_or_manager_construction_fails(
    name, tmp_path, monkeypatch
):
    module = load_example(name)
    created = []

    class FakeTemporaryDirectory:
        def __init__(self, *, prefix, delete):
            assert delete is False
            self.name = str(tmp_path / f"{prefix}failure")
            Path(self.name).mkdir()
            created.append(self.name)

        def __enter__(self):
            return self.name

        def __exit__(self, *_args):
            return None

    def fail_construction(*_args, **_kwargs):
        raise RuntimeError("construction failed")

    monkeypatch.setattr(module.tempfile, "TemporaryDirectory", FakeTemporaryDirectory)
    monkeypatch.setattr(sys, "argv", example_args(name, tmp_path / "events.pb"))
    if name == "server_lifecycle":
        monkeypatch.setattr(module, "ServerProcess", fail_construction)
    else:
        monkeypatch.setattr(module, "Client", fail_construction)

    with pytest.raises(RuntimeError, match="construction failed"):
        module.main()
    assert len(created) == 1
    assert not Path(created[0]).exists()
