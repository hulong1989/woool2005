#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成可运行的 server 发布包（tar.gz）。

包内结构与仓库一致，解包后可直接运行回归：

    woool2005-<version>-<os>-<arch>/
      build/GameServer        # 发布构建（-O2）二进制
      data/                   # 数据表 + 地图（不含 data/players 个人存档）
      tools/                  # smoke_test.py / validate_data.py / check_links.py
      docs/                   # 架构 / 协议 / 数据格式文档
      gameserver.ini  README.md  CHANGELOG.md  NOTICE.md  RUN.txt

用法：
    python3 tools/package_release.py --version v0.1.1 --build
    python3 tools/package_release.py --version dev --out dist
    make package VERSION=v0.1.1

注意：产物是**平台相关**的（当前机器架构）。Windows 请使用 tools/build_windows.ps1。
"""
import argparse
import os
import platform
import shutil
import subprocess
import sys
import tarfile
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

COPY_FILES = ["gameserver.ini", "README.md", "CHANGELOG.md", "NOTICE.md"]
COPY_DIRS = ["data", "tools", "docs"]
EXCLUDE_NAMES = {".DS_Store", "players", "__pycache__", ".git", "*.pyc"}
RELEASE_CXXFLAGS = ("-std=c++11 -O2 -Wall -Wno-unused-parameter -Wno-reorder")


def platform_tag():
    system = platform.system().lower()
    system = {"darwin": "macos", "linux": "linux", "windows": "windows"}.get(system, system)
    machine = platform.machine().lower()
    machine = {"x86_64": "x86_64", "amd64": "x86_64", "arm64": "arm64",
               "aarch64": "arm64"}.get(machine, machine)
    return "%s-%s" % (system, machine)


def default_cxx():
    return os.environ.get("WOOOL_CXX") or ("clang++" if platform.system() == "Darwin" else "g++")


def build_release_binary():
    build_dir = "build-release"
    print("构建发布版二进制（-O2 -> %s/GameServer）…" % build_dir)
    cmd = ["make", "BUILD=" + build_dir, "CXX=" + default_cxx(),
           "CXXFLAGS=" + RELEASE_CXXFLAGS]
    result = subprocess.run(cmd, cwd=ROOT)
    if result.returncode != 0:
        print("构建失败：%s" % " ".join(cmd), file=sys.stderr)
        raise SystemExit(1)
    return os.path.join(ROOT, build_dir, "GameServer")


def copy_tree(src, dst):
    shutil.copytree(
        src, dst,
        ignore=shutil.ignore_patterns(*EXCLUDE_NAMES),
        symlinks=True,
    )


def write_run_txt(stage_dir, version):
    content = """woool2005 {version} —— 服务端运行包

平台：{platform}
构建：-O2

运行（在解包目录）：
    ./build/GameServer

说明：
    · 默认监听 0.0.0.0:7100，读取当前目录的 gameserver.ini 与 data/
    · 数据表与地图在 data/；data/players/ 为个人存档，未包含在包内，首次运行自动创建
    · 回归测试：python3 tools/smoke_test.py
    · 数据校验：python3 tools/validate_data.py
    · 桌面客户端请从源码仓库 client/ 运行（需要 pygame），详见 README.md
    · 本包为平台相关产物（{platform}），其它平台请在源码目录自行 make
""".format(version=version, platform=platform_tag())
    with open(os.path.join(stage_dir, "RUN.txt"), "w", encoding="utf-8") as fh:
        fh.write(content)


def human(size):
    for unit in ("B", "KB", "MB", "GB"):
        if size < 1024 or unit == "GB":
            return "%.1f %s" % (size, unit)
        size /= 1024.0


def main():
    parser = argparse.ArgumentParser(description="生成 server 发布包")
    parser.add_argument("--version", default="dev", help="版本号（用于文件名），默认 dev")
    parser.add_argument("--out", default="dist", help="输出目录，默认 dist/")
    parser.add_argument("--build", action="store_true",
                        help="先用 -O2 构建 build-release/GameServer")
    args = parser.parse_args()

    if args.build:
        binary = build_release_binary()
    else:
        binary = os.path.join(ROOT, "build", "GameServer")
        if not os.path.isfile(binary):
            print("缺少 %s，请先 make 或加 --build" % binary, file=sys.stderr)
            return 1

    package_name = "woool2005-%s-%s" % (args.version, platform_tag())
    out_dir = os.path.join(ROOT, args.out)
    os.makedirs(out_dir, exist_ok=True)
    tarball = os.path.join(out_dir, package_name + ".tar.gz")

    stage_root = tempfile.mkdtemp(prefix="woool-package-")
    try:
        stage_dir = os.path.join(stage_root, package_name)
        os.makedirs(os.path.join(stage_dir, "build"))
        shutil.copy2(binary, os.path.join(stage_dir, "build", "GameServer"))
        os.chmod(os.path.join(stage_dir, "build", "GameServer"), 0o755)

        for name in COPY_FILES:
            src = os.path.join(ROOT, name)
            if os.path.isfile(src):
                shutil.copy2(src, os.path.join(stage_dir, name))
        for name in COPY_DIRS:
            src = os.path.join(ROOT, name)
            if os.path.isdir(src):
                copy_tree(src, os.path.join(stage_dir, name))
        # data/players 是个人存档，不打进包
        players = os.path.join(stage_dir, "data", "players")
        if os.path.isdir(players):
            shutil.rmtree(players)

        write_run_txt(stage_dir, args.version)

        print("打包 -> %s" % os.path.relpath(tarball, ROOT))
        with tarfile.open(tarball, "w:gz", compresslevel=9) as tar:
            tar.add(stage_dir, arcname=package_name)
    finally:
        shutil.rmtree(stage_root, ignore_errors=True)

    print("完成：%s（%s）" % (os.path.relpath(tarball, ROOT),
                             human(os.path.getsize(tarball))))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
