# time-traveller
# Time-Travel Debugger (TTDB) - Server (Phase 01)

A C++ server implementation for time-travel debugging built on a multi-pass pipeline. The server processes C-- binary source files, validates structural scopes, resolves function calls to byte offsets, executes instructions line-by-line, and generates a formatted `.tdbg` session file containing full state execution snapshots and a dense temporal index.

## Pipeline Architecture

1. **Stage 0 (Receive):** Reads the source file (`source.bin`) written in the custom C-- instruction set.
2. **Stage 1 (Pass 0x0 - Validation):** Enforces strict 1:1 `func` and `func_end` block matching and explicitly rejects nested function declarations.
3. **Stage 2 (Pass 0x1 - Resolve):** Converts lines into structured binary records inside `resolve.bin` formatted as `[offset(8B)][string_size(4B)][string]` and patches `call` instructions with their target function byte offsets.
4. **Stage 3 (Pass 0x2 - Execution):** Executes instructions step-by-step starting from `main`, manages call stack frames, and logs state snapshots after every step into a doubly linked list `Timeline`.
5. **Stage 4 (Pass 0x3 - Serialization):** Formats and writes the fixed 16-byte header, sequential snapshot stream, and trailing dense index array into `session.tdbg`.

## Data Structures Used

* **Stack (`Stack<T>`):** Used for Stage 1 validation (to match scope keywords) and Stage 3 call stack management (LIFO frame tracking).
* **Doubly Linked List (`Timeline`):** Maintains the timeline of execution snapshots, allowing dynamic appends and forward/backward traversal.

## C-- Supported Instruction Set

* `func <name> [params]`
* `func_end`
* `call <func_name> [args]`
* `set <var> <val>`
* `add <var1> <var2/val>`
* `sub <var1> <var2/val>`
* `mul <var1> <var2/val>`
* `div <var1> <var2/val>`

## Build & Run Instructions

### Building
```bash
make
