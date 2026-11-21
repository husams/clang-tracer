#!/usr/bin/env python3
"""Rewrite GCC assembly so every artificial `this` parameter has no DW_AT_name.

GCC emits fixed DIE offsets, so the name attribute of a cloned abbreviation becomes
DW_AT_description with the same data form. Machine code, types, locations, object-pointer
references and hidden VTT parameters are unchanged. This is a controlled shape, not output
of an unmodified compiler. Usage: strip_receiver_name.py INPUT.s OUTPUT.s
"""
import pathlib
import re
import sys

source = pathlib.Path(sys.argv[1]).read_text()
prefix, rest = source.split('\t.section\t.debug_abbrev,"",@progbits\n', 1)
abbrevs, suffix = rest.split('\t.section\t.debug_aranges,"",@progbits\n', 1)
assert abbrevs.endswith('\t.byte\t0\n')
abbrevs = abbrevs[:-len('\t.byte\t0\n')]
pattern = r'(?m)^\t\.uleb128 (0x[0-9a-f]+)\t# \(abbrev code\)\n'
starts = list(re.finditer(pattern, abbrevs))
blocks = {}
for index, match in enumerate(starts):
    end = starts[index + 1].start() if index + 1 < len(starts) else len(abbrevs)
    blocks[int(match.group(1), 16)] = abbrevs[match.start():end]
next_code = max(blocks) + 1
clones = {}
changes = []

die_pattern = r'(?m)^\t\.uleb128 (0x[0-9a-f]+)\t# \(DIE \((0x[0-9a-f]+)\) ([^)]+)\)\n'
dies = list(re.finditer(die_pattern, prefix))
for index in range(len(dies) - 1, -1, -1):
    match = dies[index]
    end = dies[index + 1].start() if index + 1 < len(dies) else len(prefix)
    data = prefix[match.start():end]
    if match.group(3) != 'DW_TAG_formal_parameter' or '# DW_AT_name: "this"' not in data:
        continue
    original = int(match.group(1), 16)
    block = blocks[original]
    assert '# (DW_AT_artificial)' in block
    if original not in clones:
        assert next_code < 128 and original < 128
        clone = block.replace(f'.uleb128 {hex(original)}\t# (abbrev code)',
                              f'.uleb128 {hex(next_code)}\t# (abbrev code)', 1)
        clone = clone.replace('.uleb128 0x3\t# (DW_AT_name)',
                              '.uleb128 0x5a\t# (DW_AT_description)', 1)
        assert '# (DW_AT_name)' not in clone
        clones[original] = (next_code, clone)
        next_code += 1
    new_code = clones[original][0]
    data = data.replace(f'.uleb128 {hex(original)}\t# (DIE',
                        f'.uleb128 {hex(new_code)}\t# (DIE', 1)
    data = data.replace('# DW_AT_name: "this"', '# DW_AT_description: "this"', 1)
    prefix = prefix[:match.start()] + data + prefix[end:]
    changes.append(match.group(2))
assert changes
output = prefix + '\t.section\t.debug_abbrev,"",@progbits\n' + abbrevs
output += ''.join(clone for _, clone in clones.values())
output += '\t.byte\t0\n\t.section\t.debug_aranges,"",@progbits\n' + suffix
pathlib.Path(sys.argv[2]).write_text(output)
print(f"unnamed receivers: {len(changes)}")
