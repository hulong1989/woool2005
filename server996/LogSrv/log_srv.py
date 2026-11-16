#!/usr/bin/env python3
"""LogSrv：日志查看。"""
import argparse, json, os
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
LOG = os.path.join(ROOT, "server996", "Log", "GameSrv.log")
class H(BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def do_GET(self):
        try:
            lines = open(LOG, encoding="utf-8", errors="replace").read().splitlines()[-200:]
        except OSError:
            lines = []
        b = json.dumps({"lines": lines}, ensure_ascii=False).encode()
        self.send_response(200); self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(b))); self.end_headers(); self.wfile.write(b)
if __name__ == "__main__":
    ap = argparse.ArgumentParser(); ap.add_argument("--port", type=int, default=7003); a = ap.parse_args()
    print("LogSrv listening", a.port, flush=True)
    ThreadingHTTPServer(("127.0.0.1", a.port), H).serve_forever()
