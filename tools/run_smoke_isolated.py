#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""在临时副本里运行 176 项回归测试，不污染工作区。

`tools/smoke_test.py` 会删改 data/ 下的运行态文件并写入 data/players/，
直接在工作区跑会把仓库搞脏。本脚本把 build/、data/、tools/、gameserver.ini
复制到临时目录后再跑，Makefile 的 `make check` 与 CI 共用这一入口。

用法：
    python3 tools/run_smoke_isolated.py

退出码：与 smoke_test.py 一致（0 = 全部通过）
"""
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
COPY_NAMES = ["build", "data", "tools", "gameserver.ini"]


def main():
    if not os.path.isfile(os.path.join(ROOT, "build", "GameServer")):
        print("缺少 build/GameServer，请先 make", file=sys.stderr)
        return 1

    temp_root = tempfile.mkdtemp(prefix="woool-smoke-")
    try:
        for name in COPY_NAMES:
            src = os.path.join(ROOT, name)
            dst = os.path.join(temp_root, name)
            if os.path.isdir(src):
                shutil.copytree(src, dst)
            elif os.path.isfile(src):
                shutil.copy2(src, dst)
        print("回归在隔离副本中运行：%s" % temp_root)
        return subprocess.call([sys.executable, os.path.join(temp_root, "tools", "smoke_test.py")],
                               cwd=temp_root)
    finally:
        shutil.rmtree(temp_root, ignore_errors=True)


if __name__ == "__main__":
    raise SystemExit(main())
