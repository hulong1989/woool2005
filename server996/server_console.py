#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""996 风格服务器控制台（Pygame）"""
import json, os, socket, sys, time
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "vendor"))
import pygame
pygame.init()
W, H = 1000, 640
screen = pygame.display.set_mode((W, H))
pygame.display.set_caption("996 引擎 · 服务器控制台")
clock = pygame.time.Clock()
FONT = "/System/Library/Fonts/STHeiti Medium.ttc"
_f = {}
def font(s):
    if s not in _f: _f[s] = pygame.font.Font(FONT, s)
    return _f[s]
def text(s, size, color):
    return font(size).render(str(s), True, color)

CFG = json.load(open(os.path.join(ROOT, "server996", "Config", "engine.json"), encoding="utf-8"))
LOG = os.path.join(ROOT, "server996", "Log", "GameSrv.log")
GOLD = (201, 162, 74)
PALE = (238, 231, 214)
GREEN = (90, 200, 130)
RED = (220, 80, 70)

def online(port):
    s = socket.socket(); s.settimeout(0.15)
    try:
        return s.connect_ex(("127.0.0.1", int(port))) == 0
    finally:
        s.close()

def panel(rect, title=None):
    pygame.draw.rect(screen, (6, 7, 10), rect)
    pygame.draw.rect(screen, (24, 29, 38), rect.inflate(-6, -6))
    pygame.draw.rect(screen, (120, 128, 140), rect, 2)
    pygame.draw.rect(screen, (145, 112, 52), rect.inflate(-10, -10), 1)
    if title:
        bar = pygame.Rect(rect.x + 12, rect.y + 8, rect.w - 24, 28)
        pygame.draw.rect(screen, (75, 20, 16), bar)
        pygame.draw.rect(screen, (170, 120, 55), bar, 1)
        t = text(title, 15, (250, 225, 170))
        screen.blit(t, (bar.centerx - t.get_width() // 2, bar.y + 5))

def tail(n=16):
    try:
        lines = open(LOG, encoding="utf-8", errors="replace").read().splitlines()
        return lines[-n:]
    except OSError:
        return ["<无日志>"]

components = [("GameGate", 7101), ("GameSrv", 7100), ("LoginSrv", 7001),
              ("DBSrv", 7002), ("LogSrv", 7003)]
buttons = []
last = 0
status = {}
players = 0

while True:
    for e in pygame.event.get():
        if e.type == pygame.QUIT:
            pygame.quit(); sys.exit(0)
        if e.type == pygame.MOUSEBUTTONDOWN:
            for r, cb in buttons:
                if r.collidepoint(e.mouse.get_pos()): cb()
    now = time.time()
    if now - last > 1:
        last = now
        status = {n: online(p) for n, p in components}
        try: players = len(os.listdir(os.path.join(ROOT, "data", "players")))
        except OSError: players = 0
    screen.fill((7, 8, 11))
    # 顶栏
    pygame.draw.rect(screen, (70, 18, 16), (0, 0, W, 52))
    pygame.draw.rect(screen, (170, 120, 55), (0, 50, W, 2))
    title = text("996 引擎 · 服务器控制台", 22, (250, 230, 180))
    screen.blit(title, (24, 13))
    screen.blit(text("经典一区", 13, (220, 200, 150)), (W - 130, 20))
    # 左：组件
    left = pygame.Rect(18, 70, 300, 360)
    panel(left, "服务组件")
    y = left.y + 52
    for name, port in components:
        on = status.get(name, False)
        pygame.draw.circle(screen, GREEN if on else RED, (left.x + 28, y + 9), 7)
        screen.blit(text(name, 14, PALE), (left.x + 46, y))
        screen.blit(text("%d" % port, 12, GOLD), (left.x + 160, y + 2))
        screen.blit(text("运行" if on else "停止", 12, GREEN if on else RED), (left.x + 225, y + 2))
        y += 42
    # 左下统计
    info = pygame.Rect(18, 450, 300, 150)
    panel(info, "运行数据")
    screen.blit(text("在线端口  %d / 5" % sum(status.values()), 13, PALE), (info.x + 24, info.y + 54))
    screen.blit(text("角色存档  %d" % players, 13, PALE), (info.x + 24, info.y + 82))
    screen.blit(text("数据目录  " + os.path.join(ROOT, "data"), 10, (160, 170, 180)), (info.x + 24, info.y + 110))
    # 右：日志
    right = pygame.Rect(334, 70, 648, 530)
    panel(right, "GameSrv 日志")
    yy = right.y + 54
    for line in tail(22):
        clean = line.replace("\x1b", "").strip()
        screen.blit(text(clean[:72], 11, (205, 210, 200)), (right.x + 20, yy))
        yy += 20
    # 按钮
    buttons = []
    for i, (lab, cb) in enumerate([
        ("刷新", lambda: None),
        ("打开日志目录", lambda: os.system("open " + os.path.join(ROOT, "server996", "Log"))),
        ("退出控制台", lambda: (pygame.quit(), sys.exit(0))),
    ]):
        r = pygame.Rect(20 + i * 150, H - 46, 136, 30)
        pygame.draw.rect(screen, (45, 52, 66), r)
        pygame.draw.rect(screen, (160, 168, 180), r, 1)
        t = text(lab, 12, PALE)
        screen.blit(t, (r.centerx - t.get_width() // 2, r.y + 7))
        buttons.append((r, cb))
    pygame.display.flip()
    clock.tick(10)
