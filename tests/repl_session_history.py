"""Verify that finished sessions never block an attach once the server history is full."""
import argparse
import os
import re
import subprocess

SESSION_LIMIT = 64


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repl", "server", "target"):
        parser.add_argument("--" + name, required=True)
    args = parser.parse_args()
    cycles = SESSION_LIMIT + 3
    target = subprocess.Popen([args.target], stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    try:
        commands = f"%attach {target.pid}\n%detach\n" * cycles
        # The next attach evicts one more finished session and stays attached.
        commands += f"%attach {target.pid}\n%sessions\n%detach\n%sessions\n%quit\n"
        # Use the server under test even when the caller's shell names a live endpoint.
        env = {name: value for name, value in os.environ.items() if name != "CLANG_TRACER_ENDPOINT"}
        result = subprocess.run([args.repl, "--server", args.server, "--timeout", "60", "--fail-fast"],
                                input=commands, text=True, capture_output=True, timeout=540, env=env)
        assert result.returncode == 0, (result.stdout[-4000:], result.stderr[-4000:])
        assert target.poll() is None, "attach cycles stopped the target"
        attached = re.findall(r"^attached session=(\S+) pid=\d+$", result.stdout, re.M)
        assert len(attached) == cycles + 1, (len(attached), result.stdout[-4000:])
        assert "limit reached" not in result.stdout + result.stderr

        listed = re.findall(r"^([* ]) (\S+) pid=(\d+) state=(\w+) probes=\d+", result.stdout, re.M)
        live, final = listed[:SESSION_LIMIT], listed[SESSION_LIMIT:]
        assert len(live) == len(final) == SESSION_LIMIT, (len(listed), result.stdout[-4000:])
        ids = {entry[1] for entry in live}
        # The oldest sessions leave first; the newest stay listed.
        assert not ids & set(attached[:cycles + 1 - SESSION_LIMIT]), "oldest history was retained"
        assert ids == set(attached[-SESSION_LIMIT:]), "recent history was evicted"
        assert [e for e in live if e[3] == "ATTACHED"] == [("*", attached[-1], str(target.pid), "ATTACHED")], live
        assert all(e[0] == " " and e[3] == "DETACHED" for e in final), final
    finally:
        target.kill()
        target.wait()
    print(f"{cycles + 1} attaches succeeded with a {SESSION_LIMIT}-session history")


if __name__ == "__main__":
    main()
