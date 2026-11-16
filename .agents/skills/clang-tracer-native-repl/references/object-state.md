# Capture object state on `c++17`

For attach, preview, recording, and restoration, use the
[object tracking workflow](object-tracking.md).

## Track an owner or a moving entry

Find verified add, get, and remove boundaries before using `--owner` and
`--collection`. For a vector element use `--vector` and `--element-key`.
For a map entry use `--map` and `--map-key`. Use `--pointee` for a
verified pointer path.

Supply `--mutation` for each storage-changing boundary and verify rebinding
after the backing storage moves. For logical deltas, use
`--mode logical-writes` with synchronization and inspect `LOGICAL_UPDATE`.
Use `--mutation "destroy=SYMBOL"` for a verified directly called destruction
boundary.

## Compare state across a method

Prepare with `--on METHOD`, the declaring `--include`, original compile
command, and the `--synchronization` policy the application actually provides.
Use `--field` and `--key PATH=MEMBER` for typed keyed comparisons. Added and
removed entries carry a tree in `snapshotAfter` or `snapshotBefore`. A
completed store may leave the same value at return.

## Inspect writes to structured fields

Use `--first-seen` and verified synchronization for supported libstdc++
string, variant, or `std::any` storage. For a nested union use a path such as
`--field state.payload` and provide `--union-tag` and `--union-case TAG=MEMBER`
for typed capture. Inspect `storageWrite` for the affected bytes and original
store PC. Use method snapshots for whole typed values.

Use the [object lessons](../../../../examples/lessons/README.md) for
collection, map, logical-update, string-storage, and union fixtures. Use the
[method watch guide](../../../../docs/native-repl.md#prepare-an-object-watch-for-method-snapshots)
for synchronization choices and capture limits.
