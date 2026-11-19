/* Merge writable flags into GCC 8's address table without changing its contents.
 * Otherwise DSO loading needs text relocations, rejected by hardened loaders. */
__asm__(".pushsection __patchable_function_entries,\"aw\",@progbits\n"
        ".popsection\n");
