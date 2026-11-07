# Frida Gum 17.26.1 Linux x86-64 license audit

Scope is the release devkit with SHA-256
`4e7247a5f990b15d319c997836af2f6fea38fccb493dc7f6b4b6142009df040e`
and the pilot's SDK-linked `gum-backend.so`. The devkit tarball contains a
header, an example and `libfrida-gum.a`. The release's pinned dependency
manifest identifies the source revisions below. Counts come from the archive
member list, DWARF compilation units and the pilot's GNU linker map.
[Release](https://github.com/frida/frida/releases/tag/17.26.1),
[Dependency manifest](https://github.com/frida/releng/blob/164a6a41179c38481e6afa4303abb92c9234ea2c/deps.toml)

| Component | Archive members | Linked members | License evidence at the pinned revision |
| --- | ---: | ---: | --- |
| Frida Gum | 80 | 42 | wxWindows Library Licence 3.1 with binary distribution exception. [COPYING](https://github.com/frida/frida-gum/blob/00a05282d1074c0e043b285c70a4412d784c573d/COPYING) |
| Capstone | 79 | 74 | BSD-3-Clause and University of Illinois/NCSA for LLVM-derived code. [LICENSE](https://github.com/frida-forks/capstone/blob/f919a03a5417e1c7ea3af7035225073437030ef2/LICENSE.TXT), [LLVM license](https://github.com/frida-forks/capstone/blob/f919a03a5417e1c7ea3af7035225073437030ef2/LICENSE_LLVM.TXT) |
| GLib, GObject, GIO and GModule | 379 | 99 | LGPL-2.1-or-later with embedded code discussed below. [COPYING](https://github.com/frida-forks/glib/blob/f11db6352345ad21deb5f5783bb988bfca2a3d85/COPYING) |
| JSON-GLib | 17 | 0 | LGPL-2.1-or-later. [COPYING](https://github.com/frida-forks/json-glib/blob/0242c39f1f60f844c9df41e83434251b06609e82/COPYING) |
| libdwarf | 64 | 0 | LGPL-2.1 for the inspected library source. [Library copyright](https://github.com/frida-forks/libdwarf/blob/9ac8cc22cab54d72cb355a44e85db716a7006f47/src/lib/libdwarf/LIBDWARFCOPYRIGHT) |
| libffi | 10 | 8 | MIT. [LICENSE](https://github.com/frida-forks/libffi/blob/5cca756802a5d27988e7f9973e1299f684cfeff7/LICENSE) |
| libunwind | 54 | 0 | MIT-style with per-file copyright notices. [COPYING](https://github.com/frida-forks/libunwind/blob/733d7d1c73f3512d2a50add2337157fa385a7e6e/COPYING) |
| PCRE2 | 31 | 26 | BSD-3-Clause with PCRE2 exception, plus BSD-2-Clause SLJIT and BSD-3-Clause SIMD code. [LICENCE](https://github.com/frida-forks/pcre2/blob/683059230e4894342bb19f2776f0e50c757fde17/LICENCE.md) |
| TinyCC | 11 | 0 | Library sources LGPL-2.0-or-later. [COPYING](https://github.com/frida-forks/tinycc/blob/c0b84021e3e5c8359a469b1af2bf4abd83a7a26a/COPYING), [libtcc source](https://github.com/frida-forks/tinycc/blob/c0b84021e3e5c8359a469b1af2bf4abd83a7a26a/libtcc.c) |
| XZ liblzma | 79 | 33 | 0BSD for liblzma. [Licensing summary](https://github.com/frida-forks/xz/blob/68289232393af1ff1756bcf05164cedfd792df59/COPYING), [0BSD](https://github.com/frida-forks/xz/blob/68289232393af1ff1756bcf05164cedfd792df59/COPYING.0BSD) |
| zlib | 15 | 0 | zlib license. [LICENSE](https://github.com/frida-forks/zlib/blob/b910f805561e24a7a25521de438c82942cd7885b/LICENSE) |
| **Total** | **819** | **282** | |

The linked GLib objects include Gnulib formatting code under LGPL-2.1-or-later
and libcharset and proxy-libintl under LGPL-2.0-or-later. The devkit also
contains GLib's GVDB and XDG MIME code. GVDB is LGPL-2.1-or-later, and
XDG MIME offers LGPL-2.1-or-later or AFL-2.0. Neither appears in the linked
member list. GLib's UTF-8 source also records its LGPL option for imported
code. [Gnulib](https://github.com/frida-forks/glib/blob/f11db6352345ad21deb5f5783bb988bfca2a3d85/glib/gnulib/vasnprintf.c),
[libcharset](https://github.com/frida-forks/glib/blob/f11db6352345ad21deb5f5783bb988bfca2a3d85/glib/libcharset/localcharset.c),
[proxy-libintl](https://github.com/frida-forks/glib/blob/f11db6352345ad21deb5f5783bb988bfca2a3d85/proxy-libintl-static/libintl.c),
[GVDB](https://gitlab.gnome.org/GNOME/gvdb/-/blob/2b42fc75f09dbe1cd1057580b5782b08f2dcb400/gvdb/gvdb-reader.c),
[XDG MIME](https://github.com/frida-forks/glib/blob/f11db6352345ad21deb5f5783bb988bfca2a3d85/gio/xdgmime/xdgmime.c),
[UTF-8 source](https://github.com/frida-forks/glib/blob/f11db6352345ad21deb5f5783bb988bfca2a3d85/glib/gutf8.c)

Gum also includes separately attributed printf code, Doug Lea's public-domain
allocator and a Valgrind client header with BSD-style terms. Preserve the
individual notices in a release inventory.
[printf](https://github.com/frida/frida-gum/blob/00a05282d1074c0e043b285c70a4412d784c573d/gum/gumprintf.c),
[allocator](https://github.com/frida/frida-gum/blob/00a05282d1074c0e043b285c70a4412d784c573d/gum/dlmalloc.c),
[Valgrind header](https://github.com/frida/frida-gum/blob/00a05282d1074c0e043b285c70a4412d784c573d/gum/valgrind.h)

## Reproduce the inventory

Run from the pilot directory after the SDK build. The first command lists
every member. The second exposes the source path and compilation directory
for 813 of the 819. The five empty Capstone members and libunwind's
`dl-iterate-phdr.c.o` are identified
by their member names and the pinned source trees. The last command lists
the archive members extracted for the plugin.

```sh
ar t "$GUM_DEVKIT/libfrida-gum.a"
readelf --debug-dump=info "$GUM_DEVKIT/libfrida-gum.a"
sed -n 's/.*libfrida-gum\.a(\([^)]*\)).*/\1/p' \
  "$PILOT_BUILD/gum-backend.map" | sort -u
```

## Distribution status

The pilot stays local. The plugin includes GLib code statically, so its
`.so` extension and Gum's exception do not discharge GLib's separate
LGPL obligations. A release of this static build would need applicable
notices and license texts, exact corresponding LGPL source and changes,
and a compliant way for recipients to modify the LGPL components and relink
the plugin. The alternative of replaceable shared dependencies would need
its own link, ABI and license validation. A release that also ships the
devkit would have to cover the archive-only components. The 564 GNU
property warnings from the SDK linker also remain open.
[GLib license, sections 5 and 6](https://github.com/frida-forks/glib/blob/f11db6352345ad21deb5f5783bb988bfca2a3d85/COPYING)
