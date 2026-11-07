# Relink the Gum Stalker pilot

`gum-backend.so` uses GLib, GObject and GLib's bundled libintl and
formatting code. The applicable LGPL notices and texts are in `notices/`
and the original source headers are in `source/`. Recipients may modify
this work for their own use and reverse engineer it to debug those
modifications as permitted by the LGPL.

The package contains the pilot's plugin, complete GLib and GVDB source
trees, the dependency sources used by the build, and `relink/libgum-base.a`.
That archive holds the Frida Gum, Capstone and liblzma objects needed to
link the plugin. It excludes the original GLib, libffi and PCRE2 objects.
The build compiles replacements from the supplied source and applies
Frida's symbol prefix to the rebuilt libraries. `gum-source-pins.tsv`
identifies every upstream snapshot and `relink/base-members.txt` lists the
retained objects. `GUM_LICENSE_AUDIT.md` describes the upstream devkit and
links to the notices for embedded code.

## Rebuild locally

Use Linux x86-64, Bash, GNU binutils, Python 3, Meson 1.4 or newer, Ninja,
pkg-config, and a C/C++ toolchain targeting glibc 2.23 or earlier. The
verified build used Clang 21.1.8 with the GCC 8.5 SDK
`x86_64-2.6.32-v4.0.137`, Meson 1.9.2, and GNU BFD 2.35.2.
The plugin link uses Clang with `/usr/bin/ld.bfd` and the SDK sysroot.
Set `PILOT_GUM_LINKER` to an alternate BFD executable if needed. The
plugin link treats linker warnings as errors. Set `SDK` to the installed
SDK directory and verify the package before making any source changes.

```sh
sha256sum --check --quiet SHA256SUMS
CC="$SDK/bin/clang" CXX="$SDK/bin/clang++" ./relink.sh /tmp/gum-rebuilt
```

The destination must be a new directory. The build uses the extracted
sources without downloading dependencies and writes
`/tmp/gum-rebuilt/gum-backend.so` and its linker map. To use a modified
GLib, edit the copy under `source/glib/` and run the same command with a
new destination. Set `MESON` to select a Meson executable and `JOBS` to
set the compiler job count.

The rebuilt plugin uses the same pilot ABI. Use its absolute path in the
native REPL live runner described in the Clang Tracer pilot README. The
loader profile for the REPL server, target and agent still needs to match
the target glibc version.
