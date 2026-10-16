# Time-Travel Debugger (TTDB) - Server (Phase 01)

A C++ implementation of a time-travel debugger server for a custom language called C--. The server reads a source file, checks the program's structure, resolves function calls, and executes instructions step by step. During execution, it records snapshots of the program's state and saves them in a `.tdbg` file for later use.

## Pipeline Architecture

1. **Stage 0 (Receive):** Reads the C-- instructions from the `source.bin` file.
2. **Stage 1 (Pass 0x0 - Validation):** Checks that every `func` has a matching `func_end` and makes sure functions are not declared inside other functions.
3. **Stage 2 (Pass 0x1 - Resolve):** Stores instructions in `resolve.bin` using the format `[offset(8B)][size(4B)][string]`. It also resolves function calls by replacing them with the correct function byte offsets and checks for duplicate or missing functions.
4. **Stage 3 (Pass 0x2 - Execution):** Starts execution from `main` and runs instructions one by one. It uses a call stack to manage function calls and stores snapshots of the program's state in a doubly linked list called `Timeline`.
5. **Stage 4 (Pass 0x3 - Serialization):** Writes the execution snapshots into `session.tdbg`, along with a file header and a dense index that helps locate individual snapshots.

## Data Structures Used

- **Stack (`Stack<T>`):** Used during validation to check function blocks and during execution to manage function calls and their local variables.
- **Doubly Linked List (`Timeline`):** Stores the execution snapshots in order and allows traversal in both directions.

## C-- Supported Instruction Set

- `func <name> [params]` - Defines a function.
- `func_end` - Marks the end of a function.
- `call <func_name> [args]` - Calls a function.
- `set <var> <val>` - Assigns a value to a variable.
- `add <var1> <var2/val>` - Adds a value to a variable.
- `sub <var1> <var2/val>` - Subtracts a value from a variable.
- `mul <var1> <var2/val>` - Multiplies a variable by a value.
- `div <var1> <var2/val>` - Divides a variable by a value.

## Build & Run Instructions

### Building

Compile the project using Make:

```bash
make
```

For Windows using MinGW:

```bash
mingw32-make
```

### Running

Make sure `source.bin` is present in the project directory, then run:

```powershell
.\server.exe
```

The server generates `resolve.bin` and `session.tdbg` during successful processing.

## Testing

The server was tested using different C-- programs to check arithmetic operations, function calls, nested calls, execution snapshots, and error handling.