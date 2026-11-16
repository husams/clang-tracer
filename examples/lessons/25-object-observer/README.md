# Lesson 25: write a custom observer for an object's method

Reference: [Register a C++ type for object probing](../../../docs/native-repl.md#register-a-c-type-for-object-probing)
(REPL walkthrough; the user guide has no section for this lesson yet).

The built-in observer of an object probe records the call, its arguments and
the result. A custom observer is a small C++ class you write. It runs inside
the target at the start (`pre`) and the end (`post`) of every call of the
method you choose, and it can add named values of your own: the arguments,
fields of the object the method was called on, and the result. It also gets
the object's ID, which stays the same from the constructor to the destructor.

This lesson needs a Linux x86-64 build of the tools that includes
`%object-probe`.

| File | Purpose |
| --- | --- |
| `account.hpp` | `shop::Account` with a `balance`, a `deposits` count and `deposit(int)`. |
| `account.cpp` | The constructor, the destructor and `deposit`. |
| `target.cpp` | Opens an account, makes two deposits and closes it each time you press Enter. |
| `observer.cpp` | The custom observer for `deposit`. |
| `Makefile` | Builds `target` and writes `compile_commands.json` for this directory. |

Run `make`, start `./target`, then start `clang-tracer-repl` in this directory.

## The observer

```cpp
template <> class Interceptor<tracer::ResolvedSignature> {
public:
  void pre(const Call &call) {
    call.field("object_id", call.object_id());
    call.field("generation", call.object_generation());
    call.field("amount", call.argument<int>(0));
    call.field("balance_before", call.self()->balance);
  }

  void post(const Call &call) {
    call.field("object_id", call.object_id());
    call.field("balance_after", call.self()->balance);
    call.field("deposits", call.self()->deposits);
    call.field("returned", call.result<int>());
  }
};
```

- `tracer::ResolvedSignature` stands for the signature of the method the
  package is prepared for, `int shop::Account::deposit(int)` here. You do not
  write it out.
- `call.field("name", value)` adds a named value to the event. Events print
  these after `args=[...]`.
- `call.argument<int>(0)` reads the first argument.
- `call.self()` is the object the method was called on, typed as
  `shop::Account` from the header you pass to `prepare`. That is why
  `call.self()->balance` reads the real field.
- `call.result<int>()` is only available in `post`.

## Part 1: prepare the package

```bash
%object-probe prepare shop::Account --include ./account.hpp --compdb ./compile_commands.json --translation-unit ./account.cpp --method "shop::Account::deposit(int)" --callbacks ./observer.cpp --output ./account.object
```

- `--include`, `--compdb` and `--translation-unit` tell the tracer how the
  class was compiled, so that it can build the typed receiver.
- `--method` selects the method to observe. Repeat it for more methods.
- `--callbacks` is your observer. It is compiled during prepare, so an error
  in it is reported now and not in the target.
- `--output` must be a new path. Run `make clean` before you repeat the lesson.

Expected: `object-probe package=./account.object type=shop::Account methods=1 writers=0`.

## Part 2: attach, start and record one account

```bash
%attach 12345
%object-probe preview --package ./account.object
%object-probe start --package ./account.object
%record account.db --sqlite --count 6 --timeout 30
```

Expected: preview lists the verified method and the construct and destroy
sites, and start prints `object-probe registration=... active=yes methods=1`.
Note the registration number.

Press Enter once in the target terminal while `%record` waits. Expected:
`received=6` and `complete=true`.

```bash
%object-probe status REGISTRATION_ID
%detach
%events open account.db --as acct
%events list acct
```

Expected: `%detach` restores the target, and the recording holds six events
(the cursor, session and thread columns are left out here):

```text
0 ... phase=PRE call=1 function=shop::Account::deposit(int) args=[amount=25] object_id=1 generation=1 amount=25 balance_before=100
1 ... phase=POST call=1 function=shop::Account::deposit(int) args=[amount=25] object_id=1 balance_after=125 deposits=1 returned=125 result=125
2 ... phase=PRE call=2 function=shop::Account::deposit(int) args=[amount=50] object_id=1 generation=1 amount=50 balance_before=125
3 ... phase=POST call=2 function=shop::Account::deposit(int) args=[amount=50] object_id=1 balance_after=175 deposits=2 returned=175 result=175
4 ... CREATED shop::Account#1 generation=1 address=0x... function="shop::Account::Account(int)" ... account.cpp:4
5 ... DESTROY_BEGIN shop::Account#1 generation=1 address=0x... function="shop::Account::~Account()" ... account.cpp:6
```

## How to read the events

- Events 0 to 3 come from your observer. Each pre and post pair shares a
  `call` number. Your `field` values follow `args=[...]`.
- `balance_before=125` in event 2 is the balance the first deposit left
  behind, read from the object while the second call starts.
- `object_id=1` is the same in all four calls. The tracer gave that ID to the
  account when its constructor ran.
- Events 4 and 5 are the object's lifetime. They are recorded by the same
  registration and can arrive after the method events, so do not rely on
  their position.
- `%events show acct --index 1` prints the whole event, including
  `object_fields` for each value your observer added and a `receiver_snapshot`.

## Things to try

- Record again with `--count 12` and press Enter twice. The second account
  gets `object_id=2`, so you can follow each account's calls separately.
- Add `call.field("deposits_before", call.self()->deposits);` to `pre`,
  prepare into a new `--output`, and run the lesson again.
- Misspell a field in `observer.cpp` and run `prepare` again with a new
  `--output`. The compiler error appears at prepare time, before any
  attach.

## Limits you will meet

- `call.self()` reads the object while the method runs. Borrow it only
  inside `pre` and `post`; do not keep the pointer.
- A callback that fails at run time is recorded with its call ID and phase.
  Select only those events with `--callback-errors only` on `%collect`,
  `%record` or `%events open`.
- The Makefile builds `target` with `-O0 -fno-inline -fpatchable-function-entry=8`,
  the flags the tracer's own object-probe tests use. A call that the compiler
  inlines never reaches the method entry, so it cannot be observed.

Run `make clean` before repeating the lesson so `account.object` can be
created again.
