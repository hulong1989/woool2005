#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""无头截取桌面客户端界面截图（用于 README / 文档）。

原理：用 SDL dummy 视频驱动运行 `client/desktop_client.py` 的渲染代码，
自动登录 → 选角 → 进图，然后保存若干界面到 `docs/screenshots/`。
不弹窗口、不需要显示器，但**需要本机已有可运行的 GameServer**。

前置条件：
    1. 已在工程根目录 make 并启动 ./build/GameServer（监听 7100）
    2. data/accounts.txt 中存在测试账号（仓库内置 `网页演示 / 123456`）
    3. 该账号下有角色（仓库内置 characters.txt 已包含；无存档时会按新角色进入）
    4. macOS：客户端字体使用系统 STHeiti（其它平台需先改 client/desktop_client.py 的 FONT_PATH）

用法：
    python3 tools/capture_screenshots.py                  # 默认连 127.0.0.1:7100
    python3 tools/capture_screenshots.py --port 7101      # 经 GameGate 时
    python3 tools/capture_screenshots.py --out /tmp/shots # 改输出目录

输出（默认 docs/screenshots/）：
    01-login.png  登录界面
    02-characters.png  角色选择
    03-world.png  游戏世界 + HUD + 小地图 + 任务追踪
    04-panel-bag.png  背包面板
    05-panel-dungeon.png  副本面板
"""
import argparse
import os
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    parser = argparse.ArgumentParser(description="无头截取桌面客户端截图")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", default=7100, type=int)
    parser.add_argument("--account", default="网页演示")
    parser.add_argument("--password", default="123456")
    parser.add_argument("--character", default=None, help="默认选第一个角色")
    parser.add_argument("--out", default=os.path.join(ROOT, "docs", "screenshots"))
    args = parser.parse_args()

    os.environ["SDL_VIDEODRIVER"] = "dummy"
    os.environ["SDL_AUDIODRIVER"] = "dummy"
    os.environ["WOOOL_PORT"] = str(args.port)
    os.environ.setdefault("WOOOL_HOST", args.host)

    sys.path.insert(0, os.path.join(ROOT, "vendor"))
    sys.path.insert(0, os.path.join(ROOT, "client"))

    import pygame  # noqa: E402
    import desktop_client as dc  # noqa: E402

    os.makedirs(args.out, exist_ok=True)

    def save(name):
        pygame.display.flip()
        path = os.path.join(args.out, name)
        pygame.image.save(pygame.display.get_surface(), path)
        print("已保存", os.path.relpath(path, ROOT))

    client = dc.Desktop()

    client.buttons, client.tooltip = [], None
    client.draw_login()
    save("01-login.png")

    client.account, client.password = args.account, args.password
    client.login(False)
    deadline = time.time() + 3
    while time.time() < deadline and not (client.state and client.state.characters):
        client.client.poll()
        time.sleep(0.05)
    if not (client.state and client.state.characters):
        print("登录或角色列表获取失败：%s" % client.msg, file=sys.stderr)
        return 1

    client.buttons, client.tooltip = [], None
    client.draw_chars()
    save("02-characters.png")

    character = args.character or client.state.characters[0]["name"]
    client.select_char(character)
    deadline = time.time() + 5
    while time.time() < deadline:
        client.client.poll()
        client.autopilot()
        time.sleep(0.05)
        if client.state and client.state.terrain and client.state.entities:
            break
    for _ in range(20):
        client.client.poll()
        time.sleep(0.03)

    client.buttons, client.tooltip = [], None
    client.draw_world()
    client.draw_target()
    client.draw_effects()
    client.draw_panel()
    client.draw_dialog()
    save("03-world.png")

    for panel, filename in (("背包", "04-panel-bag.png"), ("副本", "05-panel-dungeon.png")):
        client.panel = panel
        client.buttons, client.tooltip = [], None
        client.draw_world()
        client.draw_panel()
        save(filename)

    print("完成：%d 张截图 -> %s" % (5, os.path.relpath(args.out, ROOT)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
