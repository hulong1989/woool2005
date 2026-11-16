#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""浏览器版游戏客户端桥接。

复用 client/game_client.py 的协议层，把 GameState 序列化为 JSON，
由浏览器 Canvas 渲染等距画面；命令通过 /cmd 回传服务器。
"""
import hashlib
import json
import os
import struct
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLIENT_DIR = os.path.join(ROOT, "client")
ASSETS = os.path.join(CLIENT_DIR, "assets")
WEB = os.path.join(CLIENT_DIR, "web")
sys.path.insert(0, CLIENT_DIR)
import game_client as gc  # noqa: E402

PORT = 8100
LOCK = threading.RLock()

state = gc.GameState()
client = None
connected = False
last_error = ""
token = ""
enc_counter = 0
login_fails = {}


def serialize():
    with LOCK:
        st = state
        ents = []
        for eid, e in st.entities.items():
            ents.append({
                "id": eid, "type": e["type"], "name": e["name"],
                "x": e["x"], "y": e["y"],
                "hp": e.get("hp", 0), "maxhp": e.get("maxhp", 0),
                "dir": e.get("dir", 0),
            })
        terrain = None
        if st.terrain is not None:
            terrain = {
                "w": st.terrain["w"], "h": st.terrain["h"],
                "ox": st.terrain["ox"], "oy": st.terrain["oy"],
                "blocks": list(st.terrain["blocks"]),
            }
        return {
            "connected": connected and bool(token), "auth": bool(token), "error": last_error,
            "name": st.name, "map": st.map_name, "title": st.map_title,
            "x": st.x, "y": st.y, "dir": st.dir,
            "hp": st.hp, "maxhp": st.maxhp, "mp": st.mp, "maxmp": st.maxmp,
            "level": st.level, "exp": st.exp, "maxexp": st.maxexp,
            "gold": st.gold, "player_class": st.player_class,
            "min_atk": st.min_atk, "max_atk": st.max_atk,
            "min_def": st.min_def, "max_def": st.max_def,
            "min_mc": st.min_mc, "max_mc": st.max_mc,
            "min_sc": st.min_sc, "max_sc": st.max_sc,
            "entities": ents, "terrain": terrain,
            "death_left": max(0.0, st.death_at + 2.5 - time.time()) if st.death_at else 0.0,
            "effects": [{
                "kind": e["kind"], "x": e["x"], "y": e["y"],
                "text": e.get("text", ""), "color": e.get("color", "#fff"),
                "left": max(0.0, e["until"] - time.time()),
            } for e in st.effects],
            "characters": st.characters, "party": st.party, "guild": st.guild,
            "dungeons": st.dungeons, "dungeon_records": st.dungeon_records,
            "dungeon_slots": st.dungeon_slots, "trade": st.trade,
            "skills": st.skills, "chat": st.chat[-60:],
            "bag": st.bag, "equip": st.equip,
            "shop": st.shop, "quests": st.quests, "dialog": st.dialog,
            "titles": st.titles, "codex": st.codex, "points": st.available_points,
        }


def poll_loop():
    global connected, last_error
    last_quest = 0.0
    last_map = None
    while True:
        try:
            if client is None:
                time.sleep(0.5)
                continue
            client.poll()
            now = time.time()
            with LOCK:
                connected = client.sock is not None
                pos = (state.x, state.y, state.map_name)
            if pos != last_map:
                last_map = pos
                try:
                    secure_send("mapdata")
                except Exception:
                    pass
            if now - last_quest > 2.0:
                last_quest = now
                try:
                    secure_send("questlog")
                    secure_send("partylist")
                    secure_send("guildinfo")
                    secure_send("dungeonlist")
                except Exception:
                    pass
            time.sleep(0.03)
        except Exception as exc:
            with LOCK:
                connected = False
                last_error = str(exc)
            time.sleep(0.5)


def secure_send(text):
    global enc_counter
    with LOCK:
        c = client
        tk = token
    if c is None:
        return
    if not tk:
        c.send_text(text)
        return
    enc_counter += 1
    seed = tk.encode("utf-8") + struct.pack("<Q", enc_counter)
    ks = hashlib.sha256(seed).hexdigest()
    data = text.encode("utf-8")
    cipher = bytes(b ^ ord(ks[i % len(ks)]) for i, b in enumerate(data))
    c.send_text("enc %x %s" % (enc_counter, cipher.hex()))


def _login_limited(name):
    now = time.time()
    arr = [t for t in login_fails.get(name, []) if now - t < 60]
    login_fails[name] = arr
    return len(arr) >= 5


def _login_failed(name):
    login_fails.setdefault(name, []).append(time.time())


def login_account(name, password, cls="0", register=False):
    global state, client, token, connected, last_error, enc_counter
    name = (name or "").strip()[:20]
    password = (password or "").strip()[:40]
    cls = cls if cls in ("0", "1", "2") else "0"
    if not name or not password:
        return False, "账号和密码不能为空"
    if _login_limited(name):
        return False, "登录失败次数过多，请 1 分钟后再试"
    with LOCK:
        if client is not None:
            try:
                client.sock.close()
            except Exception:
                pass
        state = gc.GameState()
        client = None
        token = ""
        enc_counter = 0
        connected = False
        last_error = ""
    try:
        c = gc.GameClient("127.0.0.1", 7100, state)
        if register:
            c.connect(name, cls, command="register %s %s %s" % (name, password, cls))
            time.sleep(0.2)
            c.send_text("login %s %s %s" % (name, password, cls))
        else:
            c.connect(name, cls, command="login %s %s %s" % (name, password, cls))
        with LOCK:
            client = c
            state.player_class = int(cls)
            connected = True
    except OSError as exc:
        with LOCK:
            last_error = "连接服务器失败：%s" % exc
        return False, last_error

    # 等待服务端下发会话 token
    for _ in range(60):
        with LOCK:
            tk = state.auth_token
            if tk:
                token = tk
                break
            msg = state.chat[-1] if state.chat else ""
        if msg and ("失败" in msg or "错误" in msg or "不存在" in msg):
            with LOCK:
                last_error = msg
            return False, msg
        time.sleep(0.05)

    if not token:
        _login_failed(name)
        with LOCK:
            last_error = last_error or "登录失败（账号不存在或密码错误）"
            connected = False
        return False, last_error
    login_fails.pop(name, None)

    secure_send("charlist")
    with LOCK:
        last_error = ""
    return True, ""


def create_character(name, cls):
    if not token:
        return False, "未登录"
    name = (name or "").strip()[:20]
    cls = cls if cls in ("0", "1", "2") else "0"
    if len(name) < 2:
        return False, "角色名至少 2 个字符"
    secure_send("createchar %s %s" % (name, cls))
    time.sleep(0.4)
    secure_send("charlist")
    return True, ""


def select_character(name):
    global connected
    if not token:
        return False, "未登录"
    with LOCK:
        c = client
    if c is None:
        return False, "未连接"
    secure_send("selectchar " + name)
    for _ in range(40):
        with LOCK:
            if state.name == name:
                connected = True
                break
        time.sleep(0.05)
    with LOCK:
        if state.name != name:
            return False, "进入角色失败"
        state.player_class = next((ch["cls"] for ch in state.characters if ch["name"] == name), 0)
    for cmd in ("skills", "mapdata", "bag", "equipment", "shop", "questlog", "titlelist", "codex"):
        secure_send(cmd)
    return True, ""


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def _json(self, obj, code=200):
        body = json.dumps(obj, ensure_ascii=False).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _file(self, path, ctype):
        try:
            with open(path, "rb") as f:
                body = f.read()
        except OSError:
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        path = urlparse(self.path).path
        if path in ("/", "/index.html"):
            return self._file(os.path.join(WEB, "index.html"), "text/html; charset=utf-8")
        if path == "/game.js":
            return self._file(os.path.join(WEB, "game.js"), "application/javascript; charset=utf-8")
        if path == "/style.css":
            return self._file(os.path.join(WEB, "style.css"), "text/css; charset=utf-8")
        if path == "/state":
            return self._json(serialize())
        if path.startswith("/assets/"):
            rel = path[len("/assets/"):]
            full = os.path.normpath(os.path.join(ASSETS, rel))
            if not full.startswith(ASSETS):
                return self.send_error(403)
            ctype = "application/octet-stream"
            if full.endswith(".gif"):
                ctype = "image/gif"
            elif full.endswith(".png"):
                ctype = "image/png"
            elif full.endswith(".json"):
                ctype = "application/json"
            return self._file(full, ctype)
        self.send_error(404)

    def do_POST(self):
        path = urlparse(self.path).path
        length = int(self.headers.get("Content-Length", 0) or 0)
        try:
            data = json.loads(self.rfile.read(length) or b"{}")
        except Exception:
            return self._json({"ok": False}, 400)
        if path == "/api/logout":
            with LOCK:
                global client, token, enc_counter, state, connected
                if client is not None:
                    try:
                        client.sock.close()
                    except Exception:
                        pass
                client = None
                token = ""
                enc_counter = 0
                state = gc.GameState()
                connected = False
            return self._json({"ok": True})
        if path == "/api/create":
            ok, err = create_character(data.get("name", ""), str(data.get("cls", "0")))
            return self._json({"ok": ok, "error": err})
        if path == "/api/select":
            supplied = self.headers.get("X-Token", "")
            with LOCK:
                ok_token = bool(token) and supplied == token
            if not ok_token:
                return self._json({"ok": False, "error": "未登录"}, 401)
            ok, err = select_character((data.get("name") or "").strip())
            return self._json({"ok": ok, "error": err})
        if path in ("/api/deletechar", "/api/renamechar"):
            supplied = self.headers.get("X-Token", "")
            with LOCK:
                ok_token = bool(token) and supplied == token
            if not ok_token:
                return self._json({"ok": False, "error": "未登录"}, 401)
            if path == "/api/deletechar":
                secure_send("deletechar " + (data.get("name") or "").strip())
            else:
                secure_send("renamechar %s %s" % ((data.get("old") or "").strip(),
                                                  (data.get("new") or "").strip()))
            time.sleep(0.4)
            secure_send("charlist")
            return self._json({"ok": True})
        if path == "/api/password":
            supplied = self.headers.get("X-Token", "")
            with LOCK:
                ok_token = bool(token) and supplied == token
            if not ok_token:
                return self._json({"ok": False, "error": "未登录或 token 失效"}, 401)
            secure_send("changepass %s %s" % (data.get("old", ""), data.get("new", "")))
            return self._json({"ok": True})
        if path in ("/api/login", "/api/register"):
            ok, err = login_account(
                data.get("name", ""), data.get("password", ""),
                str(data.get("cls", "0")), register=(path == "/api/register"))
            return self._json({"ok": ok, "token": token if ok else "", "error": err})
        if path == "/cmd":
            text = (data.get("text") or "").strip()
            supplied = self.headers.get("X-Token", "")
            with LOCK:
                ok_token = bool(token) and supplied == token
            if not ok_token:
                return self._json({"ok": False, "error": "未登录或 token 失效"}, 401)
            if client is not None and text:
                try:
                    secure_send(text)
                    return self._json({"ok": True})
                except Exception as exc:
                    return self._json({"ok": False, "error": str(exc)})
            return self._json({"ok": False, "error": "not connected"})
        self.send_error(404)


def main():
    threading.Thread(target=poll_loop, daemon=True).start()
    server = ThreadingHTTPServer(("127.0.0.1", PORT), Handler)
    print("Web game: http://127.0.0.1:%d" % PORT)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
