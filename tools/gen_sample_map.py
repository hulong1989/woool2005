#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成示例物理地图（文本 .nmp 格式）。

用法:
    python3 tools/gen_sample_map.py
    python3 tools/gen_sample_map.py --width 512 --height 512 --output data/maps/physics/map0.nmp

说明:
    原版 .nmp 为二进制格式，快照中没有格式说明；本骨架使用等价的文本格式：
        [map] name/width/height
        [block] 行号=由 0/1 组成的行（缺省行视为全部可通行）
"""
import argparse
import os


def build_rows(width: int, height: int):
    rows = {}
    rows[0] = "1" * width
    rows[height - 1] = "1" * width
    # 出生点 (477,222) 附近放一段墙，用于验证阻挡
    for y in range(218, 227):
        row = ["0"] * width
        for x in range(465, 476):
            if 0 <= x < width:
                row[x] = "1"
        rows[y] = "".join(row)
    return rows


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--width", type=int, default=512)
    ap.add_argument("--height", type=int, default=512)
    ap.add_argument("--output", default=os.path.join("data", "maps", "physics", "map0.nmp"))
    args = ap.parse_args()

    rows = build_rows(args.width, args.height)
    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    with open(args.output, "w", encoding="utf-8", newline="\n") as fp:
        fp.write("[map]\nname=map0\nwidth=%d\nheight=%d\n[block]\n" % (args.width, args.height))
        for y in sorted(rows):
            fp.write("%d=%s\n" % (y, rows[y]))

    print("已生成 %s (%dx%d，%d 行阻挡)" % (args.output, args.width, args.height, len(rows)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
