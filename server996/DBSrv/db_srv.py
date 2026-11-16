#!/usr/bin/env python3
"""DBSrv：存档备份与健康检查。"""
import argparse, json, os, shutil, threading, time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, "data")
BACKUP = os.path.join(ROOT, "server996", "Share", "backup")

def backup():
    os.makedirs(BACKUP, exist_ok=True)
    for name in ("players", "guilds.dat", "guildware.dat", "boss_kills.dat",
                 "dungeon_rank.dat", "dungeon_first.dat", "season.dat",
                 "accounts.txt", "characters.txt"):
        src = os.path.join(SRC, name); dst = os.path.join(BACKUP, name)
        try:
            if os.path.isdir(src):
                if os.path.exists(dst): shutil.rmtree(dst)
                shutil.copytree(src, dst)
            elif os.path.exists(src): shutil.copy2(src, dst)
        except OSError:
            pass

def loop(sec):
    while True:
        backup(); time.sleep(sec)

class H(BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def do_GET(self):
        if self.path == "/backup": backup()
        files = len(os.listdir(os.path.join(SRC, "players"))) if os.path.isdir(os.path.join(SRC, "players")) else 0
        b = json.dumps({"ok": True, "players": files, "backup": BACKUP}).encode()
        self.send_response(200); self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(b))); self.end_headers(); self.wfile.write(b)

if __name__ == "__main__":
    ap = argparse.ArgumentParser(); ap.add_argument("--port", type=int, default=7002)
    ap.add_argument("--seconds", type=int, default=60); a = ap.parse_args()
    threading.Thread(target=loop, args=(a.seconds,), daemon=True).start()
    print("DBSrv listening", a.port, flush=True)
    ThreadingHTTPServer(("127.0.0.1", a.port), H).serve_forever()
