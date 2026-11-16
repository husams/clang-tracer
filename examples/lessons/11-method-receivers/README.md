# Lesson 11: read a method's receiver

Guide section: [Decode objects, containers, and method receivers](../../../docs/guide/tutorials.md#decode-objects-containers-and-method-receivers).
Related example: [C++ objects](../../cpp-interceptors/README.md).

A method gets a hidden first argument: the object it runs on. In the agent,
`call.self()` is a pointer to that object, typed from DWARF, so you can read its
public fields before and after the method runs.

```bash
make
./target
```

```bash
%attach 12345
%probe inject "Account::deposit(int)" --callback hook.cpp --print-arguments --print-result
%collect --count 2 --timeout 30 --as deposits
%events show deposits --index 0
%events show deposits --index 1
```

Press Enter once while `%collect` waits. Expected: PRE shows `balance=100` and
`operations=0`; POST shows `balance=125` and `operations=1`, and the result is
125. Pressing Enter again starts from 125 and 1, so the receiver really is
the live object.

`audit_code` is private, so the callback cannot name it. The values you read
are recorded as fields on the event, which is why `%events show` prints
`balance` and `operations` next to the arguments.

To capture the whole receiver object rather than chosen fields, prepare the
probe from the headers: see the [prepared method example](../../prepared-injectors/README.md).
