import subprocess
import threading
import sys
import json
import time
import os

SERVER_PATH = './build/src/c_tools_lsp'

def stream_reader(stream, prefix):
    buf = b''
    while True:
        data = stream.read(1)
        if not data:
            break
        buf += data
        if buf.endswith(b'\r\n\r\n'):
            header = buf.decode('utf-8', errors='replace')
            clen = 0
            for line in header.strip().split('\r\n'):
                if line.lower().startswith('content-length:'):
                    clen = int(line.split(':')[1].strip())
            body = stream.read(clen)
            content = body.decode('utf-8', errors='replace')
            print(f"[{prefix}] {content}", flush=True)
            buf = b''

def send_msg(process, msg):
    content = json.dumps(msg)
    header = f"Content-Length: {len(content)}\r\n\r\n{content}"
    process.stdin.write(header.encode('utf-8'))
    process.stdin.flush()

def main():
    process = subprocess.Popen(
        [SERVER_PATH],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        bufsize=0
    )

    threading.Thread(target=stream_reader, args=(process.stdout, "STDOUT"), daemon=True).start()
    threading.Thread(target=stream_reader, args=(process.stderr, "STDERR"), daemon=True).start()

    time.sleep(0.3)

    # Initialize with include paths
    msg = {
        "jsonrpc": "2.0",
        "id": 1,
        "method": "initialize",
        "params": {
            "processId": None,
            "rootUri": None,
            "capabilities": {},
            "initializationOptions": {
                "includePaths": [os.path.abspath("src")]
            }
        }
    }
    send_msg(process, msg)
    time.sleep(0.3)

    # initialized notification
    msg = {
        "jsonrpc": "2.0",
        "method": "initialized",
        "params": {}
    }
    send_msg(process, msg)
    time.sleep(0.3)

    # didOpen with server.c content
    with open('src/server.c', 'r') as f:
        text = f.read()
    msg = {
        "jsonrpc": "2.0",
        "method": "textDocument/didOpen",
        "params": {
            "textDocument": {
                "uri": "file:///test/server.c",
                "languageId": "c",
                "version": 1,
                "text": text
            }
        }
    }
    send_msg(process, msg)
    time.sleep(0.3)

    # function complexity request for stdin_cb
    msg = {
        "jsonrpc": "2.0",
        "id": 2,
        "method": "textDocument/functionComplexity",
        "params": {
            "textDocument": {"uri": "file:///test/server.c"},
            "functionName": "stdin_cb"
        }
    }
    send_msg(process, msg)

    timeout = 10
    start = time.time()
    while time.time() - start < timeout:
        if process.poll() is not None:
            break
        time.sleep(0.2)

    if process.poll() is None:
        process.terminate()
        process.wait(timeout=2)

    ret = process.poll()
    print(f"[SYSTEM] Server exited with code: {ret}")
    return ret

if __name__ == "__main__":
    sys.exit(main())
