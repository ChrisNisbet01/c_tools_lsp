# C Tools LSP Server

A Language Server Protocol (LSP) server that provides cyclomatic complexity analysis for C code.

## Features

- **Cyclomatic complexity** calculation for C functions via `textDocument/functionComplexity`
- Standard LSP lifecycle: `initialize`, `initialized`, `shutdown`, `exit`
- Document synchronization: `textDocument/didOpen`, `didChange`, `didClose`
- VSCode extension included in `c-tools-lsp-vscode/`

## Dependencies

- json-c
- libubox (with uloop and runqueue)
- CMake
- C compiler (C11)
- [cyclomatic_complexity](https://github.com/anomalyco/c_tools) tool at runtime

## Build

```sh
cmake -S . -B build
cmake --build build
```

The server binary is produced at `build/src/c_tools_lsp`.

## Usage

The server speaks LSP over stdin/stdout. Run it directly:

```sh
./build/src/c_tools_lsp
```

Or launch it from an editor that supports LSP. With the VSCode extension, open `c-tools-lsp-vscode/` as a workspace folder and launch with F5.

### Configuration

Pass include paths via the `initializationOptions` in the `initialize` request:

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "initialize",
  "params": {
    "initializationOptions": {
      "includePaths": ["/path/to/includes"]
    }
  }
}
```

### Custom method

**`textDocument/functionComplexity`** — computes cyclomatic complexity for a named function.

Request:
```json
{
  "jsonrpc": "2.0",
  "id": 2,
  "method": "textDocument/functionComplexity",
  "params": {
    "textDocument": { "uri": "file:///path/to/file.c" },
    "functionName": "my_function"
  }
}
```

Response:
```json
{
  "jsonrpc": "2.0",
  "id": 2,
  "result": {
    "complexity": 5,
    "function_name": "my_function"
  }
}
```

## Testing

```sh
python3 server_test.py
python3 test_full_protocol.py
python3 test_complexity.py
```

## Project structure

```
src/
  main.c          — entry point
  server.c/h      — server setup and event loop
  transport.c/h   — stdin/stdout I/O with Content-Length framing
  rpc.c/h         — JSON-RPC message dispatch
  handlers.c/h    — LSP method handlers
  documents.c/h   — in-memory document store
  tool_runner.c/h — async external tool execution via fork+pipe
  utils.c/h       — misc utilities
c-tools-lsp-vscode/ — VSCode extension
```
