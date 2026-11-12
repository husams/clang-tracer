# Lesson 7: capture and inspect complete native stacks

Guide section: [Capture and inspect complete native stacks](../../../docs/guide/tutorials.md#capture-and-inspect-complete-native-stacks).

Files: the first lesson's `target.cpp` and `hook.cpp`. Run `make`, start
`./target`, then start `clang-tracer-repl` in this directory.

```bash
%attach 12345
%probe inject "add(int, int)" --callback hook.cpp --stack-depth full --stack-application-only false --print-result
%collect --count 2 --timeout 30 --as fullstack
%events stack fullstack --index 0
%events stats fullstack
%detach
```

Press Enter in the target terminal while `%collect` waits.

Expected: a `stack COMPLETE` report whose frame `#0` is `add(int, int)` at
`target.cpp:6`, followed by `main`, the libc start frames, and `_start`.
