#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""文档链接检查：校验 Markdown 中的相对链接与图片是否存在。

扫描范围：README.md、CHANGELOG.md、NOTICE.md、docs/**/*.md
跳过：http(s)/mailto 链接、纯锚点、代码块与行内代码中的内容。

用法：
    python3 tools/check_links.py
    python3 tools/check_links.py --verbose   # 打印每个被检查的链接

退出码：0 = 全部有效；1 = 存在失效链接
"""
import argparse
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

TOP_FILES = ["README.md", "CHANGELOG.md", "NOTICE.md"]
DOCS_DIR = os.path.join(ROOT, "docs")

LINK_RE = re.compile(r"\[[^\]]*\]\(\s*<?([^)\s>]+)>?(?:\s+\"[^\"]*\")?\s*\)")
FENCE_RE = re.compile(r"^\s*(```|~~~)")
INLINE_CODE_RE = re.compile(r"`[^`\n]*`")
COMMENT_RE = re.compile(r"<!--.*?-->", re.S)


def collect_documents():
    files = []
    for name in TOP_FILES:
        path = os.path.join(ROOT, name)
        if os.path.isfile(path):
            files.append(path)
    if os.path.isdir(DOCS_DIR):
        for dirpath, _dirnames, filenames in os.walk(DOCS_DIR):
            for name in sorted(filenames):
                if name.endswith(".md"):
                    files.append(os.path.join(dirpath, name))
    return files


def strip_noise(text):
    """移除代码块、行内代码与 HTML 注释，避免把代码里的示例链接当文档链接。"""
    lines = text.splitlines()
    out = []
    in_fence = False
    for line in lines:
        if FENCE_RE.match(line):
            in_fence = not in_fence
            out.append("")
            continue
        out.append("" if in_fence else line)
    text = "\n".join(out)
    text = COMMENT_RE.sub("", text)
    return INLINE_CODE_RE.sub("", text)


def is_external(target):
    lowered = target.lower()
    return (lowered.startswith(("http://", "https://", "mailto:", "tel:", "ftp://"))
            or target.startswith("#"))


def check_file(path, verbose):
    rel_file = os.path.relpath(path, ROOT)
    with open(path, encoding="utf-8") as fh:
        text = strip_noise(fh.read())

    broken = []
    checked = 0
    for match in LINK_RE.finditer(text):
        target = match.group(1).strip()
        if not target or is_external(target):
            continue

        # 去掉锚点与查询串；GitHub 对目录链接也接受
        clean = target.split("#", 1)[0].split("?", 1)[0].strip()
        if not clean:
            continue

        base = os.path.dirname(path)
        resolved = os.path.normpath(os.path.join(base, clean))
        checked += 1
        if os.path.isdir(resolved):
            continue
        if not os.path.isfile(resolved):
            broken.append((target, os.path.relpath(resolved, ROOT)))
            continue
        if verbose:
            print("  ok  %s -> %s" % (rel_file, target))
    return rel_file, checked, broken


def main():
    parser = argparse.ArgumentParser(description="Markdown 相对链接检查")
    parser.add_argument("--verbose", action="store_true", help="打印每个有效链接")
    args = parser.parse_args()

    documents = collect_documents()
    if not documents:
        print("没有找到可检查的 Markdown 文档", file=sys.stderr)
        return 1

    total_checked = 0
    total_broken = 0
    for path in documents:
        rel_file, checked, broken = check_file(path, args.verbose)
        total_checked += checked
        total_broken += len(broken)
        for target, resolved in broken:
            print("[BROKEN] %s: %s -> %s（目标不存在）" % (rel_file, target, resolved))

    print("")
    print("链接检查完成：%d 个文档，%d 个相对链接，%d 个失效"
          % (len(documents), total_checked, total_broken))
    return 1 if total_broken else 0


if __name__ == "__main__":
    raise SystemExit(main())
