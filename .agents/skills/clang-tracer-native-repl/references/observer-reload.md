# Reload an active observer on `c++17`

Use `%probe reload PROBE_ID --callback observer-v2.cpp` to replace that
probe's callback while keeping its binding and deployment options. Add
`--watch --count 2 --timeout 20` for a bounded edit loop. A rejected edit
leaves the current generation active. Calls already in flight finish under
the ID they entered. Check `%probes` after an interrupted reply, and use
the new ID for subsequent event filters and removal. Retired generations
can still count toward the target's retained-probe limit.

Trigger the same input before and after reload and compare its event payload
or result, depending on the probe's permissions. Use `%probe remove ID` on
the active generation and verify restoration.
