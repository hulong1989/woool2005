#!/usr/bin/env python3
"""LoginSrv：账号服务（注册/账号信息）。游戏登录仍由 GameSrv 完成。"""
import argparse, hashlib, json, os, random, threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ACC = os.path.join(ROOT, "data", "accounts.txt")
LOCK = threading.Lock()

def load():
    out = {}
    try:
        for line in open(ACC, encoding="utf-8"):
            line = line.strip()
            if not line or line.startswith("#"): continue
            p = line.split("/")
            if len(p) >= 3: out[p[0]] = (p[1], p[2])
    except OSError:
        pass
    return out

def save(d):
    with open(ACC, "w", encoding="utf-8") as f:
        f.write("# name/salt/hash\n")
        for n, (salt, h) in d.items():
            f.write("%s/%s/%s\n" % (n, salt, h))

class H(BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def _j(self, o, code=200):
        b = json.dumps(o, ensure_ascii=False).encode()
        self.send_response(code); self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(b))); self.end_headers(); self.wfile.write(b)
    def do_POST(self):
        n = int(self.headers.get("Content-Length", 0) or 0)
        d = json.loads(self.rfile.read(n) or b"{}")
        if self.path != "/api/register":
            return self._j({"ok": False, "error": "not found"}, 404)
        name = (d.get("name") or "").strip(); pw = (d.get("password") or "").strip()
        if not name or not pw:
            return self._j({"ok": False, "error": "账号或密码为空"})
        with LOCK:
            acc = load()
            if name in acc:
                return self._j({"ok": False, "error": "账号已存在"})
            salt = "".join(random.choice("0123456789abcdef") for _ in range(16))
            h = hashlib.sha256((salt + pw).encode()).hexdigest()
            acc[name] = (salt, h); save(acc)
        return self._j({"ok": True})

if __name__ == "__main__":
    ap = argparse.ArgumentParser(); ap.add_argument("--port", type=int, default=7001); a = ap.parse_args()
    print("LoginSrv listening", a.port, flush=True)
    ThreadingHTTPServer(("127.0.0.1", a.port), H).serve_forever()
