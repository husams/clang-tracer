"""Link a fixture and a matching GNU debug companion from the same object."""

import hashlib
import subprocess
import zlib


def link_split_fixture(compiler, source, root, image, debug, compile_flags, link_flags):
    def run(*arguments):
        subprocess.run([compiler, *arguments], cwd=root, check=True, timeout=30)

    run(*compile_flags, "-g", "-c", source, "-o", "fixture.o")
    build_id = hashlib.sha1((root / "fixture.o").read_bytes()).hexdigest()
    build_id_flag = "-Wl,--build-id=0x" + build_id
    run(*link_flags, build_id_flag, "fixture.o", "-o", debug)
    crc = zlib.crc32((root / debug).read_bytes())
    (root / "debuglink.s").write_text(
        '.section .fixture_debuglink,"",@progbits\n'
        f'.asciz "{debug}"\n.balign 4\n.long {crc}\n'
        '.section .note.GNU-stack,"",@progbits\n'
    )
    (root / "debuglink.ld").write_text(
        "SECTIONS { .gnu_debuglink : { KEEP(*(.fixture_debuglink)) } } INSERT AFTER .comment;\n"
    )
    run("-c", "debuglink.s", "-o", "debuglink.o")
    run(*link_flags, build_id_flag, "-Wl,--strip-debug,-T,debuglink.ld",
        "fixture.o", "debuglink.o", "-o", image)
