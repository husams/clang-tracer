"""Topic-based, syntax-highlightable help for the interactive console."""

from collections.abc import Iterable
from dataclasses import dataclass

from prompt_toolkit.formatted_text import FormattedText


@dataclass(frozen=True)
class HelpTopic:
    title: str
    summary: str
    usage: tuple[str, ...] = ()
    options: tuple[tuple[str, str], ...] = ()
    examples: tuple[str, ...] = ()
    notes: tuple[str, ...] = ()


HELP_TOPICS = {
    "": HelpTopic(
        "Clang Tracer console",
        "Control a local or remote tracer, attach processes, add probes, "
        "and read events.",
        usage=(
            "server start|status|stop",
            "attach PID | sessions | use SESSION | detach [SESSION]",
            "modules | symbols [OPTIONS]",
            "probe resolve|inject|add|remove ...",
            "events [OPTIONS] | collect COUNT [OPTIONS] | record FILE COUNT [OPTIONS]",
            "drain DATABASE [--listen] [--count N] [STREAM_OPTIONS]",
            "help [TOPIC] | ! SHELL_COMMAND | exit",
        ),
        options=(
            ("help TOPIC", "Show detailed help for a command or group."),
            ("! COMMAND", "Run on the console host in /bin/sh; see help shell."),
            ("--color MODE", "Choose auto, always, or never; see help colors."),
        ),
        examples=(
            "help probe inject",
            "help server",
            "help events",
            "help !",
            "! pwd",
        ),
        notes=(
            "Quote selectors and paths containing spaces. # starts a console "
            "comment except inside ! shell text.",
            "Ctrl-C clears input or cancels a live stream. Exit stops only a "
            "server launched by this console.",
        ),
    ),
    "server": HelpTopic(
        "Server commands",
        "Start and inspect a local server owned by this console, or inspect "
        "an existing endpoint.",
        usage=(
            "server start [OPTIONS]",
            "server status",
            "server stop [--timeout SECONDS]",
        ),
        options=(
            (
                "start --executable PATH",
                "Server executable; default clang-tracer-server from PATH.",
            ),
            (
                "start --agent PATH",
                "Agent library; default uses the installed or adjacent build library.",
            ),
            ("start --timeout SECONDS", "Readiness deadline; default 30 seconds."),
            (
                "start --operation-timeout SECONDS",
                "Native operation timeout, 1–300; default 10.",
            ),
            (
                "start --durable-directory PATH",
                "Enable durable shared-context recording.",
            ),
            ("start --event-history-limit N", "Retained event count; default 4096."),
            ("start --context-history-limit N", "Context event count; default 128."),
            ("start --context-history-bytes N", "Serialized context-byte budget."),
            ("stop --timeout SECONDS", "Graceful shutdown wait; default 30 seconds."),
        ),
        examples=("server start", "server status", "server stop --timeout 45"),
        notes=(
            "This console never adopts or stops an independently started server.",
            "A shutdown timeout retains the managed child for a later wait; "
            "it is not force-killed.",
        ),
    ),
    "server start": HelpTopic(
        "server start",
        "Launch and own a local native server, then wait for the endpoint "
        "to become ready.",
        usage=("server start [--executable PATH] [--agent PATH] [--timeout SECONDS]",),
        options=(
            (
                "--executable PATH",
                "Executable on this host; default clang-tracer-server.",
            ),
            (
                "--agent PATH",
                "Agent library on this host; default uses the installed or "
                "adjacent build library.",
            ),
            ("--timeout SECONDS", "Readiness deadline; default 30."),
            (
                "--operation-timeout SECONDS",
                "Native operation timeout from 1 to 300; default 10.",
            ),
            ("--durable-directory PATH", "Enable durable context recording."),
            ("--event-history-limit N", "Retained event count; default 4096."),
            ("--context-history-limit N", "Retained context count; default 128."),
            (
                "--context-history-bytes N",
                "Serialized context bytes; default 16777216.",
            ),
        ),
        examples=("server start --timeout 45",),
    ),
    "server status": HelpTopic(
        "server status",
        "Report endpoint readiness and whether this client owns the server process.",
        usage=("server status",),
        examples=("server status",),
    ),
    "server stop": HelpTopic(
        "server stop",
        "Request graceful shutdown of a server launched by this console.",
        usage=("server stop [--timeout SECONDS]",),
        options=(("--timeout SECONDS", "Wait deadline; default 30."),),
        examples=("server stop", "server stop --timeout 60"),
        notes=("An external server cannot be stopped through this client.",),
    ),
    "attach": HelpTopic(
        "attach",
        "Attach to a Linux process and select the returned session.",
        usage=("attach PID",),
        examples=("attach 4242",),
    ),
    "sessions": HelpTopic(
        "sessions",
        "List sessions and their probes on the connected server.",
        usage=("sessions",),
        examples=("sessions",),
    ),
    "modules": HelpTopic(
        "modules",
        "List current eligible mapped application image paths on the server.",
        usage=("modules",),
        examples=("modules",),
    ),
    "symbols": HelpTopic(
        "symbols",
        "List traceable functions and methods from eligible application "
        "ELF/DWARF on the server host.",
        usage=("symbols [--filter TEXT] [--module TEXT] [--limit N] [--page TOKEN]",),
        options=(
            ("--filter TEXT", "Case-sensitive substring of selector or linkage name."),
            (
                "--module TEXT",
                "Narrow by case-sensitive substring of eligible module path.",
            ),
            ("--limit N", "Optional page size; default unlimited, 0 means unlimited."),
            ("--page TOKEN", "Continue a previous listing using its opaque token."),
        ),
        examples=("symbols --filter Calculator",),
        notes=(
            "Requires an attached session. Discovery reads application ELF/DWARF "
            "on the server; functions without usable debug information are omitted.",
            "--module narrows the existing eligible application image set; runtime "
            "libraries are not added by this option.",
            "Use the full selector shown here with probe resolve or probe inject.",
        ),
    ),
    "use": HelpTopic(
        "use",
        "Select an already attached session for probe and event commands.",
        usage=("use SESSION_ID",),
        examples=("use s1",),
    ),
    "detach": HelpTopic(
        "detach",
        "Detach a session and restore its probes; without an argument, "
        "detach the selected session.",
        usage=("detach [SESSION_ID]",),
        examples=("detach", "detach s1"),
    ),
    "probe": HelpTopic(
        "Probe commands",
        "Resolve C++ declarations, install interceptors, add a complete "
        "spec, or remove a probe.",
        usage=(
            'probe resolve "C++ selector" [OPTIONS]',
            'probe inject "C++ selector" --callback FILE [OPTIONS]',
            'probe validate "C++ selector" --callback FILE [OPTIONS]',
            "probe add SPEC.pbtxt",
            "probe remove PROBE_ID",
        ),
        examples=("help probe inject", 'probe resolve "add(int, int)"'),
    ),
    "probe resolve": HelpTopic(
        "probe resolve",
        "Ask the server to infer a C++ callable signature from target DWARF.",
        usage=('probe resolve "C++ selector" [OPTIONS]',),
        options=(
            ("--include HEADER", "Add a header on the server host; may be repeated."),
            ("--module TEXT", "Mapped path or path substring to search explicitly."),
            ("--debug-max-entries N", "Optional DWARF entry limit; 0 is unlimited."),
            ("--debug-max-depth N", "Optional DWARF depth limit; 0 is unlimited."),
            (
                "--compdb PATH",
                "Compilation database directory or JSON file on the server host.",
            ),
            (
                "--compile-option ARG",
                "Append one compiler option; may be repeated in order.",
            ),
        ),
        examples=('probe resolve "Calculator::multiply(int, int) const"',),
    ),
    "probe inject": HelpTopic(
        "probe inject",
        "Resolve a declaration and install a C++ interceptor from a "
        "callback source file.",
        usage=('probe inject "C++ selector" --callback FILE [OPTIONS]',),
        options=(
            (
                "--callback FILE",
                "Required C++ Interceptor source, read on the console host.",
            ),
            ("--stack 0..64", "Capture entry stacks; default 0 disables capture."),
            (
                "--internal-frames",
                "Include tracer/JIT/runtime frames; default filters them.",
            ),
            (
                "--print-arguments",
                "Include captured arguments in trace snapshots and display.",
            ),
            (
                "--print-result",
                "Include the original result in trace snapshots and display.",
            ),
            ("--include HEADER", "Header on the server host; may be repeated."),
            (
                "--compile-option ARG",
                "Append one compiler option; may be repeated in order.",
            ),
            ("--debug-max-entries N", "Optional DWARF entry limit; 0 is unlimited."),
            ("--debug-max-depth N", "Optional DWARF depth limit; 0 is unlimited."),
            ("--module TEXT", "Mapped path or path substring to search explicitly."),
            (
                "--compdb PATH",
                "Compilation database directory or JSON file on the server host.",
            ),
            (
                "--package SERVER_PATH",
                "Server-host path for shared callback package lineage.",
            ),
        ),
        examples=(
            'probe inject "add(int, int)" --callback '
            "/usr/local/share/clang-tracer/examples/cpp-interceptors/"
            "free_function.cpp --stack 16",
            'probe inject "Calculator::multiply(int, int) const" --callback '
            "/usr/local/share/clang-tracer/examples/cpp-interceptors/method.cpp",
        ),
        notes=(
            "Injecting the same target again replaces its active probe in this "
            "session. The new callback compiles before the old hook is removed.",
            "Callback source is sent by value; include and compilation-"
            "database paths refer to the server host.",
            "If --compdb is omitted, the server can infer compiler flags from the "
            "first --include.",
            "--print-arguments and --print-result add fields to trace "
            "snapshots and display; they do not print from the interceptor.",
        ),
    ),
    "probe add": HelpTopic(
        "probe add",
        "Install a full Protobuf ProbeSpec parsed from a text-format file.",
        usage=("probe add SPEC.pbtxt",),
        examples=("probe add /tmp/probe.pbtxt",),
    ),
    "probe validate": HelpTopic(
        "probe validate",
        "Check callback syntax or complete compilation without installing a probe.",
        usage=('probe validate "C++ selector" --callback FILE [OPTIONS]',),
        options=(
            ("--callback FILE", "Callback source file on the console host."),
            (
                "--level syntax|compile",
                "Select syntax parsing or full code generation and symbol checks.",
            ),
            ("--include HEADER", "Header on the server host; may be repeated."),
            ("--compdb PATH", "Compilation database on the server host."),
            (
                "--compile-option ARG",
                "Append one compiler option; may be repeated in order.",
            ),
            ("--debug-max-entries N", "Optional DWARF entry limit; 0 is unlimited."),
            ("--debug-max-depth N", "Optional DWARF depth limit; 0 is unlimited."),
            ("--module TEXT", "Mapped path or path substring to search explicitly."),
        ),
        examples=('probe validate "add(int, int)" --callback interceptor.cpp',),
        notes=("Validation does not install a probe or execute callback code.",),
    ),
    "probe remove": HelpTopic(
        "probe remove",
        "Remove one probe and restore the target's original bytes.",
        usage=("probe remove PROBE_ID",),
        examples=("probe remove 7",),
    ),
    "events": HelpTopic(
        "events",
        "Stream live events until interrupted or until --count is reached.",
        usage=(
            "events [--session ID ...] [--pid PID] [--after CURSOR] "
            "[--name NAME ...] [--count N] [--timeout SECONDS]",
        ),
        options=(
            (
                "--session ID",
                "Filter by session; may be repeated. Defaults to selected "
                "session, or all sessions.",
            ),
            ("--pid PID", "Filter by process ID."),
            (
                "--name NAME",
                "Exact registered C++ selector/linkage name or emitted "
                "context event name; long selectors use the linkage name. "
                "repeatable and ORed (up to 4096 UTF-8 bytes), while ANDed "
                "with other filters.",
            ),
            ("--after CURSOR", "Resume strictly after this event sequence; default 0."),
            ("--count N", "Stop after N events; default streams until interrupted."),
            ("--timeout SECONDS", "Total stream deadline; default has no deadline."),
        ),
        examples=("events --count 10 --timeout 20", "events --session s1 --after 42"),
        notes=(
            "Expired server history cursors report OUT_OF_RANGE; the console "
            "does not reset them automatically.",
        ),
    ),
    "collect": HelpTopic(
        "collect",
        "Stream and display events until interrupted or an optional limit is reached.",
        usage=("collect [COUNT] [STREAM_OPTIONS]",),
        options=(
            (
                "COUNT",
                "Optional positive event count; default streams until interrupted.",
            ),
            (
                "--session ID",
                "Filter session; may be repeated. Defaults to selected session or all.",
            ),
            ("--pid PID", "Filter process ID."),
            (
                "--name NAME",
                "Exact registered C++ selector/linkage name or emitted "
                "context event name; long selectors use the linkage name. "
                "repeatable and ORed (up to 4096 UTF-8 bytes).",
            ),
            ("--after CURSOR", "Start strictly after this sequence; default 0."),
            ("--timeout SECONDS", "Total deadline; default has no deadline."),
        ),
        examples=("collect 20 --timeout 10",),
    ),
    "record": HelpTopic(
        "record",
        "Save events as binary Protobuf or append them to SQLite.",
        usage=("record FILE COUNT [--sqlite] [STREAM_OPTIONS]",),
        options=(
            ("FILE", "New Protobuf file, or a new/existing SQLite database."),
            ("--sqlite", "Append complete events to SQLite with deduplication."),
            ("COUNT", "Required positive event count."),
            (
                "--session ID",
                "Filter session; may be repeated. Defaults to selected session or all.",
            ),
            ("--pid PID", "Filter process ID."),
            (
                "--name NAME",
                "Exact registered C++ selector/linkage name or emitted "
                "context event name; long selectors use the linkage name. "
                "repeatable and ORed (up to 4096 UTF-8 bytes).",
            ),
            ("--after CURSOR", "Start strictly after this sequence; default 0."),
            ("--timeout SECONDS", "Total deadline; default 30."),
        ),
        examples=("record capture.pb 1000 --timeout 60",),
        notes=(
            "File framing is uint32 little-endian length followed by "
            "serialized TraceEvent bytes.",
        ),
    ),
    "drain": HelpTopic(
        "drain",
        "Commit retained events to SQLite, then consume them from shared history.",
        usage=("drain DATABASE [--listen] [--count N] [STREAM_OPTIONS]",),
        options=(
            ("DATABASE", "SQLite file on this host; appends and deduplicates."),
            ("--listen", "Continue collecting generated events until Ctrl-C."),
            ("--count N", "Stop after at most N events."),
            ("--timeout SECONDS", "Stop after this duration; default no deadline."),
            ("--session ID", "Filter; defaults to selected session or all."),
            ("--pid PID", "Filter by process ID."),
            ("--name NAME", "Repeatable exact function/context event name filter."),
            ("--after CURSOR", "Only consume events after this sequence."),
        ),
        examples=("drain capture.db", "drain capture.db --listen --timeout 60"),
        notes=(
            "Without --listen, drains only the snapshot present at the first read.",
            "Storage errors leave events unacknowledged; retry the same database.",
            "Consumption affects all readers. Overflow can still lose events.",
        ),
    ),
    "help": HelpTopic(
        "help",
        "Show global help, a command group, or one command's detailed "
        "options and examples.",
        usage=("help [TOPIC ...]",),
        examples=("help", "help server", "help probe inject", "help events"),
    ),
    "colors": HelpTopic(
        "Color output",
        "Choose whether the console styles its prompt, help, and event display.",
        usage=("clang-tracer-console [--color auto|always|never]",),
        options=(
            ("auto", "Use color on a terminal unless NO_COLOR is set; default."),
            ("always", "Emit ANSI styling even when output is redirected."),
            ("never", "Disable ANSI styling."),
        ),
        examples=("help colors",),
        notes=("The explicit always and never settings override NO_COLOR.",),
    ),
    "shell": HelpTopic(
        "Shell escape",
        "A leading ! runs the exact remainder through /bin/sh -c on the console host.",
        usage=("! SHELL_COMMAND",),
        examples=("! pwd", "! printf '%s\\n' \"$HOME\" | sort # shell comment"),
        notes=(
            "The shell inherits terminal input/output, current directory, "
            "and environment. cd does not persist after the command.",
            "Ctrl-C interrupts the foreground process group. The console "
            "waits for and reaps the shell without killing it.",
            "A bare ! or whitespace-only shell command is rejected. # is "
            "preserved in shell input, unlike console comments.",
        ),
    ),
    "exit": HelpTopic(
        "exit / quit",
        "Leave the console. A server launched by this console is shut down "
        "gracefully; an external server remains running.",
        usage=("exit | quit",),
        examples=("exit", "quit"),
    ),
}
HELP_TOPICS["!"] = HELP_TOPICS["shell"]


def help_for(arguments: Iterable[object]) -> FormattedText:
    key = " ".join(str(argument).lower() for argument in arguments)
    if key == "quit":
        key = "exit"
    topic = HELP_TOPICS.get(key)
    if topic is None:
        known = ", ".join(name or "(overview)" for name in HELP_TOPICS)
        return FormattedText([("class:error", f"No help for {key!r}. Topics: {known}")])

    fragments: list[tuple[str, str]] = [
        ("class:help.heading", topic.title + "\n"),
        ("class:help.text", topic.summary + "\n"),
    ]
    if topic.usage:
        fragments.append(("class:help.label", "Usage:\n"))
        fragments.extend(("class:help.usage", f"  {line}\n") for line in topic.usage)
    if topic.options:
        fragments.append(("class:help.label", "Options:\n"))
        fragments.extend(
            ("class:help.option", f"  {option} — {description}\n")
            for option, description in topic.options
        )
    if topic.examples:
        fragments.append(("class:help.label", "Examples:\n"))
        fragments.extend(
            ("class:help.example", f"  {line}\n") for line in topic.examples
        )
    if topic.notes:
        fragments.append(("class:help.label", "Notes:\n"))
        fragments.extend(("class:help.text", f"  {note}\n") for note in topic.notes)
    if fragments[-1][1].endswith("\n"):
        fragments[-1] = (fragments[-1][0], fragments[-1][1].rstrip())
    return FormattedText(fragments)
