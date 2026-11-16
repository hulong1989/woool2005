#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""GameServer Web 管理后台。

- 拥有服务器进程（stdin/stdout），提供 JSON API 与静态页面
- 实时采集：状态、在线玩家、副本实例、Boss、日志
- 可执行：启动/停止/重启、公告、传送、加经验/金币、发物品、踢人、任意控制台命令
"""
import atexit
import json
import os
import re
import signal
import socket
import subprocess
import sys
import threading
import time
from collections import deque
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN = os.path.join(ROOT, "build", "GameServer")
CLIENT = os.path.join(ROOT, "client", "game_client.py")
STATIC = os.path.join(ROOT, "tools", "dashboard")
GAME_PORT = 7100
WEB_PORT = 8099

ANSI = re.compile(r"\x1b\[[0-9;]*m|\x1b\][^\x07]*\x07")

PLAYER_RE = re.compile(r"\s+(.+?)\s+Lv\.(\d+)\s+地图\s+(\d+)\s+\((\d+),(\d+)\)\s+\[(.+?)\]")
DUNGEON_HEAD_RE = re.compile(r"^(.+?)（(\d+) 个实例）：$")
DUNGEON_SLOT_RE = re.compile(
    r"^\s+(\d+)（(.+?)）：(?:空闲（地图 (\d+)）|队伍 (.+?)（地图 (\d+)），剩余 (\d+) 秒)")
BOSS_RE = re.compile(r"^\s+(.+?)\s+地图\s+(\d+)\s+\((\d+),(\d+)\)\s+(.+)$")
STARTUP_RE = re.compile(r"刷出怪物:\s*(\d+)，NPC:\s*(\d+)，任务:\s*(\d+).*Boss:\s*(\d+)")
STATUS_RE = re.compile(r"客户端:\s*(\d+).*测试角色:\s*(\d+)")
SEASON_RE = re.compile(r"当前第\s*(\d+)\s*赛季")
GM_RE = re.compile(r"^\s+(.+?)\s+Lv\.(\d+)$")


class Bridge:
    def __init__(self):
        self.lock = threading.RLock()
        self.proc = None
        self.reader = None
        self.collector = None
        self.stop_flag = False
        self.start_time = 0
        self.logs = deque(maxlen=4000)
        self.log_id = 0
        self.collect_target = None
        self.collected = []
        self.clients_trend = deque(maxlen=40)
        self.state = {
            "running": False, "pid": 0, "port": GAME_PORT,
            "clients": 0, "test": 0, "monsters": 0, "npc": 0,
            "quests": 0, "boss": 0, "season": 1, "dungeon_instances": 0,
            "players": [], "dungeons": [], "bosses": [], "gm": [],
        }

    # ------------------------------------------------------------ process
    def port_busy(self):
        s = socket.socket()
        try:
            s.settimeout(0.25)
            return s.connect_ex(("127.0.0.1", GAME_PORT)) == 0
        finally:
            s.close()

    def start(self):
        with self.lock:
            if self.proc is not None and self.proc.poll() is None:
                return True
            if self.port_busy():
                self.log("端口 %d 已被其它进程占用，未启动" % GAME_PORT, "error")
                return False
            try:
                self.proc = subprocess.Popen(
                    [BIN], cwd=ROOT, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT, text=True, bufsize=1)
            except OSError as exc:
                self.log("启动失败：%s" % exc, "error")
                return False
            self.start_time = time.time()
            self.state["running"] = True
            self.state["pid"] = self.proc.pid
            self.log("服务器进程已启动（PID %d）" % self.proc.pid, "ok")
            self.reader = threading.Thread(target=self._read_loop, daemon=True)
            self.reader.start()
        return True

    def stop(self):
        with self.lock:
            proc = self.proc
            if proc is None:
                self.state["running"] = False
                return
            try:
                if proc.poll() is None:
                    proc.stdin.write("exit\n")
                    proc.stdin.flush()
            except Exception:
                pass
            try:
                proc.wait(timeout=2)
            except Exception:
                try:
                    proc.terminate()
                except Exception:
                    pass
            self.proc = None
            self.state["running"] = False
            self.state["pid"] = 0
            self.log("服务器已停止", "warn")

    def restart(self):
        self.stop()
        time.sleep(0.3)
        return self.start()

    # ------------------------------------------------------------ io
    def send_raw(self, cmd):
        with self.lock:
            proc = self.proc
            if proc is None or proc.poll() is not None:
                self.log("服务器未运行，命令未执行：%s" % cmd, "error")
                return False
            try:
                proc.stdin.write(cmd + "\n")
                proc.stdin.flush()
            except Exception as exc:
                self.log("命令发送失败：%s" % exc, "error")
                return False
        self.log("> " + cmd, "cmd")
        return True

    def _read_loop(self):
        proc = self.proc
        try:
            for raw in proc.stdout:
                line = ANSI.sub("", raw.rstrip("\n"))
                with self.lock:
                    self.log(line, self._tag(line))
                    if self.collect_target is not None:
                        self.collected.append(line)
                    self._parse(line)
        except Exception:
            pass

    def _tag(self, line):
        if any(k in line for k in ("失败", "错误", "未找到", "无效")):
            return "error"
        if any(k in line for k in ("公告", "首杀", "赛季", "通关", "集齐", "冠军")):
            return "warn"
        if any(k in line for k in ("已启动", "客户端接入", "已进入世界", "已保存")):
            return "ok"
        if line.startswith("  ") or line.startswith("可用命令"):
            return "muted"
        return "info"

    def log(self, text, tag="info"):
        self.log_id += 1
        self.logs.append({"id": self.log_id, "t": time.strftime("%H:%M:%S"),
                          "text": text, "tag": tag})

    # ------------------------------------------------------------ parse
    def _parse(self, line):
        m = STATUS_RE.search(line)
        if m:
            self.state["clients"] = int(m.group(1))
            self.state["test"] = int(m.group(2))
            self.clients_trend.append(int(m.group(1)))
            return
        m = STARTUP_RE.search(line)
        if m:
            self.state["monsters"] = int(m.group(1))
            self.state["npc"] = int(m.group(2))
            self.state["quests"] = int(m.group(3))
            self.state["boss"] = int(m.group(4))
            return
        m = SEASON_RE.search(line)
        if m:
            self.state["season"] = int(m.group(1))

    def _collect(self, cmd, duration=0.45):
        with self.lock:
            self.collect_target = cmd
            self.collected = []
        if not self.send_raw(cmd):
            with self.lock:
                self.collect_target = None
            return []
        time.sleep(duration)
        with self.lock:
            self.collect_target = None
            lines = list(self.collected)
            self.collected = []
        return lines

    def _collector_loop(self):
        while not self.stop_flag:
            proc = self.proc
            if proc is None or proc.poll() is not None:
                time.sleep(0.5)
                continue
            status_lines = self._collect("status", 0.2)
            online = self._collect("online", 0.45)
            dungeons = self._collect("dungeon", 0.45)
            bosses = self._collect("boss", 0.45)
            season = self._collect("season status", 0.3)
            gmlist = self._collect("gmlist", 0.3)
            with self.lock:
                self.state["players"] = self._parse_players(online)
                self.state["dungeons"] = self._parse_dungeons(dungeons)
                self.state["dungeon_instances"] = sum(
                    len(d["slots"]) for d in self.state["dungeons"])
                self.state["bosses"] = self._parse_bosses(bosses)
                gm = []
                for line in gmlist:
                    m = GM_RE.match(line)
                    if m:
                        gm.append({"name": m.group(1), "level": int(m.group(2))})
                self.state["gm"] = gm
                for line in season:
                    m = SEASON_RE.search(line)
                    if m:
                        self.state["season"] = int(m.group(1))
            time.sleep(1.0)

    def _parse_players(self, lines):
        players = []
        for line in lines:
            m = PLAYER_RE.match(line)
            if m:
                players.append({
                    "name": m.group(1), "level": int(m.group(2)),
                    "map": int(m.group(3)), "x": int(m.group(4)), "y": int(m.group(5)),
                    "kind": m.group(6),
                })
        return players

    def _parse_dungeons(self, lines):
        groups = []
        current = None
        for line in lines:
            m = DUNGEON_HEAD_RE.match(line)
            if m:
                current = {"name": m.group(1), "total": int(m.group(2)), "slots": []}
                groups.append(current)
                continue
            m = DUNGEON_SLOT_RE.match(line)
            if m and current is not None:
                if m.group(3):
                    current["slots"].append({
                        "index": int(m.group(1)), "difficulty": m.group(2),
                        "state": "idle", "party": "", "map": int(m.group(3)),
                        "remain": 0})
                else:
                    current["slots"].append({
                        "index": int(m.group(1)), "difficulty": m.group(2),
                        "state": "busy", "party": m.group(4),
                        "map": int(m.group(5)),
                        "remain": int(m.group(6)) if m.group(6) else 0})
        return groups

    def _parse_bosses(self, lines):
        bosses = []
        for line in lines:
            m = BOSS_RE.match(line)
            if m:
                bosses.append({
                    "name": m.group(1), "map": int(m.group(2)),
                    "x": int(m.group(3)), "y": int(m.group(4)),
                    "status": m.group(5).strip(),
                })
        return bosses

    # ------------------------------------------------------------ api
    def snapshot(self):
        with self.lock:
            s = dict(self.state)
            s["uptime"] = int(time.time() - self.start_time) if self.start_time else 0
            s["trend"] = list(self.clients_trend)
            return s

    def logs_since(self, since):
        with self.lock:
            return [l for l in self.logs if l["id"] > since]

    # ------------------------------------------------------------ actions
    def action(self, kind, data):
        if kind == "start":
            return self.start()
        if kind == "stop":
            self.stop()
            return True
        if kind == "restart":
            return self.restart()
        if kind == "save":
            return self.send_raw("save")
        if kind == "announce":
            text = (data.get("text") or "").strip()
            return bool(text) and self.send_raw("announce " + text)
        if kind == "command":
            cmd = (data.get("cmd") or "").strip()
            return bool(cmd) and self.send_raw(cmd)
        if kind == "client":
            try:
                subprocess.Popen([sys.executable, CLIENT, "--auto"], cwd=ROOT,
                                 stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                self.log("已启动图形客户端", "ok")
                return True
            except Exception as exc:
                self.log("客户端启动失败：%s" % exc, "error")
                return False
        name = (data.get("name") or "").strip()
        if not name:
            return False
        if kind == "teleport":
            return self.send_raw("teleport %s %s %s %s" % (
                name, data.get("map", 1), data.get("x", 100), data.get("y", 100)))
        if kind == "addexp":
            return self.send_raw("addexp %s %d" % (name, int(data.get("amount", 100))))
        if kind == "addgold":
            return self.send_raw("addgold %s %d" % (name, int(data.get("amount", 100))))
        if kind == "give":
            item = (data.get("item") or "").strip()
            return bool(item) and self.send_raw("give %s %s" % (name, item))
        if kind == "kick":
            return self.send_raw("kickname " + name)
        return False


BRIDGE = Bridge()


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
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
        parsed = urlparse(self.path)
        path = parsed.path
        if path in ("/", "/index.html"):
            return self._file(os.path.join(STATIC, "index.html"), "text/html; charset=utf-8")
        if path == "/style.css":
            return self._file(os.path.join(STATIC, "style.css"), "text/css; charset=utf-8")
        if path == "/app.js":
            return self._file(os.path.join(STATIC, "app.js"), "application/javascript; charset=utf-8")
        if path == "/api/status":
            return self._json(BRIDGE.snapshot())
        if path == "/api/players":
            return self._json({"players": BRIDGE.snapshot()["players"]})
        if path == "/api/dungeons":
            return self._json({"dungeons": BRIDGE.snapshot()["dungeons"]})
        if path == "/api/bosses":
            return self._json({"bosses": BRIDGE.snapshot()["bosses"]})
        if path == "/api/gm":
            return self._json({"gm": BRIDGE.snapshot()["gm"]})
        if path == "/api/config":
            return self._json({
                "root": ROOT, "binary": BIN, "port": GAME_PORT,
                "maps": len([f for f in os.listdir(os.path.join(ROOT, "data", "maps", "logic"))
                             if f.endswith(".ini")]),
                "python": sys.version.split()[0],
            })
        if path == "/api/logs":
            since = int(parse_qs(parsed.query).get("since", ["0"])[0] or 0)
            return self._json({"logs": BRIDGE.logs_since(since)})
        self.send_error(404)

    def do_POST(self):
        parsed = urlparse(self.path)
        length = int(self.headers.get("Content-Length", "0") or 0)
        try:
            data = json.loads(self.rfile.read(length) or b"{}")
        except Exception:
            return self._json({"ok": False, "error": "bad json"}, 400)
        if parsed.path == "/api/action":
            kind = data.get("type", "")
            ok = BRIDGE.action(kind, data)
            return self._json({"ok": bool(ok)})
        self.send_error(404)


def main():
    if not os.path.exists(BIN):
        print("未找到 build/GameServer，请先 make", file=sys.stderr)
        return 1
    BRIDGE.collector = threading.Thread(target=BRIDGE._collector_loop, daemon=True)
    BRIDGE.collector.start()
    BRIDGE.start()
    atexit.register(BRIDGE.stop)

    def _shutdown(signum, frame):
        BRIDGE.stop()
        sys.exit(0)

    signal.signal(signal.SIGTERM, _shutdown)
    signal.signal(signal.SIGINT, _shutdown)

    server = ThreadingHTTPServer(("127.0.0.1", WEB_PORT), Handler)
    print("Dashboard: http://127.0.0.1:%d" % WEB_PORT)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        BRIDGE.stop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
