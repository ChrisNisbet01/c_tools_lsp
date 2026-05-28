#!/usr/bin/env python3
import subprocess
import json
import time
import os
import select

def recv_response(proc, timeout=5):
    buf = b''
    deadline = time.time() + timeout
    while time.time() < deadline:
        r, _, _ = select.select([proc.stdout], [], [], 0.2)
        if r:
            chunk = proc.stdout.read(1)
            if not chunk:
                return None
            buf += chunk
            if buf.endswith(b'\r\n\r\n'):
                hdr = buf.decode()
                buf = b''
                clen = 0
                for line in hdr.strip().split('\r\n'):
                    if line.lower().startswith('content-length:'):
                        clen = int(line.split(':')[1].strip())
                while len(buf) < clen:
                    r2, _, _ = select.select([proc.stdout], [], [], 1)
                    if r2:
                        buf += proc.stdout.read(clen - len(buf))
                    else:
                        break
                if len(buf) == clen:
                    return json.loads(buf.decode())
                return None
        if proc.poll() is not None:
            return None
    return None

def send(proc, msg):
    data = json.dumps(msg).encode()
    proc.stdin.write(f'Content-Length: {len(data)}\r\n\r\n'.encode() + data)
    proc.stdin.flush()

abspath = os.path.abspath('src/server.c')
uri = 'file://' + abspath

with open('src/server.c') as f:
    text = f.read()

proc = subprocess.Popen(
    ['./build/src/c_tools_lsp'],
    stdin=subprocess.PIPE,
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    bufsize=0
)

# Initialize
send(proc, {
    'jsonrpc': '2.0', 'id': 1, 'method': 'initialize',
    'params': {
        'processId': None, 'rootUri': None, 'capabilities': {},
        'initializationOptions': {'includePaths': [os.path.abspath('src')]}
    }
})
r = recv_response(proc, 5)
print('=== Init ===')
print(json.dumps(r, indent=2))

# initialized
send(proc, {'jsonrpc': '2.0', 'method': 'initialized', 'params': {}})
time.sleep(0.1)

# didOpen
send(proc, {
    'jsonrpc': '2.0', 'method': 'textDocument/didOpen',
    'params': {
        'textDocument': {
            'uri': uri, 'languageId': 'c', 'version': 1, 'text': text
        }
    }
})
print(f'\n=== didOpen URI: {uri} ===')
time.sleep(0.2)

# Complexity request
send(proc, {
    'jsonrpc': '2.0', 'id': 2, 'method': 'textDocument/functionComplexity',
    'params': {
        'textDocument': {'uri': uri},
        'functionName': 'stdin_cb'
    }
})

print('\n=== Complexity ===')
r = recv_response(proc, 10)
print(json.dumps(r, indent=2) if r else '(no response)')

time.sleep(0.3)
if proc.stderr:
    err = proc.stderr.read()
    if err:
        print('\n=== Stderr ===')
        print(err.decode()[-2000:])

proc.terminate()
proc.wait()
print(f'\nExit code: {proc.returncode}')
