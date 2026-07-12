import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SUIYE = ROOT / "bin" / ("suiye.exe" if sys.platform.startswith("win") else "suiye")
DOCS = {}
KEYWORDS = ["println", "print", "let", "set", "const", "if", "else", "while", "repeat", "for", "func", "call", "return", "include", "load", "array", "map", "push", "put", "get", "unset", "now", "random", "typeof", "len", "upper", "lower", "host.hello"]

def read_msg():
    headers = {}
    while True:
        line = sys.stdin.buffer.readline()
        if not line:
            return None
        line = line.decode("ascii", errors="ignore").strip()
        if not line:
            break
        k, _, v = line.partition(":")
        headers[k.lower()] = v.strip()
    length = int(headers.get("content-length", "0"))
    if length <= 0:
        return None
    return json.loads(sys.stdin.buffer.read(length).decode("utf-8"))

def send(payload):
    data = json.dumps(payload, separators=(",", ":")).encode("utf-8")
    sys.stdout.buffer.write(f"Content-Length: {len(data)}\r\n\r\n".encode("ascii") + data)
    sys.stdout.buffer.flush()

def notify(method, params):
    send({"jsonrpc": "2.0", "method": method, "params": params})

def diagnostics(uri, text):
    tmp = ROOT / "build" / "lsp_check.sye"
    tmp.parent.mkdir(exist_ok=True)
    tmp.write_text(text, encoding="utf-8")
    proc = subprocess.run([str(SUIYE), "--check", str(tmp)], cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    items = []
    m = re.search(r":(\d+):(\d+):\s*(.*)", proc.stderr + proc.stdout)
    if proc.returncode != 0 and m:
        line = max(0, int(m.group(1)) - 1)
        col = max(0, int(m.group(2)) - 1)
        items.append({"range": {"start": {"line": line, "character": col}, "end": {"line": line, "character": col + 1}}, "severity": 1, "source": "suiye", "message": m.group(3)})
    notify("textDocument/publishDiagnostics", {"uri": uri, "diagnostics": items})

def main():
    while True:
        msg = read_msg()
        if msg is None:
            break
        method = msg.get("method")
        mid = msg.get("id")
        params = msg.get("params", {})
        if method == "initialize":
            send({"jsonrpc": "2.0", "id": mid, "result": {"capabilities": {"textDocumentSync": 1, "completionProvider": {"resolveProvider": False}}}})
        elif method == "shutdown":
            send({"jsonrpc": "2.0", "id": mid, "result": None})
        elif method == "textDocument/didOpen":
            doc = params["textDocument"]
            DOCS[doc["uri"]] = doc.get("text", "")
            diagnostics(doc["uri"], DOCS[doc["uri"]])
        elif method == "textDocument/didChange":
            uri = params["textDocument"]["uri"]
            DOCS[uri] = params["contentChanges"][-1].get("text", "")
            diagnostics(uri, DOCS[uri])
        elif method == "textDocument/completion":
            send({"jsonrpc": "2.0", "id": mid, "result": [{"label": k, "kind": 14} for k in KEYWORDS]})
        elif mid is not None:
            send({"jsonrpc": "2.0", "id": mid, "result": None})

if __name__ == "__main__":
    main()
