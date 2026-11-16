#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""GameServer 可视化管理控制台（tkinter）。

启动后自动拉起 build/GameServer，实时显示日志、在线玩家、世界统计，
并提供常用管理命令、公告、传送、重启/停止与客户端启动按钮。
"""
import os
import queue
import re
import socket
import subprocess
import sys
import threading
import time
import tkinter as tk
from tkinter import ttk

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN = os.path.join(ROOT, "build", "GameServer")
CLIENT = os.path.join(ROOT, "client", "game_client.py")
PORT = 7100

ANSI = re.compile(r"\x1b\[[0-9;]*m|\x1b\][^\x07]*\x07")


class ServerApp:
    def __init__(self, root):
        self.root = root
        self.proc = None
        self.queue = queue.Queue()
        self.start_time = 0
        self.stats = {"clients": 0, "test": 0, "monsters": 0, "npc": 0,
                      "quests": 0, "boss": 0, "dungeon": 0}
        self.collecting_online = False
        self.dungeon_counts = {}
        self.last_online = 0
        try:
            self.logfile = open("/tmp/server_gui_run.log", "a", encoding="utf-8")
        except Exception:
            self.logfile = None

        root.title("GameServer 可视化管理控制台")
        root.geometry("1180x780")
        root.configure(bg="#141418")

        self._build_ui()
        self._start_server()
        self._drain()
        self._tick()

    # ------------------------------------------------------------------ UI
    def _build_ui(self):
        header = tk.Frame(self.root, bg="#1b1b22")
        header.pack(fill="x")
        tk.Label(header, text="GameServer 管理控制台", fg="#e8c37a", bg="#1b1b22",
                 font=("PingFang SC", 18, "bold")).pack(side="left", padx=14, pady=8)
        self.state_var = tk.StringVar(value="启动中…")
        self.state_label = tk.Label(header, textvariable=self.state_var, fg="#ffcc66",
                                    bg="#1b1b22", font=("Menlo", 12, "bold"))
        self.state_label.pack(side="left", padx=10)
        self.port_var = tk.StringVar(value="端口 %d" % PORT)
        tk.Label(header, textvariable=self.port_var, fg="#9a9aa5", bg="#1b1b22",
                 font=("Menlo", 11)).pack(side="right", padx=12)

        cards = tk.Frame(self.root, bg="#141418")
        cards.pack(fill="x", padx=10, pady=(8, 4))
        self.card_vars = {}
        specs = [("clients", "在线客户端", "#4fb06d"), ("test", "测试角色", "#3fa7d6"),
                 ("monsters", "怪物", "#d65a3f"), ("npc", "NPC", "#e8c37a"),
                 ("quests", "任务", "#7fd0ff"), ("boss", "Boss", "#ff7777"),
                 ("dungeon", "副本实例", "#c8a2ff")]
        for i, (key, label, color) in enumerate(specs):
            card = tk.Frame(cards, bg="#1e1e26", highlightbackground="#2c2c36",
                            highlightthickness=1)
            card.grid(row=0, column=i, sticky="ew", padx=3)
            cards.grid_columnconfigure(i, weight=1)
            tk.Label(card, text=label, fg="#8a8a95", bg="#1e1e26",
                     font=("PingFang SC", 10)).pack(pady=(6, 0))
            var = tk.StringVar(value="0")
            tk.Label(card, textvariable=var, fg=color, bg="#1e1e26",
                     font=("Menlo", 20, "bold")).pack(pady=(0, 6))
            self.card_vars[key] = var

        paned = tk.PanedWindow(self.root, orient="horizontal", bg="#141418",
                               sashwidth=5, sashrelief="flat")
        paned.pack(fill="both", expand=True, padx=10, pady=6)

        left = tk.Frame(paned, bg="#141418")
        paned.add(left, minsize=620)
        logbar = tk.Frame(left, bg="#141418")
        logbar.pack(fill="x")
        tk.Label(logbar, text="服务器日志", fg="#cfcfd8", bg="#141418",
                 font=("PingFang SC", 11, "bold")).pack(side="left")
        tk.Button(logbar, text="清屏", command=self._clear_log, bg="#2a2a33", fg="#dddddd",
                  relief="flat").pack(side="right", padx=2)
        self.log = tk.Text(left, bg="#0f0f14", fg="#c8c8d0", insertbackground="#ffffff",
                           relief="flat", wrap="none", font=("Menlo", 11))
        self.log.pack(fill="both", expand=True)
        for tag, color in (("info", "#c8c8d0"), ("system", "#7fd0ff"),
                           ("ok", "#4fb06d"), ("warn", "#ffcc66"),
                           ("error", "#ff7777"), ("cmd", "#c8a2ff")):
            self.log.tag_configure(tag, foreground=color)

        right = tk.Frame(paned, bg="#141418")
        paned.add(right, minsize=330)
        self._build_players(right)
        self._build_controls(right)

    def _build_players(self, parent):
        tk.Label(parent, text="在线玩家", fg="#cfcfd8", bg="#141418",
                 font=("PingFang SC", 11, "bold")).pack(anchor="w")
        cols = ("name", "level", "map", "pos", "kind")
        self.players = ttk.Treeview(parent, columns=cols, show="headings", height=7)
        for col, text, width in (("name", "名字", 100), ("level", "等级", 50),
                                 ("map", "地图", 50), ("pos", "坐标", 90),
                                 ("kind", "类型", 70)):
            self.players.heading(col, text=text)
            self.players.column(col, width=width, anchor="center")
        self.players.pack(fill="x", pady=4)
        self.players.bind("<<TreeviewSelect>>", self._on_player_select)

        tele = tk.Frame(parent, bg="#141418")
        tele.pack(fill="x", pady=2)
        tk.Label(tele, text="传送:", fg="#9a9aa5", bg="#141418").pack(side="left")
        self.tp_map = tk.StringVar(value="1")
        self.tp_x = tk.StringVar(value="477")
        self.tp_y = tk.StringVar(value="222")
        for var, width in ((self.tp_map, 4), (self.tp_x, 5), (self.tp_y, 5)):
            tk.Entry(tele, textvariable=var, width=width, bg="#22222a", fg="#eeeeee",
                     relief="flat").pack(side="left", padx=1)
        tk.Button(tele, text="传送选中玩家", command=self._teleport_selected,
                  bg="#3fa7d6", fg="#101014", relief="flat").pack(side="left", padx=4)

    def _build_controls(self, parent):
        tk.Label(parent, text="快捷命令", fg="#cfcfd8", bg="#141418",
                 font=("PingFang SC", 11, "bold")).pack(anchor="w", pady=(10, 2))
        grid = tk.Frame(parent, bg="#141418")
        grid.pack(fill="x")
        buttons = [
            ("服务器状态", "status"), ("在线玩家", "online"), ("Boss 状态", "boss"),
            ("副本状态", "dungeon"), ("赛季状态", "season status"),
            ("首杀记录", "firstkills"), ("副本排行", "dungeonrank"),
            ("NPC 列表", "npcs"), ("商店", "shop"), ("保存存档", "save"),
        ]
        for i, (text, cmd) in enumerate(buttons):
            tk.Button(grid, text=text, command=lambda c=cmd: self.send(c),
                      bg="#26262f", fg="#dddddd", relief="flat").grid(
                row=i // 2, column=i % 2, sticky="ew", padx=2, pady=2)
        grid.grid_columnconfigure(0, weight=1)
        grid.grid_columnconfigure(1, weight=1)

        row = tk.Frame(parent, bg="#141418")
        row.pack(fill="x", pady=(8, 2))
        self.cmd_var = tk.StringVar()
        entry = tk.Entry(row, textvariable=self.cmd_var, bg="#22222a", fg="#eeeeee",
                         insertbackground="#ffffff", relief="flat")
        entry.pack(side="left", fill="x", expand=True, ipady=4)
        entry.bind("<Return>", lambda e: self._send_entry())
        tk.Button(row, text="发送", command=self._send_entry, bg="#4fb06d",
                  fg="#101014", relief="flat").pack(side="left", padx=3)

        tk.Label(parent, text="全服公告", fg="#cfcfd8", bg="#141418",
                 font=("PingFang SC", 11, "bold")).pack(anchor="w", pady=(8, 2))
        row2 = tk.Frame(parent, bg="#141418")
        row2.pack(fill="x")
        self.ann_var = tk.StringVar()
        ann = tk.Entry(row2, textvariable=self.ann_var, bg="#22222a", fg="#eeeeee",
                       insertbackground="#ffffff", relief="flat")
        ann.pack(side="left", fill="x", expand=True, ipady=4)
        ann.bind("<Return>", lambda e: self._announce())
        tk.Button(row2, text="公告", command=self._announce, bg="#e8c37a",
                  fg="#101014", relief="flat").pack(side="left", padx=3)

        bottom = tk.Frame(parent, bg="#141418")
        bottom.pack(fill="x", pady=(12, 0))
        tk.Button(bottom, text="启动客户端", command=self._launch_client, bg="#7fd0ff",
                  fg="#101014", relief="flat").pack(side="left", padx=2)
        tk.Button(bottom, text="重启服务器", command=self._restart, bg="#ffcc66",
                  fg="#101014", relief="flat").pack(side="left", padx=2)
        tk.Button(bottom, text="停止服务器", command=self._stop, bg="#d65a3f",
                  fg="#ffffff", relief="flat").pack(side="left", padx=2)

        self.footer_var = tk.StringVar(value="")
        tk.Label(parent, textvariable=self.footer_var, fg="#6f6f7a", bg="#141418",
                 font=("Menlo", 9), anchor="w", justify="left").pack(fill="x", pady=(10, 0))

    # --------------------------------------------------------------- server
    def _port_busy(self):
        s = socket.socket()
        try:
            s.settimeout(0.3)
            return s.connect_ex(("127.0.0.1", PORT)) == 0
        finally:
            s.close()

    def _start_server(self):
        if self.proc is not None or self._port_busy():
            self._append("端口 %d 已被占用，未重复启动。" % PORT, "warn")
            return
        try:
            self.proc = subprocess.Popen(
                [BIN], cwd=ROOT, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT, text=True, bufsize=1)
        except OSError as exc:
            self._append("启动失败：%s" % exc, "error")
            return

        self.start_time = time.time()
        self.state_var.set("运行中")
        self.state_label.configure(fg="#4fb06d")
        threading.Thread(target=self._reader, daemon=True).start()
        self.root.after(900, self._status_tick)
        self.root.after(1500, lambda: self.send("online"))

    def _reader(self):
        try:
            for line in self.proc.stdout:
                self.queue.put(ANSI.sub("", line.rstrip("\n")))
        except Exception:
            pass

    def _stop_process(self, send_exit=True):
        if self.proc is None:
            return
        try:
            if send_exit and self.proc.poll() is None:
                self.proc.stdin.write("exit\n")
                self.proc.stdin.flush()
        except Exception:
            pass
        try:
            self.proc.wait(timeout=2)
        except Exception:
            try:
                self.proc.terminate()
            except Exception:
                pass
        self.proc = None
        self.state_var.set("已停止")
        self.state_label.configure(fg="#ff7777")

    def _restart(self):
        self._append("正在重启服务器…", "warn")
        self._stop_process()
        self.root.after(400, self._start_server)

    def _stop(self):
        self._stop_process()
        self._append("服务器已停止。", "warn")

    def _launch_client(self):
        try:
            subprocess.Popen([sys.executable, CLIENT, "--auto"], cwd=ROOT,
                             stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            self._append("已启动图形客户端。", "ok")
        except Exception as exc:
            self._append("客户端启动失败：%s" % exc, "error")

    # --------------------------------------------------------------- output
    def send(self, cmd):
        if cmd == "online":
            for item in self.players.get_children():
                self.players.delete(item)
            self.collecting_online = True
        if self.proc is None or self.proc.poll() is not None:
            self._append("服务器未运行，无法发送：%s" % cmd, "warn")
            return
        try:
            self.proc.stdin.write(cmd + "\n")
            self.proc.stdin.flush()
            self._append("> " + cmd, "cmd")
        except Exception as exc:
            self._append("发送失败：%s" % exc, "error")

    def _send_entry(self):
        cmd = self.cmd_var.get().strip()
        if cmd:
            self.send(cmd)
            self.cmd_var.set("")

    def _announce(self):
        text = self.ann_var.get().strip()
        if text:
            self.send("announce " + text)
            self.ann_var.set("")

    def _append(self, line, tag="info"):
        self.log.insert("end", line + "\n", tag)
        self.log.see("end")
        if int(self.log.index("end-1c").split(".")[0]) > 4000:
            self.log.delete("1.0", "500.0")

    def _clear_log(self):
        self.log.delete("1.0", "end")

    # ---------------------------------------------------------------- parse
    def _drain(self):
        try:
            while True:
                line = self.queue.get_nowait()
                self._handle_line(line)
        except queue.Empty:
            pass
        self.root.after(80, self._drain)

    def _handle_line(self, line):
        if self.logfile is not None:
            try:
                self.logfile.write(time.strftime("[%H:%M:%S] ") + line + "\n")
                self.logfile.flush()
            except Exception:
                pass

        if line.startswith("服务器: GameServer |"):
            m = re.search(r"客户端:\s*(\d+)", line)
            if m:
                self.stats["clients"] = int(m.group(1))
                self.card_vars["clients"].set(m.group(1))
            m = re.search(r"测试角色:\s*(\d+)", line)
            if m:
                self.stats["test"] = int(m.group(1))
                self.card_vars["test"].set(m.group(1))
            return

        m = re.search(r"刷出怪物:\s*(\d+)，NPC:\s*(\d+)，任务:\s*(\d+).*Boss:\s*(\d+)", line)
        if m:
            for key, val in (("monsters", m.group(1)), ("npc", m.group(2)),
                             ("quests", m.group(3)), ("boss", m.group(4))):
                self.card_vars[key].set(val)
            self._append(line, "ok")
            return

        m = re.match(r"^(.+?)（(\d+) 个实例）", line)
        if m:
            self.dungeon_counts[m.group(1)] = int(m.group(2))
            total = sum(self.dungeon_counts.values())
            self.stats["dungeon"] = total
            self.card_vars["dungeon"].set(str(total))
            self._append(line, "system")
            return

        if line.startswith("  ") and self.collecting_online:
            m = re.match(r"\s+(.+?)\s+Lv\.(\d+)\s+地图\s+(\d+)\s+\((\d+),(\d+)\)\s+\[(.+?)\]", line)
            if m:
                self.players.insert("", "end", values=(
                    m.group(1), m.group(2), m.group(3),
                    "%s,%s" % (m.group(4), m.group(5)), m.group(6)))
                self._append(line, "ok")
                return
        if line.strip() == "（无）":
            self.collecting_online = False
        if self.collecting_online and line and not line.startswith("  "):
            self.collecting_online = False
        tag = "info"
        if any(k in line for k in ("失败", "错误", "错误!", "未找到")):
            tag = "error"
        elif any(k in line for k in ("公告", "首杀", "赛季", "通关", "集齐")):
            tag = "warn"
        elif any(k in line for k in ("客户端接入", "已进入世界", "已启动")):
            tag = "ok"
        elif line.startswith("  ") or line.startswith("可用命令") or line.startswith("  "):
            tag = "system"
        self._append(line, tag)

    def _on_player_select(self, _event=None):
        sel = self.players.selection()
        if sel:
            vals = self.players.item(sel[0], "values")
            if len(vals) >= 4:
                try:
                    x, y = vals[3].split(",")
                    self.tp_x.set(x)
                    self.tp_y.set(y)
                    self.tp_map.set(vals[2])
                except Exception:
                    pass

    def _teleport_selected(self):
        sel = self.players.selection()
        if not sel:
            return
        name = self.players.item(sel[0], "values")[0]
        self.send("teleport %s %s %s %s" % (name, self.tp_map.get(),
                                            self.tp_x.get(), self.tp_y.get()))

    def _status_tick(self):
        if self.proc is not None and self.proc.poll() is None:
            self.send("status")
        self.root.after(3000, self._status_tick)

    # ----------------------------------------------------------------- tick
    def _tick(self):
        if self.proc is not None and self.proc.poll() is None:
            secs = int(time.time() - self.start_time)
            self.footer_var.set(
                "PID %d    运行 %02d:%02d:%02d    监听 0.0.0.0:%d    数据目录 data/" % (
                    self.proc.pid, secs // 3600, (secs % 3600) // 60, secs % 60, PORT))
            if self.stats["clients"] > 0 and time.time() - self.last_online > 8:
                self.last_online = time.time()
                self.send("online")
        else:
            self.footer_var.set("服务器未运行")
        self.root.after(1000, self._tick)

    def on_close(self):
        self._stop_process()
        self.root.destroy()


def main():
    if not os.path.exists(BIN):
        print("未找到 build/GameServer，请先 make", file=sys.stderr)
        return 1
    root = tk.Tk()
    app = ServerApp(root)
    root.protocol("WM_DELETE_WINDOW", app.on_close)
    root.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
