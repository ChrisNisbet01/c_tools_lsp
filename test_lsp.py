#!/usr/bin/env python3
"""Test that the LSP server responds to requests with proper Content-Length framing."""

import subprocess
import sys
import os
import select
import json
import fcntl


def set_nonblock(fd):
    flags = fcntl.fcntl(fd, fcntl.F_GETFL)
    fcntl.fcntl(fd, fcntl.F_SETFL, flags | os.O_NONBLOCK)


def send_frame(pipe, obj):
    body = json.dumps(obj)
    header = f"Content-Length: {len(body)}\r\n\r\n"
    pipe.write(header.encode() + body.encode())
    pipe.flush()


def recv_frame(pipe, timeout=5.0):
    buf = b""
    while True:
        r, _, _ = select.select([pipe], [], [], timeout)
        if not r:
            return None
        try:
            chunk = os.read(pipe.fileno(), 65536)
        except BlockingIOError:
            continue
        if not chunk:
            return None
        buf += chunk
        while True:
            header_end = buf.find(b"\r\n\r\n")
            if header_end == -1:
                break
            header = buf[:header_end]
            body_start = header_end + 4
            for line in header.decode().split("\r\n"):
                if line.lower().startswith("content-length:"):
                    length = int(line.split(":")[1].strip())
            body_end = body_start + length
            if len(buf) >= body_end:
                return json.loads(buf[body_start:body_end].decode())
            break
    return None


def main():
    proc = subprocess.Popen(
        [os.path.join(os.path.dirname(__file__), "build", "src", "c_tools_lsp")],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )

    set_nonblock(proc.stdout.fileno())
    set_nonblock(proc.stderr.fileno())

    # Send initialize request
    send_frame(proc.stdin, {
        "jsonrpc": "2.0",
        "id": 1,
        "method": "initialize",
        "params": {"processId": None, "capabilities": {}},
    })

    resp = recv_frame(proc.stdout, timeout=3.0)
    if resp is None:
        print("FAIL: No response to initialize", file=sys.stderr)
    else:
        print(f"PASS: Got initialize response: {json.dumps(resp, indent=2)}")

    # Send initialized notification
    send_frame(proc.stdin, {
        "jsonrpc": "2.0",
        "method": "initialized",
        "params": {},
    })

    # Write test C file
    test_file = "/tmp/test_simple.c"
    with open(test_file, "w") as f:
        f.write("int add(int a, int b) { return a + b; }\n")

    # Send didOpen
    with open(test_file) as f:
        text = f.read()
    send_frame(proc.stdin, {
        "jsonrpc": "2.0",
        "method": "textDocument/didOpen",
        "params": {
            "textDocument": {
                "uri": f"file://{test_file}",
                "languageId": "c",
                "version": 1,
                "text": text,
            },
        },
    })

    # Request function complexity
    send_frame(proc.stdin, {
        "jsonrpc": "2.0",
        "id": 2,
        "method": "textDocument/functionComplexity",
        "params": {
            "textDocument": {"uri": f"file://{test_file}"},
            "functionName": "add",
        },
    })

    resp = recv_frame(proc.stdout, timeout=10.0)
    if resp is None:
        print("FAIL: No response to functionComplexity", file=sys.stderr)
    else:
        print(f"PASS: Got functionComplexity response: {json.dumps(resp, indent=2)}")

    # Read stderr
    proc.kill()
    err = b""
    try:
        while True:
            chunk = os.read(proc.stderr.fileno(), 4096)
            if not chunk:
                break
            err += chunk
    except (BlockingIOError, OSError):
        pass
    proc.wait()

    print("\nSTDERR:")
    for line in err.decode().splitlines():
        print(f"  {line}")

    return 0 if resp is not None else 1


if __name__ == "__main__":
    sys.exit(main())
