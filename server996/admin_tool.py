#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""996 引擎 · 服务器管理工具（Pygame）

功能：
  概览  启动/停止 996 进程组、端口状态
  配置  gameserver.ini / engine.json 常用参数
  数据  经验、职业、技能、怪物、掉落、商店、事件、NPC 表格编辑
  日志  GameSrv 实时日志
"""
import json
import os
import shutil
import socket
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(ROOT, "vendor"))
import pygame  # noqa: E402

W, H = 1180, 760
FONT = "/System/Library/Fonts/STHeiti Medium.ttc"
GOLD = (201, 162, 74)
PALE = (238, 231, 214)
DIM = (145, 155, 170)
RED = (190, 65, 55)
GREEN = (90, 195, 125)
BG = (7, 9, 13)

pygame.init()
screen = pygame.display.set_mode((W, H))
pygame.display.set_caption("996 引擎 · 服务器管理工具")
clock = pygame.time.Clock()

_f = {}
def font(s):
    if s not in _f:
        _f[s] = pygame.font.Font(FONT, s)
    return _f[s]

def text(s, size, color=PALE):
    return font(size).render(str(s), True, color)

def panel(rect, title=None):
    pygame.draw.rect(screen, (5, 6, 9), rect)
    pygame.draw.rect(screen, (24, 29, 39), rect.inflate(-6, -6))
    pygame.draw.rect(screen, (120, 128, 140), rect, 2)
    pygame.draw.rect(screen, (145, 112, 52), rect.inflate(-10, -10), 1)
    if title:
        bar = pygame.Rect(rect.x + 12, rect.y + 8, rect.w - 24, 28)
        pygame.draw.rect(screen, (75, 20, 16), bar)
        pygame.draw.rect(screen, (170, 120, 55), bar, 1)
        t = text(title, 15, (250, 225, 170))
        screen.blit(t, (bar.centerx - t.get_width() // 2, bar.y + 5))

class Admin:
    def __init__(self):
        self.tab = "概览"
        self.buttons = []
        self.active = None
        self.fields = {}
        self.msg = ""
        self.table_file = "levels.txt"
        self.table_header = []
        self.table_rows = []
        self.table_comments = []
        self.row_sel = 0
        self.col_sel = 0
        self.col_scroll = 0
        self.cell_edit = False
        self.cell_buf = ""
        self.log_scroll = 0
        self.load_config()
        self.load_table(self.table_file)

    # ------------------------------------------------------------- config
    def load_config(self):
        self.fields = {}
        ini = os.path.join(ROOT, "gameserver.ini")
        name, port = "GameServer", "7100"
        section = ""
        try:
            for line in open(ini, encoding="utf-8"):
                s = line.strip()
                if s.startswith("[") and s.endswith("]"):
                    section = s[1:-1]
                elif "=" in s and not s.startswith(";"):
                    k, v = s.split("=", 1)
                    if section == "server" and k.strip() == "name":
                        name = v.strip()
                    if section == "server" and k.strip() == "port":
                        port = v.strip()
        except OSError:
            pass
        try:
            cfg = json.load(open(os.path.join(HERE, "Config", "engine.json"), encoding="utf-8"))
        except Exception:
            cfg = {}
        self.fields["name"] = name
        self.fields["port"] = port
        self.fields["gate"] = str(cfg.get("GameGate", 7101))
        self.fields["backup"] = str(cfg.get("backup_seconds", 60))

    def save_config(self):
        ini = os.path.join(ROOT, "gameserver.ini")
        lines = []
        section = ""
        seen_name = seen_port = False
        try:
            src = open(ini, encoding="utf-8").read().splitlines()
        except OSError:
            src = ["[server]", "name=GameServer", "addr=0.0.0.0", "port=7100"]
        for line in src:
            s = line.strip()
            if s.startswith("[") and s.endswith("]"):
                if section == "server":
                    if not seen_name:
                        lines.append("name=" + self.fields["name"]); seen_name = True
                    if not seen_port:
                        lines.append("port=" + self.fields["port"]); seen_port = True
                section = s[1:-1]
            if section == "server" and "=" in line and not s.startswith(";"):
                k = line.split("=", 1)[0].strip()
                if k == "name":
                    line = "name=" + self.fields["name"]; seen_name = True
                elif k == "port":
                    line = "port=" + self.fields["port"]; seen_port = True
            lines.append(line)
        if section == "server":
            if not seen_name: lines.append("name=" + self.fields["name"])
            if not seen_port: lines.append("port=" + self.fields["port"])
        open(ini, "w", encoding="utf-8").write("\n".join(lines) + "\n")

        cfg_path = os.path.join(HERE, "Config", "engine.json")
        try:
            cfg = json.load(open(cfg_path, encoding="utf-8"))
        except Exception:
            cfg = {}
        cfg["GameGate"] = int(self.fields["gate"])
        cfg["GameSrv"] = int(self.fields["port"])
        cfg["backup_seconds"] = int(self.fields["backup"])
        json.dump(cfg, open(cfg_path, "w", encoding="utf-8"), ensure_ascii=False, indent=2)
        self.msg = "配置已保存，重启服务器后生效。"

    # -------------------------------------------------------------- tables
    def load_table(self, name):
        self.table_file = name
        path = os.path.join(ROOT, "data", name)
        self.table_header, self.table_rows, self.table_comments = [], [], []
        try:
            for line in open(path, encoding="utf-8"):
                s = line.rstrip("\n")
                if not s.strip():
                    self.table_comments.append(line)
                elif s.lstrip().startswith("#"):
                    if "/" in s and not self.table_header:
                        self.table_header = [x.strip() for x in s.lstrip("# ").split("/")]
                    self.table_comments.append(line)
                else:
                    self.table_rows.append([x.strip() for x in s.split("/")])
        except OSError:
            self.msg = "读取失败：" + name
        self.row_sel = min(self.row_sel, max(0, len(self.table_rows) - 1))
        self.col_sel = 0
        self.col_scroll = 0
        self.cell_edit = False

    def save_table(self):
        path = os.path.join(ROOT, "data", self.table_file)
        out = list(self.table_comments)
        for row in self.table_rows:
            out.append("/".join(row))
        open(path, "w", encoding="utf-8").write("\n".join(out) + "\n")
        self.msg = "已保存 data/%s，服务端约 1 秒内热更新" % self.table_file

    def add_row(self):
        n = max(1, len(self.table_rows[0]) if self.table_rows else len(self.table_header))
        self.table_rows.append(["0"] * n)
        self.row_sel = len(self.table_rows) - 1

    def del_row(self):
        if self.table_rows:
            self.table_rows.pop(self.row_sel)
            self.row_sel = max(0, min(self.row_sel, len(self.table_rows) - 1))

    # -------------------------------------------------------------- process
    def running(self):
        s = socket.socket(); s.settimeout(0.15)
        try:
            return s.connect_ex(("127.0.0.1", int(self.fields.get("gate", 7101)))) == 0
        finally:
            s.close()

    def start_server(self):
        if self.running():
            self.msg = "服务器已经在运行。"
            return
        log = open(os.path.join(HERE, "Log", "AdminStart.log"), "ab", buffering=0)
        subprocess.Popen([sys.executable, os.path.join(HERE, "engine996.py")], cwd=ROOT,
                         stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
        self.msg = "正在启动 996 进程组…"

    def stop_server(self):
        subprocess.run(["pkill", "-TERM", "-f", "server996/engine996.py"])
        self.msg = "已发送停止指令，等待存档。"

    def backup(self):
        dst = os.path.join(HERE, "Share", "manual_" + time.strftime("%Y%m%d_%H%M%S"))
        try:
            shutil.copytree(os.path.join(ROOT, "data"), dst)
            self.msg = "已备份到 " + dst
        except Exception as exc:
            self.msg = "备份失败：" + str(exc)

    # ----------------------------------------------------------------- ui
    def field(self, rect, key, placeholder=""):
        pygame.draw.rect(screen, (12, 15, 21), rect)
        pygame.draw.rect(screen, GOLD if self.active == key else (95, 105, 120), rect, 1)
        val = self.fields.get(key, "")
        screen.blit(text(val or placeholder, 13, PALE), (rect.x + 7, rect.y + 5))
        self.buttons.append((rect, lambda k=key: setattr(self, "active", k)))

    def button(self, rect, label, cb, active=False):
        h = rect.collidepoint(pygame.mouse.get_pos())
        base = (85, 92, 108) if h else ((115, 85, 36) if active else (40, 46, 58))
        pygame.draw.rect(screen, base, rect)
        pygame.draw.rect(screen, (160, 168, 180), rect, 1)
        t = text(label, 12, PALE)
        screen.blit(t, (rect.centerx - t.get_width() // 2, rect.centery - t.get_height() // 2))
        self.buttons.append((rect, cb))

    def draw_tabs(self):
        tabs = ["概览", "配置", "数据表", "日志"]
        for i, t in enumerate(tabs):
            r = pygame.Rect(16, 78 + i * 48, 120, 40)
            sel = self.tab == t
            pygame.draw.rect(screen, (70, 58, 30) if sel else (30, 36, 46), r)
            pygame.draw.rect(screen, GOLD if sel else (100, 108, 122), r, 1)
            tx = text(t, 14, PALE)
            screen.blit(tx, (r.centerx - tx.get_width() // 2, r.y + 10))
            self.buttons.append((r, lambda t=t: setattr(self, "tab", t)))

    def draw_overview(self):
        box = pygame.Rect(156, 78, 1000, 500)
        panel(box, "服务器状态")
        ports = [("GameGate", int(self.fields.get("gate", 7101))),
                 ("GameSrv", int(self.fields.get("port", 7100))),
                 ("LoginSrv", 7001), ("DBSrv", 7002), ("LogSrv", 7003)]
        y = box.y + 62
        for name, p in ports:
            s = socket.socket(); s.settimeout(0.12)
            try: on = s.connect_ex(("127.0.0.1", p)) == 0
            finally: s.close()
            pygame.draw.circle(screen, GREEN if on else RED, (box.x + 36, y + 8), 7)
            screen.blit(text(name, 14, PALE), (box.x + 58, y))
            screen.blit(text(str(p), 13, GOLD), (box.x + 210, y))
            screen.blit(text("运行" if on else "停止", 13, GREEN if on else RED), (box.x + 300, y))
            y += 40
        self.button(pygame.Rect(box.x + 50, box.bottom - 70, 150, 36), "启动服务器", self.start_server, True)
        self.button(pygame.Rect(box.x + 215, box.bottom - 70, 150, 36), "停止服务器", self.stop_server)
        self.button(pygame.Rect(box.x + 380, box.bottom - 70, 150, 36), "手动备份", self.backup)
        screen.blit(text("启动器：server996/engine996.py", 12, DIM), (box.x + 560, box.bottom - 62))

    def draw_config(self):
        box = pygame.Rect(156, 78, 1000, 520)
        panel(box, "基础配置")
        rows = [("区服名称", "name"), ("GameSrv 端口", "port"),
                ("GameGate 端口", "gate"), ("备份间隔(秒)", "backup")]
        y = box.y + 62
        for lab, key in rows:
            screen.blit(text(lab, 14, PALE), (box.x + 40, y + 8))
            self.field(pygame.Rect(box.x + 220, y, 260, 34), key)
            y += 52
        self.button(pygame.Rect(box.x + 40, y + 12, 150, 36), "保存配置", self.save_config, True)
        self.button(pygame.Rect(box.x + 210, y + 12, 120, 36), "重新载入", self.load_config)
        screen.blit(text("修改端口后需要重启 GameSrv / GameGate。", 12, DIM), (box.x + 360, y + 22))

    def draw_data(self):
        files = ["levels.txt", "classes.txt", "skills.txt", "monsters.txt", "drops.txt",
                 "shop.txt", "events.txt", "npcs.txt", "quests.txt", "bosses.txt", "sets.txt"]
        box = pygame.Rect(156, 78, 1000, 600)
        panel(box, "数据表编辑")
        for i, fn in enumerate(files):
            r = pygame.Rect(box.x + 16 + (i % 6) * 158, box.y + 44 + (i // 6) * 30, 150, 26)
            sel = fn == self.table_file
            pygame.draw.rect(screen, (80, 64, 30) if sel else (28, 34, 44), r)
            pygame.draw.rect(screen, GOLD if sel else (90, 100, 115), r, 1)
            screen.blit(text(fn.replace(".txt", ""), 11, PALE), (r.x + 8, r.y + 5))
            self.buttons.append((r, lambda f=fn: self.load_table(f)))
        # 表格
        grid = pygame.Rect(box.x + 16, box.y + 110, box.w - 32, 400)
        pygame.draw.rect(screen, (12, 15, 21), grid)
        pygame.draw.rect(screen, (100, 108, 122), grid, 1)
        cols = max(1, len(self.table_header))
        visible = 7
        cw = max(110, (grid.w - 40) // visible)
        rh = 24
        for c in range(self.col_scroll, min(cols, self.col_scroll + visible)):
            head = self.table_header[c] if c < len(self.table_header) else "col%d" % c
            screen.blit(text(head[:8], 11, GOLD), (grid.x + 10 + (c - self.col_scroll) * cw, grid.y + 6))
        start = max(0, self.row_sel - 12)
        for ri in range(start, min(len(self.table_rows), start + 14)):
            y = grid.y + 30 + (ri - start) * rh
            if ri == self.row_sel:
                pygame.draw.rect(screen, (60, 52, 30), (grid.x + 2, y - 2, grid.w - 4, rh))
            for c in range(self.col_scroll, min(cols, self.col_scroll + visible)):
                val = self.table_rows[ri][c] if c < len(self.table_rows[ri]) else ""
                if ri == self.row_sel and c == self.col_sel and self.cell_edit:
                    val = self.cell_buf
                    pygame.draw.rect(screen, (80, 70, 35), (grid.x + 6 + (c - self.col_scroll) * cw, y - 1, cw - 4, rh))
                screen.blit(text(val[:12], 11, PALE), (grid.x + 10 + (c - self.col_scroll) * cw, y + 2))
        # 操作
        by = box.bottom - 48
        self.button(pygame.Rect(box.x + 16, by, 100, 30), "保存", self.save_table, True)
        self.button(pygame.Rect(box.x + 126, by, 90, 30), "增加行", self.add_row)
        self.button(pygame.Rect(box.x + 226, by, 90, 30), "删除行", self.del_row)
        self.button(pygame.Rect(box.x + 326, by, 90, 30), "左移", lambda: setattr(self, "col_scroll", max(0, self.col_scroll - 1)))
        self.button(pygame.Rect(box.x + 426, by, 90, 30), "右移", lambda: setattr(self, "col_scroll", self.col_scroll + 1))
        screen.blit(text("点击单元格后用方向键选择，Enter 编辑，Esc 取消。", 11, DIM), (box.x + 540, by + 8))

    def draw_log(self):
        box = pygame.Rect(156, 78, 1000, 600)
        panel(box, "GameSrv 日志")
        path = os.path.join(HERE, "Log", "GameSrv.log")
        try:
            lines = open(path, encoding="utf-8", errors="replace").read().splitlines()[-28:]
        except OSError:
            lines = ["<无日志>"]
        y = box.y + 52
        for line in lines:
            clean = line.replace("\x1b", "").strip()
            screen.blit(text(clean[:100], 11, (200, 208, 200)), (box.x + 22, y))
            y += 18
        self.button(pygame.Rect(box.x + 22, box.bottom - 50, 110, 30), "刷新", lambda: None)

    def handle(self, ev):
        if ev.type == pygame.QUIT:
            return False
        if ev.type == pygame.MOUSEBUTTONDOWN:
            pos = pygame.mouse.get_pos()
            for r, cb in reversed(self.buttons):
                if r.collidepoint(pos):
                    cb(); return True
            if self.tab == "数据表":
                box = pygame.Rect(156, 78, 1000, 600)
                grid = pygame.Rect(box.x + 16, box.y + 110, box.w - 32, 400)
                if grid.collidepoint(pos):
                    cols = max(1, len(self.table_header))
                    visible = 7
                    cw = max(110, (grid.w - 40) // visible)
                    rh = 24
                    start = max(0, self.row_sel - 12)
                    col = self.col_scroll + (pos[0] - grid.x - 10) // cw
                    row = start + (pos[1] - grid.y - 30) // rh
                    if 0 <= row < len(self.table_rows) and 0 <= col < cols:
                        self.row_sel, self.col_sel = row, col
                        self.cell_edit = False
        elif ev.type == pygame.KEYDOWN:
            if self.active:
                if ev.key in (pygame.K_RETURN, pygame.K_ESCAPE, pygame.K_TAB):
                    self.active = None
                elif ev.key == pygame.K_BACKSPACE:
                    self.fields[self.active] = self.fields.get(self.active, "")[:-1]
                elif ev.unicode and ev.unicode.isprintable():
                    self.fields[self.active] = self.fields.get(self.active, "") + ev.unicode
                return True
            if self.tab == "数据表" and self.table_rows:
                if self.cell_edit:
                    if ev.key == pygame.K_RETURN:
                        while len(self.table_rows[self.row_sel]) <= self.col_sel:
                            self.table_rows[self.row_sel].append("")
                        self.table_rows[self.row_sel][self.col_sel] = self.cell_buf
                        self.cell_edit = False
                    elif ev.key == pygame.K_ESCAPE:
                        self.cell_edit = False
                    elif ev.key == pygame.K_BACKSPACE:
                        self.cell_buf = self.cell_buf[:-1]
                    elif ev.unicode and ev.unicode.isprintable():
                        self.cell_buf += ev.unicode
                    return True
                if ev.key == pygame.K_UP: self.row_sel = max(0, self.row_sel - 1)
                elif ev.key == pygame.K_DOWN: self.row_sel = min(len(self.table_rows) - 1, self.row_sel + 1)
                elif ev.key == pygame.K_LEFT: self.col_sel = max(0, self.col_sel - 1)
                elif ev.key == pygame.K_RIGHT: self.col_sel += 1
                elif ev.key == pygame.K_RETURN:
                    self.cell_edit = True
                    self.cell_buf = self.table_rows[self.row_sel][self.col_sel] if self.col_sel < len(self.table_rows[self.row_sel]) else ""
                return True
        return True

    def run(self):
        run = True
        while run:
            for ev in pygame.event.get():
                run = self.handle(ev) and run
            self.buttons = []
            screen.fill(BG)
            pygame.draw.rect(screen, (70, 18, 16), (0, 0, W, 52))
            pygame.draw.rect(screen, (170, 120, 55), (0, 50, W, 2))
            screen.blit(text("996 引擎 · 服务器管理工具", 22, (250, 230, 180)), (22, 12))
            screen.blit(text("经典一区", 13, (220, 200, 150)), (W - 120, 20))
            self.draw_tabs()
            if self.tab == "概览": self.draw_overview()
            elif self.tab == "配置": self.draw_config()
            elif self.tab == "数据表": self.draw_data()
            else: self.draw_log()
            if self.msg:
                screen.blit(text(self.msg, 12, (235, 180, 110)), (160, H - 34))
            pygame.display.flip()
            clock.tick(20)
        pygame.quit()

if __name__ == "__main__":
    a = Admin()
    if "--data" in sys.argv:
        a.tab = "数据表"
    a.run()
