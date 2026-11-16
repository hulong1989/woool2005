#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""数据表校验器 —— 字段数 / 数值范围 / 交叉引用 / 地图文件完整性。

用法:
    python3 tools/validate_data.py                # 校验全部
    python3 tools/validate_data.py --strict       # warning 也视为失败
    python3 tools/validate_data.py --only items,skills
    python3 tools/validate_data.py --quiet        # 只打印错误与汇总

退出码:
    0  通过（可能有 warning）
    1  存在 error（或 --strict 下的 warning）

设计原则:
    · 与 C++ 加载器保持同样的解析规则（UTF-8、'/' 分列、'#' 注释、跳过空行）
    · 只报告"确定有问题"的内容为 error；可能在运行期才暴露的隐患报 warning
    · 不修改任何文件，纯只读
"""
import argparse
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA = os.path.join(ROOT, "data")
MAPS_LOGIC = os.path.join(DATA, "maps", "logic")
MAPS_PHYS = os.path.join(DATA, "maps", "physics")

ERRORS = []
WARNINGS = []
CHECKS = 0


# ---------------------------------------------------------------------------
# 基础设施
# ---------------------------------------------------------------------------
def error(where, msg):
    ERRORS.append("%s: %s" % (where, msg))


def warning(where, msg):
    WARNINGS.append("%s: %s" % (where, msg))


def check(cond, where, msg):
    global CHECKS
    CHECKS += 1
    if not cond:
        error(where, msg)
    return cond


def read_rows(path):
    """返回 [(行号, 原始行, 字段列表)]，跳过空行与 '#' 注释。"""
    rows = []
    with open(path, encoding="utf-8-sig", errors="replace") as fh:
        for lineno, raw in enumerate(fh, 1):
            line = raw.rstrip("\r\n")
            stripped = line.strip()
            if not stripped or stripped.startswith("#"):
                continue
            rows.append((lineno, line, [p.strip() for p in line.split("/")]))
    return rows


def as_int(value, where, name, lo=None, hi=None):
    """解析整数；失败记录 error 并返回 None。"""
    if value is None or value == "":
        error(where, "字段 [%s] 为空，应为整数" % name)
        return None
    try:
        number = int(value, 10)
    except ValueError:
        error(where, "字段 [%s]=%r 不是整数" % (name, value))
        return None
    if lo is not None and number < lo:
        error(where, "字段 [%s]=%d 小于下限 %d" % (name, number, lo))
    if hi is not None and number > hi:
        error(where, "字段 [%s]=%d 大于上限 %d" % (name, number, hi))
    return number


def as_name(value, where, name, max_bytes):
    """名字字段：非空 + 不超过底层 char[] 容量（字节数，UTF-8）。"""
    if value is None or value == "":
        error(where, "字段 [%s] 为空" % name)
        return value
    size = len(value.encode("utf-8"))
    if size > max_bytes:
        warning(where, "字段 [%s]=%r 为 %d 字节，超过 %d 字节缓冲（会被截断）"
                % (name, value, size, max_bytes))
    return value


def require_file(path):
    check(os.path.isfile(path), os.path.relpath(path, ROOT), "文件不存在")
    return path


# ---------------------------------------------------------------------------
# 各表校验
# ---------------------------------------------------------------------------
def load_clean_names(path, col=0):
    """读取名字集合（用于交叉引用），同时去重检查。"""
    names = set()
    for lineno, raw, fields in read_rows(path):
        if len(fields) > col and fields[col]:
            name = fields[col]
            if name in names:
                warning("%s:%d" % (os.path.relpath(path, ROOT), lineno),
                        "重复名字 %r" % name)
            names.add(name)
    return names


def validate_items():
    path = require_file(os.path.join(DATA, "baseitem.txt"))
    names = set()
    for lineno, raw, f in read_rows(path):
        where = "%s:%d" % (os.path.relpath(path, ROOT), lineno)
        check(len(f) in (20, 22), where, "字段数应为 20 或 22，实际 %d" % len(f))
        if len(f) < 20:
            continue
        name = f[0]
        as_name(name, where, "名字（ITEMBASE.szName[16]）", 15)
        if name in names:
            warning(where, "重复物品名 %r（按名查找只会命中第一条）" % name)
        names.add(name)
        as_int(f[1], where, "stdmode", 0, 255)
        as_int(f[2], where, "shape", 0, 255)
        as_int(f[3], where, "image", 0, 65535)
        as_int(f[4], where, "specialpower", 0, 255)
        for idx, label in ((5, "ac1"), (6, "ac2"), (7, "mac1"), (8, "mac2"),
                           (9, "dc1"), (10, "dc2"), (11, "mc1"), (12, "mc2"),
                           (13, "sc1"), (14, "sc2")):
            as_int(f[idx], where, label, 0, 255)
        as_int(f[15], where, "weight", 0, 255)
        as_int(f[16], where, "maxdura", 0, 65535)
        as_int(f[17], where, "price")
        as_int(f[18], where, "needtype")
        as_int(f[19], where, "needlevel")
        if len(f) >= 22:
            as_int(f[20], where, "useType", 0, 3)
            as_int(f[21], where, "useValue", 0, 65535)
    return names


def validate_monsters(item_names):
    path = require_file(os.path.join(DATA, "monsters.txt"))
    names = set()
    for lineno, raw, f in read_rows(path):
        where = "%s:%d" % (os.path.relpath(path, ROOT), lineno)
        check(len(f) == 10, where, "字段数应为 10，实际 %d" % len(f))
        if len(f) < 10:
            continue
        name = as_name(f[0], where, "名字", 31)
        if name in names:
            warning(where, "重复怪物名 %r" % name)
        names.add(name)
        as_int(f[1], where, "最大HP", 1)
        min_atk = as_int(f[2], where, "最小攻击", 0)
        max_atk = as_int(f[3], where, "最大攻击", 0)
        if min_atk is not None and max_atk is not None and min_atk > max_atk:
            error(where, "最小攻击 %d 大于最大攻击 %d" % (min_atk, max_atk))
        as_int(f[4], where, "防御", 0)
        as_int(f[5], where, "经验", 0)
        if f[6] and f[6] != "无":
            as_name(f[6], where, "掉落物品", 31)
            check(f[6] in item_names, where, "掉落物品 %r 不在 baseitem.txt" % f[6])
        as_int(f[7], where, "掉率", 0, 100)
        as_int(f[8], where, "等级", 1)
        as_int(f[9], where, "外形", 0)
    return names


def validate_spawns(map_ids, monster_names):
    path = require_file(os.path.join(DATA, "spawns.txt"))
    for lineno, raw, f in read_rows(path):
        where = "%s:%d" % (os.path.relpath(path, ROOT), lineno)
        check(len(f) == 5, where, "字段数应为 5，实际 %d" % len(f))
        if len(f) < 5:
            continue
        map_id = as_int(f[0], where, "地图id", 1)
        if map_id is not None:
            check(map_id in map_ids, where, "地图 %d 不存在（data/maps/logic/%d.ini）"
                  % (map_id, map_id))
        as_int(f[1], where, "x", 0)
        as_int(f[2], where, "y", 0)
        as_int(f[3], where, "数量", 1)
        check(f[4] in monster_names, where, "怪物 %r 不在 monsters.txt" % f[4])


def validate_drops(monster_names, item_names):
    path = require_file(os.path.join(DATA, "drops.txt"))
    for lineno, raw, f in read_rows(path):
        where = "%s:%d" % (os.path.relpath(path, ROOT), lineno)
        check(len(f) == 3, where, "字段数应为 3，实际 %d" % len(f))
        if len(f) < 3:
            continue
        check(f[0] in monster_names, where, "怪物 %r 不在 monsters.txt" % f[0])
        check(f[1] in item_names, where, "物品 %r 不在 baseitem.txt" % f[1])
        as_int(f[2], where, "概率", 0, 100)


def validate_classes(skill_classes=None):
    path = require_file(os.path.join(DATA, "classes.txt"))
    ids = set()
    for lineno, raw, f in read_rows(path):
        where = "%s:%d" % (os.path.relpath(path, ROOT), lineno)
        check(len(f) == 11, where, "字段数应为 11，实际 %d" % len(f))
        if len(f) < 11:
            continue
        class_id = as_int(f[0], where, "职业id", 0, 2)
        if class_id is not None:
            check(class_id not in ids, where, "重复职业id %d" % class_id)
            ids.add(class_id)
        as_name(f[1], where, "名称", 31)
        for idx, label in ((2, "HP成长"), (3, "MP成长"), (4, "最小攻击"), (5, "最大攻击"),
                           (6, "最小防御"), (7, "最大防御"), (8, "魔法成长"),
                           (9, "道术成长"), (10, "负重成长")):
            as_int(f[idx], where, label, 0)
        if f[4] and f[5]:
            try:
                if int(f[4]) > int(f[5]):
                    error(where, "最小攻击大于最大攻击")
            except ValueError:
                pass
    for expected in (0, 1, 2):
        check(expected in ids, os.path.relpath(path, ROOT), "缺少职业 id=%d" % expected)


def validate_levels():
    path = require_file(os.path.join(DATA, "levels.txt"))
    levels = []
    for lineno, raw, f in read_rows(path):
        where = "%s:%d" % (os.path.relpath(path, ROOT), lineno)
        check(len(f) == 2, where, "字段数应为 2，实际 %d" % len(f))
        if len(f) < 2:
            continue
        level = as_int(f[0], where, "等级", 1)
        exp = as_int(f[1], where, "升级经验", 1)
        if level is not None:
            levels.append(level)
        if level is not None and exp is not None and exp <= 0:
            error(where, "升级经验必须 > 0")
    for index, level in enumerate(levels, 1):
        check(level == index, os.path.relpath(path, ROOT),
              "等级必须从 1 连续递增，第 %d 条是 %d" % (index, level))
    check(len(levels) >= 2, os.path.relpath(path, ROOT), "等级表过短")


def validate_skills():
    path = require_file(os.path.join(DATA, "skills.txt"))
    ids = set()
    for lineno, raw, f in read_rows(path):
        where = "%s:%d" % (os.path.relpath(path, ROOT), lineno)
        check(len(f) == 9, where, "字段数应为 9，实际 %d" % len(f))
        if len(f) < 9:
            continue
        skill_id = as_int(f[0], where, "技能id", 1)
        if skill_id is not None:
            check(skill_id not in ids, where, "重复技能id %d" % skill_id)
            ids.add(skill_id)
            if skill_id > 35:
                warning(where, "技能id %d 大于 35（当前已学技能位图只有 32 位，"
                               "33 以上会与低位技能冲突）" % skill_id)
        as_name(f[1], where, "名称", 31)
        as_int(f[2], where, "职业", 0, 2)
        as_int(f[3], where, "需要等级", 1)
        as_int(f[4], where, "消耗MP", 0)
        as_int(f[5], where, "冷却ms", 0)
        as_int(f[6], where, "威力", 0)
        as_int(f[7], where, "类型", 1, 9)
        as_int(f[8], where, "射程", 0)


def validate_shop(item_names):
    path = require_file(os.path.join(DATA, "shop.txt"))
    for lineno, raw, f in read_rows(path):
        where = "%s:%d" % (os.path.relpath(path, ROOT), lineno)
        check(len(f) == 2, where, "字段数应为 2，实际 %d" % len(f))
        if len(f) < 2:
            continue
        check(f[0] in item_names, where, "物品 %r 不在 baseitem.txt" % f[0])
        as_int(f[1], where, "售价", 1)


def validate_npcs(map_ids):
    path = require_file(os.path.join(DATA, "npcs.txt"))
    names = set()
    for lineno, raw, f in read_rows(path):
        where = "%s:%d" % (os.path.relpath(path, ROOT), lineno)
        check(len(f) >= 9, where, "字段数应至少 9，实际 %d" % len(f))
        if len(f) < 9:
            continue
        if len(f) > 9:
            warning(where, "字段数 %d > 9，对话文本含 '/' 会被截断" % len(f))
        name = as_name(f[0], where, "名字", 31)
        if name in names:
            warning(where, "重复 NPC 名 %r（对话/商店按名字索引）" % name)
        names.add(name)
        npc_map = as_int(f[1], where, "地图id", 1)
        if npc_map is not None:
            check(npc_map in map_ids, where, "地图 %d 不存在" % npc_map)
        as_int(f[2], where, "x", 0)
        as_int(f[3], where, "y", 0)
        npc_type = as_int(f[4], where, "类型", 1, 3)
        dest_map = as_int(f[5], where, "目标地图", 0)
        as_int(f[6], where, "目标x", 0)
        as_int(f[7], where, "目标y", 0)
        if f[8] == "":
            warning(where, "对话文本为空")
        if npc_type in (2, 3) and dest_map not in (None, 0):
            check(dest_map in map_ids, where, "目标地图 %d 不存在" % dest_map)
    return names


def validate_quests(npc_names, item_names, monster_names):
    path = require_file(os.path.join(DATA, "quests.txt"))
    ids = set()
    for lineno, raw, f in read_rows(path):
        where = "%s:%d" % (os.path.relpath(path, ROOT), lineno)
        check(len(f) >= 12, where, "字段数应至少 12，实际 %d" % len(f))
        if len(f) < 12:
            continue
        quest_id = as_int(f[0], where, "任务id", 1)
        if quest_id is not None:
            check(quest_id not in ids, where, "重复任务id %d" % quest_id)
            ids.add(quest_id)
        as_name(f[1], where, "名称", 31)
        check(f[2] in npc_names, where, "发布 NPC %r 不在 npcs.txt" % f[2])
        quest_type = as_int(f[3], where, "类型", 1, 2)
        target = f[4]
        if quest_type == 1:
            check(target in monster_names, where, "杀怪目标 %r 不在 monsters.txt" % target)
        elif quest_type == 2:
            check(target in item_names, where, "收集目标 %r 不在 baseitem.txt" % target)
        as_int(f[5], where, "数量", 1)
        as_int(f[6], where, "经验", 0)
        as_int(f[7], where, "金币", 0)
        if f[8]:
            as_name(f[8], where, "奖励物品", 15)
            check(f[8] in item_names, where, "奖励物品 %r 不在 baseitem.txt" % f[8])
        as_int(f[9], where, "奖励数量", 1)
        as_int(f[10], where, "需求等级", 1)
        if f[11] == "":
            warning(where, "对话文本为空")
        if len(f) >= 13 and f[12]:
            prev_id = as_int(f[12], where, "前置任务id", 0)
            if prev_id is not None and prev_id != 0:
                check(prev_id in ids, where, "前置任务 %d 不存在" % prev_id)
        if len(f) >= 14 and f[13]:
            as_int(f[13], where, "可重复", 0, 1)


def validate_bosses(map_ids, monster_names):
    path = require_file(os.path.join(DATA, "bosses.txt"))
    for lineno, raw, f in read_rows(path):
        where = "%s:%d" % (os.path.relpath(path, ROOT), lineno)
        check(len(f) == 5, where, "字段数应为 5，实际 %d" % len(f))
        if len(f) < 5:
            continue
        check(f[0] in monster_names, where, "Boss 怪 %r 不在 monsters.txt" % f[0])
        map_id = as_int(f[1], where, "地图id", 1)
        if map_id is not None:
            check(map_id in map_ids, where, "地图 %d 不存在" % map_id)
        as_int(f[2], where, "x", 0)
        as_int(f[3], where, "y", 0)
        as_int(f[4], where, "刷新秒数", 1)


def validate_dungeons(map_ids, monster_names, achievement_ids, title_ids):
    path = require_file(os.path.join(DATA, "dungeons.txt"))
    ids = set()
    for lineno, raw, f in read_rows(path):
        where = "%s:%d" % (os.path.relpath(path, ROOT), lineno)
        check(len(f) == 6, where, "字段数应为 6（id/名称/Boss/地图列表/成就id/称号id），"
                                 "实际 %d" % len(f))
        if len(f) < 6:
            continue
        dungeon_id = as_int(f[0], where, "副本id", 1)
        if dungeon_id is not None:
            check(dungeon_id not in ids, where, "重复副本id %d" % dungeon_id)
            ids.add(dungeon_id)
        as_name(f[1], where, "名称", 31)
        check(f[2] in monster_names, where, "Boss %r 不在 monsters.txt" % f[2])
        maps = [m.strip() for m in f[3].split(",") if m.strip()]
        check(len(maps) > 0, where, "实例地图列表为空")
        for m in maps:
            try:
                map_id = int(m, 10)
            except ValueError:
                error(where, "实例地图 %r 不是整数" % m)
                continue
            check(map_id in map_ids, where, "实例地图 %d 不存在" % map_id)
        ach_id = as_int(f[4], where, "成就id", 1)
        title_id = as_int(f[5], where, "称号id", 1)
        if ach_id is not None:
            check(ach_id in achievement_ids, where, "成就id %d 不存在" % ach_id)
        if title_id is not None:
            check(title_id in title_ids, where, "称号id %d 不存在" % title_id)


def validate_achievements(monster_names):
    path = require_file(os.path.join(DATA, "achievements.txt"))
    ids = set()
    known_types = {"level", "boss", "dungeon", "upgrade", "enchant", "quest", "codex"}
    for lineno, raw, f in read_rows(path):
        where = "%s:%d" % (os.path.relpath(path, ROOT), lineno)
        check(len(f) >= 5, where, "字段数应至少 5，实际 %d" % len(f))
        if len(f) < 5:
            continue
        ach_id = as_int(f[0], where, "成就id", 1)
        if ach_id is not None:
            check(ach_id not in ids, where, "重复成就id %d" % ach_id)
            ids.add(ach_id)
        as_name(f[1], where, "名称", 31)
        ach_type = f[3]
        if ach_type not in known_types:
            error(where, "未知成就类型 %r（应为 %s）"
                  % (ach_type, "/".join(sorted(known_types))))
        if ach_type == "boss":
            check(f[4] in monster_names, where, "Boss 值 %r 不在 monsters.txt" % f[4])
        else:
            as_int(f[4], where, "值", 0)
        if len(f) >= 6:
            as_int(f[5], where, "点数", 0)
        if len(f) >= 7 and f[6]:
            validate_bonus_list(f[6], where, "称号加成")
    return ids


def validate_titles():
    path = require_file(os.path.join(DATA, "titles.txt"))
    ids = set()
    for lineno, raw, f in read_rows(path):
        where = "%s:%d" % (os.path.relpath(path, ROOT), lineno)
        check(len(f) >= 3, where, "字段数应至少 3，实际 %d" % len(f))
        if len(f) < 3:
            continue
        title_id = as_int(f[0], where, "称号id", 1)
        if title_id is not None:
            check(title_id not in ids, where, "重复称号id %d" % title_id)
            ids.add(title_id)
        as_name(f[1], where, "名称", 31)
        as_int(f[2], where, "价格")
        if len(f) >= 4 and f[3]:
            validate_bonus_list(f[3], where, "称号加成")
    return ids


BONUS_KEYS = {"HP", "MP", "AC", "MAC", "DC", "MC", "SC"}


def validate_bonus_list(text, where, name):
    for part in text.split(","):
        part = part.strip()
        if not part:
            continue
        match = re.match(r"^([A-Z]+):(-?\d+)$", part)
        if not match:
            error(where, "%s 片段 %r 格式应为 KEY:数字" % (name, part))
            continue
        if match.group(1) not in BONUS_KEYS:
            error(where, "%s 未知属性 %r（支持 %s）"
                  % (name, match.group(1), "/".join(sorted(BONUS_KEYS))))


def validate_sets(item_names):
    path = require_file(os.path.join(DATA, "sets.txt"))
    names = set()
    for lineno, raw, f in read_rows(path):
        where = "%s:%d" % (os.path.relpath(path, ROOT), lineno)
        check(len(f) == 4, where, "字段数应为 4，实际 %d" % len(f))
        if len(f) < 4:
            continue
        name = as_name(f[0], where, "套装名", 31)
        if name in names:
            warning(where, "重复套装名 %r" % name)
        names.add(name)
        members = [m.strip() for m in f[1].split(",") if m.strip()]
        check(len(members) > 0, where, "套装成员为空")
        for member in members:
            check(member in item_names, where, "成员 %r 不在 baseitem.txt" % member)
        need = as_int(f[2], where, "触发件数", 1)
        if need is not None and members and need > len(members):
            error(where, "触发件数 %d 大于成员数 %d" % (need, len(members)))
        for part in f[3].split(","):
            part = part.strip()
            if part:
                validate_bonus_list(part, where, "套装属性")


def validate_dialogues(npc_names, map_ids):
    path = require_file(os.path.join(DATA, "dialogues.txt"))
    seen = set()
    for lineno, raw, f in read_rows(path):
        where = "%s:%d" % (os.path.relpath(path, ROOT), lineno)
        check(len(f) == 5, where, "字段数应为 5（NPC/序号/文本/动作/参数），实际 %d"
              % len(f))
        if len(f) < 5:
            continue
        check(f[0] in npc_names, where, "NPC %r 不在 npcs.txt" % f[0])
        index = as_int(f[1], where, "序号", 1)
        key = (f[0], index)
        if index is not None:
            check(key not in seen, where, "NPC %r 的选项序号 %d 重复" % (f[0], index))
            seen.add(key)
        as_name(f[2], where, "选项文本", 63)
        action = f[3]
        check(action in ("shop", "quests", "teleport", "close"), where,
              "未知动作 %r（应为 shop/quests/teleport/close）" % action)
        if action == "teleport":
            parts = [p.strip() for p in f[4].split(",")]
            if len(parts) != 3:
                error(where, "teleport 参数应为 地图id,x,y，实际 %r" % f[4])
            else:
                try:
                    map_id = int(parts[0], 10)
                    check(map_id in map_ids, where, "teleport 地图 %d 不存在" % map_id)
                except ValueError:
                    error(where, "teleport 地图id %r 不是整数" % parts[0])


def validate_events(map_ids, monster_names):
    path = require_file(os.path.join(DATA, "events.txt"))
    for lineno, raw, f in read_rows(path):
        where = "%s:%d" % (os.path.relpath(path, ROOT), lineno)
        kind = f[0]
        if kind == "doubleexp":
            check(len(f) == 4, where,
                  "doubleexp 字段数应为 4（类型/名称/HH:MM/分钟），实际 %d" % len(f))
            if len(f) < 4:
                continue
            as_name(f[1], where, "名称", 31)
            validate_hhmm(f[2], where)
            as_int(f[3], where, "持续分钟", 1)
        elif kind == "worldboss":
            check(len(f) == 8, where,
                  "worldboss 字段数应为 8（类型/名称/HH:MM/分钟/地图/x/y/怪物），"
                  "实际 %d" % len(f))
            if len(f) < 8:
                continue
            as_name(f[1], where, "名称", 31)
            validate_hhmm(f[2], where)
            as_int(f[3], where, "持续分钟", 1)
            map_id = as_int(f[4], where, "地图id", 1)
            if map_id is not None:
                check(map_id in map_ids, where, "地图 %d 不存在" % map_id)
            as_int(f[5], where, "x", 0)
            as_int(f[6], where, "y", 0)
            check(f[7] in monster_names, where, "怪物 %r 不在 monsters.txt" % f[7])
        else:
            error(where, "未知活动类型 %r（应为 doubleexp/worldboss）" % kind)


def validate_hhmm(text, where):
    match = re.match(r"^(\d{1,2}):(\d{2})$", text)
    if not match:
        error(where, "时间 %r 格式应为 HH:MM" % text)
        return
    hour, minute = int(match.group(1)), int(match.group(2))
    if not (0 <= hour <= 23):
        error(where, "小时 %d 超出 0-23" % hour)
    if not (0 <= minute <= 59):
        error(where, "分钟 %d 超出 0-59" % minute)


# ---------------------------------------------------------------------------
# 地图
# ---------------------------------------------------------------------------
def parse_ini_sections(path):
    sections = {}
    current = None
    with open(path, encoding="utf-8-sig", errors="replace") as fh:
        for lineno, raw in enumerate(fh, 1):
            line = raw.strip()
            if not line or line.startswith(";") or line.startswith("#"):
                continue
            if line.startswith("["):
                end = line.find("]")
                if end > 0:
                    current = line[1:end].strip().lower()
                    sections.setdefault(current, {})
                continue
            if "=" not in line:
                continue
            key, value = line.split("=", 1)
            sections.setdefault(current or "", {})[key.strip().lower()] = value.strip()
    return sections


def validate_physics_maps():
    """返回 {名字: (宽, 高)}。"""
    names = {}
    if not os.path.isdir(MAPS_PHYS):
        error(os.path.relpath(MAPS_PHYS, ROOT), "目录不存在")
        return names
    for filename in sorted(os.listdir(MAPS_PHYS)):
        if not filename.endswith(".nmp"):
            continue
        path = os.path.join(MAPS_PHYS, filename)
        rel = os.path.relpath(path, ROOT)
        sections = parse_ini_sections(path)
        meta = sections.get("map", {})
        if not meta:
            error(rel, "缺少 [map] 段")
            continue
        name = meta.get("name", "")
        if not name:
            error(rel, "[map] name 为空")
        try:
            width = int(meta.get("width", "0"), 10)
            height = int(meta.get("height", "0"), 10)
        except ValueError:
            error(rel, "[map] width/height 必须是整数")
            continue
        if width <= 0 or height <= 0:
            error(rel, "[map] width/height 必须 > 0（实际 %dx%d）" % (width, height))
            continue
        if name:
            if name in names:
                warning(rel, "重复的物理地图名 %r" % name)
            names[name] = (width, height)
        rows = sections.get("block", {})
        for key, value in rows.items():
            try:
                row = int(key, 10)
            except ValueError:
                error(rel, "[block] 行号 %r 不是整数" % key)
                continue
            if not (0 <= row < height):
                error(rel, "[block] 行号 %d 超出 0-%d" % (row, height - 1))
                continue
            bits = re.sub(r"[\s,]", "", value)
            if len(bits) != width:
                error(rel, "[block] 第 %d 行长度 %d，应为 width=%d"
                      % (row, len(bits), width))
            elif re.search(r"[^01]", bits):
                error(rel, "[block] 第 %d 行含 0/1 以外的字符" % row)
    return names


def validate_logic_maps(physics_names):
    """返回存在的逻辑地图 id 集合。"""
    ids = set()
    if not os.path.isdir(MAPS_LOGIC):
        error(os.path.relpath(MAPS_LOGIC, ROOT), "目录不存在")
        return ids
    for filename in sorted(os.listdir(MAPS_LOGIC)):
        if not filename.endswith(".ini"):
            continue
        path = os.path.join(MAPS_LOGIC, filename)
        rel = os.path.relpath(path, ROOT)
        stem = filename[:-4]
        if not stem.isdigit():
            warning(rel, "文件名 %r 不是数字，加载器拿不到地图 id" % filename)
        else:
            map_id = int(stem, 10)
            if map_id in ids:
                error(rel, "重复地图 id %d" % map_id)
            ids.add(map_id)
        sections = parse_ini_sections(path)
        meta = sections.get("define", {})
        if not meta:
            error(rel, "缺少 [define] 段")
            continue
        for key in ("name", "blockmap"):
            if not meta.get(key):
                error(rel, "[define] 缺少 %s" % key)
        block = meta.get("blockmap")
        if block:
            check(block in physics_names, rel,
                  "引用的物理地图 %r 不在 data/maps/physics" % block)
        try:
            area_w = int(meta.get("areawidth", "0"), 10)
            area_h = int(meta.get("areaheight", "0"), 10)
        except ValueError:
            error(rel, "[define] areawidth/areaheight 必须是整数")
            area_w = area_h = 0
        if area_w <= 0 or area_h <= 0:
            error(rel, "[define] areawidth/areaheight 必须 > 0")
        if "type" not in meta:
            warning(rel, "[define] 缺少 type（加载器不会报错，但地图类型恒为 NOFIGHT）")
        else:
            try:
                int(meta["type"], 10)
            except ValueError:
                error(rel, "[define] type 必须是整数")
        if block in physics_names and area_w > 0 and area_h > 0:
            width, height = physics_names[block]
            if area_w > width or area_h > height:
                error(rel, "区域尺寸 %dx%d 大于物理地图 %s 的 %dx%d"
                      % (area_w, area_h, block, width, height))
    return ids


# ---------------------------------------------------------------------------
# 账号 / 角色
# ---------------------------------------------------------------------------
def validate_accounts():
    path = require_file(os.path.join(DATA, "accounts.txt"))
    accounts = set()
    for lineno, raw, f in read_rows(path):
        where = "%s:%d" % (os.path.relpath(path, ROOT), lineno)
        check(len(f) == 3, where, "字段数应为 3（账号/盐/哈希），实际 %d" % len(f))
        if len(f) < 3:
            continue
        check(f[0] not in accounts, where, "重复账号 %r" % f[0])
        accounts.add(f[0])
        check(re.match(r"^[0-9a-fA-F]+$", f[1]) is not None, where, "盐不是十六进制")
        check(re.match(r"^[0-9a-fA-F]{64}$", f[2]) is not None, where,
              "密码哈希应为 64 位十六进制 SHA-256")
    return accounts


def validate_characters(accounts):
    path = require_file(os.path.join(DATA, "characters.txt"))
    names = set()
    for lineno, raw, f in read_rows(path):
        where = "%s:%d" % (os.path.relpath(path, ROOT), lineno)
        check(len(f) == 3, where, "字段数应为 3（账号/角色/职业），实际 %d" % len(f))
        if len(f) < 3:
            continue
        check(f[0] in accounts, where, "账号 %r 不在 accounts.txt" % f[0])
        check(f[1] not in names, where, "重复角色名 %r" % f[1])
        names.add(f[1])
        as_name(f[1], where, "角色名", 31)
        as_int(f[2], where, "职业", 0, 2)
    return names


def validate_gm():
    path = os.path.join(DATA, "gm.txt")
    if not os.path.isfile(path):
        warning(os.path.relpath(path, ROOT), "文件不存在（GM 名单为空）")
        return
    for lineno, raw, f in read_rows(path):
        where = "%s:%d" % (os.path.relpath(path, ROOT), lineno)
        check(len(f) == 2, where, "字段数应为 2（角色名/等级），实际 %d" % len(f))
        if len(f) < 2:
            continue
        as_name(f[0], where, "角色名", 31)
        as_int(f[1], where, "GM 等级", 1, 3)


# ---------------------------------------------------------------------------
# 主流程
# ---------------------------------------------------------------------------
GROUPS = ["maps", "items", "monsters", "spawns", "drops", "classes", "levels",
          "skills", "shop", "npcs", "quests", "bosses", "dungeons",
          "achievements", "titles", "sets", "dialogues", "events", "accounts", "gm"]


def main():
    parser = argparse.ArgumentParser(description="woool2005 数据表校验器")
    parser.add_argument("--strict", action="store_true", help="warning 也视为失败")
    parser.add_argument("--only", default="", help="只校验指定分组，逗号分隔")
    parser.add_argument("--quiet", action="store_true", help="只打印错误与汇总")
    args = parser.parse_args()

    selected = [g.strip() for g in args.only.split(",") if g.strip()] or GROUPS
    unknown = [g for g in selected if g not in GROUPS]
    if unknown:
        print("未知校验分组: %s（可选: %s）" % (", ".join(unknown), ", ".join(GROUPS)))
        return 2

    def enabled(group):
        return group in selected

    if not os.path.isdir(DATA):
        print("错误: 找不到 data/ 目录（请在工程根目录运行）", file=sys.stderr)
        return 1

    # 依赖顺序：先基础表，再交叉引用
    map_ids = set()
    physics_names = {}
    if enabled("maps"):
        physics_names = validate_physics_maps()
        map_ids = validate_logic_maps(physics_names)

    item_names = validate_items() if enabled("items") else load_clean_names(
        os.path.join(DATA, "baseitem.txt"))
    monster_names = (validate_monsters(item_names) if enabled("monsters")
                     else load_clean_names(os.path.join(DATA, "monsters.txt")))

    if enabled("spawns"):
        validate_spawns(map_ids, monster_names)
    if enabled("drops"):
        validate_drops(monster_names, item_names)
    if enabled("classes"):
        validate_classes()
    if enabled("levels"):
        validate_levels()
    if enabled("skills"):
        validate_skills()
    if enabled("shop"):
        validate_shop(item_names)

    npc_names = (validate_npcs(map_ids) if enabled("npcs")
                 else load_clean_names(os.path.join(DATA, "npcs.txt")))

    if enabled("quests"):
        validate_quests(npc_names, item_names, monster_names)
    if enabled("bosses"):
        validate_bosses(map_ids, monster_names)

    achievement_ids = set()
    title_ids = set()
    if enabled("achievements"):
        achievement_ids = validate_achievements(monster_names)
    if enabled("titles"):
        title_ids = validate_titles()
    if enabled("dungeons"):
        validate_dungeons(map_ids, monster_names, achievement_ids, title_ids)
    if enabled("sets"):
        validate_sets(item_names)
    if enabled("dialogues"):
        validate_dialogues(npc_names, map_ids)
    if enabled("events"):
        validate_events(map_ids, monster_names)

    accounts = set()
    if enabled("accounts"):
        accounts = validate_accounts()
        validate_characters(accounts)
    if enabled("gm"):
        validate_gm()

    # 输出
    for message in ERRORS:
        print("[ERROR] %s" % message)
    if not args.quiet:
        for message in WARNINGS:
            print("[WARN ] %s" % message)
    print("")
    print("数据校验完成：%d 项检查，%d 个错误，%d 个警告"
          % (CHECKS, len(ERRORS), len(WARNINGS)))
    if ERRORS:
        return 1
    if args.strict and WARNINGS:
        print("--strict 模式下警告视为失败")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
