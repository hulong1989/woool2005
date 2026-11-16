#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""图形客户端（headless 模式）端到端验收：
启动服务器 → 客户端登录 → 校验地图/角色/实体/属性 → 下线。
"""
import os
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN = os.path.join(ROOT, "build", "GameServer")
CLIENT = os.path.join(ROOT, "client", "game_client.py")


def main():
    if not os.path.exists(BIN):
        print("未找到 build/GameServer，请先 make", file=sys.stderr)
        return 1

    server = subprocess.Popen(
        [BIN], cwd=ROOT, stdin=subprocess.PIPE,
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    time.sleep(1.0)

    try:
        result = subprocess.run(
            [sys.executable, CLIENT, "--headless", "--name", "客户端验收",
             "--class", "1", "--seconds", "1.2",
             "--commands", "mapdata|buy 金创药 2|bag|equipment|shop|skills|questlog|titlelist|codex|walk 2|cast 火球术"],
            cwd=ROOT, capture_output=True, text=True, timeout=30)
    finally:
        try:
            server.communicate("exit\n", timeout=10)
        except subprocess.TimeoutExpired:
            server.kill()

    out = result.stdout
    print(out.strip())

    checks = [
        ("NAME=客户端验收" in out, "登录角色名"),
        ("MAP=中州" in out, "地图名称"),
        ("LEVEL=1" in out, "等级/属性同步"),
        ("HP=50/50" in out, "HP 同步"),
        ("NPC:杂货商" in out, "NPC 视野实体"),
        ("BAG=1 EQUIP=0 SHOP=28 SKILLS=2 QUESTS=8" in out, "背包/装备/商店/技能/任务快照"),
        ("TERRAIN=61x41" in out and "BLOCKED=99" in out, "地形窗口与阻挡数据"),
        ("TITLES=31 POINTS=0" in out, "称号收集快照"),
        ("CODEX=12 ITEMS=1" in out, "套装图鉴快照"),
        ("金创药 x2" in out, "购买后背包同步"),
    ]
    ok = True
    for passed, label in checks:
        print("  [%s] %s" % ("PASS" if passed else "FAIL", label))
        ok = ok and passed

    if os.path.exists(os.path.join(ROOT, "data", "players", "客户端验收.dat")):
        os.remove(os.path.join(ROOT, "data", "players", "客户端验收.dat"))

    print("\n客户端验收通过 ✅" if ok else "\n客户端验收失败 ❌")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
